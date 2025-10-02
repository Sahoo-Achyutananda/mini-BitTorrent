#if !defined(DOWNLOAD_H)
#define DOWNLOAD_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "./tracker/utils.h"
#include "client_constructs.h"
#include "sha.h"
#include "fileops.h"
#include "client.h"
#include "threadpool.h"

extern pair<string, int> clientInfo;

void* downloadServer(void* arg);
void* handlePeerRequest(void* arg);
bool servePieceToClient(int clientSocket, string fileName, int pieceIndex);

// Download worker functions
void* downloadWorker(void* arg);
bool downloadPieceFromPeer(string& fileName, int pieceIndex, string& expectedHash, PieceSeederInfo& peer);
void mergePiecesToFile(string& fileName);
void notifyTrackerPieceCompleted(string& groupId, string& fileName, int pieceIndex);
void notifyTrackerDownloadComplete(string& groupId, string& fileName);

// Initialize download server with thread pool
void initializeDownloadServer(int basePort){
    if(downloadServerRunning) return;
    
    downloadServerPort = basePort + 2000;
    downloadServerRunning = true;
    
    // Initialize thread pool for parallel downloads
    initializeThreadPool();
    
    pthread_create(&downloadServerThread, nullptr, downloadServer, nullptr);
    pthread_detach(downloadServerThread);
    
    cout << fontBold << colorGreen << "Download server started on port " 
         << downloadServerPort << reset << endl;
}

// Thread pool task execution (defined here to avoid circular dependency)
void ThreadPool::executeTask(DownloadTask* task) {
    downloadPieceFromPeer(task->fileName, task->pieceIndex, 
                          task->expectedHash, task->seeder);
}

// Download server implementation (unchanged)
void* downloadServer(void* arg){
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0){
        perror("Download server socket creation failed");
        return nullptr;
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(downloadServerPort);
    
    if(bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
        perror("Download server bind failed");
        close(serverSocket);
        return nullptr;
    }
    
    if(listen(serverSocket, 10) < 0){
        perror("Download server listen failed");
        close(serverSocket);
        return nullptr;
    }
    
    cout << fontBold << colorBlue << "Download server listening on port " 
         << downloadServerPort << reset << endl;
    
    while(downloadServerRunning){
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        
        if(clientSocket < 0){
            if(downloadServerRunning) perror("Download server accept failed");
            continue;
        }
        
        pthread_t handlerThread;
        int* socketPtr = new int(clientSocket);
        pthread_create(&handlerThread, nullptr, handlePeerRequest, socketPtr);
        pthread_detach(handlerThread);
    }
    
    close(serverSocket);
    return nullptr;
}

// handlePeerRequest and servePieceToClient remain the same
void* handlePeerRequest(void* arg){
    int clientSocket = *(int*)arg;
    delete (int*)arg;
    
    char buffer[1024];
    int n = read(clientSocket, buffer, sizeof(buffer) - 1);
    if(n <= 0){
        close(clientSocket);
        return nullptr;
    }
    
    buffer[n] = '\0';
    string request(buffer);
    
    vector<string> tokens;
    stringstream ss(request);
    string token;
    while(getline(ss, token, '|')){
        tokens.push_back(token);
    }
    
    if(tokens.size() != 3 || tokens[0] != "GET_PIECE"){
        string response = "ERROR|Invalid request format";
        write(clientSocket, response.c_str(), response.length());
        close(clientSocket);
        return nullptr;
    }
    
    string fileName = tokens[1];
    int pieceIndex = stoi(tokens[2]);
    
    servePieceToClient(clientSocket, fileName, pieceIndex);
    
    close(clientSocket);
    return nullptr;
}

bool servePieceToClient(int clientSocket, string fileName, int pieceIndex){
    pthread_mutex_lock(&seed_mutex);
    
    auto it = seedingFiles.find(fileName);
    if(it == seedingFiles.end()){
        pthread_mutex_unlock(&seed_mutex);
        string response = "ERROR|File not found";
        write(clientSocket, response.c_str(), response.length());
        return false;
    }
    
    SeedInfo* seedInfo = it->second;
    if(pieceIndex >= (int)seedInfo->pieces.size()){
        pthread_mutex_unlock(&seed_mutex);
        string response = "ERROR|Invalid piece index";
        write(clientSocket, response.c_str(), response.length());
        return false;
    }
    
    ifstream file(seedInfo->filePath, ios::binary);
    if(!file.is_open()){
        pthread_mutex_unlock(&seed_mutex);
        string response = "ERROR|Cannot read file";
        write(clientSocket, response.c_str(), response.length());
        return false;
    }
    
    const int PIECE_SIZE = 512 * 1024;
    file.seekg(pieceIndex * PIECE_SIZE);
    
    char* pieceData = new char[PIECE_SIZE];
    file.read(pieceData, PIECE_SIZE);
    int bytesRead = file.gcount();
    file.close();
    
    pthread_mutex_unlock(&seed_mutex);
    
    string response = "PIECE_DATA|" + to_string(bytesRead) + "|\n";
    if(writeAll(clientSocket, response.c_str(), response.size()) < 0){
        delete[] pieceData;
        return false;
    }
    if(writeAll(clientSocket, pieceData, bytesRead) < 0){
        delete[] pieceData;
        return false;
    }
    
    delete[] pieceData;
    return true;
}

void handleFileMetadata(string response){
    vector<string> parts;
    stringstream ss(response);
    string item;
    
    while(getline(ss, item, '|')){
        parts.push_back(item);
    }
    
    if(parts.size() < 7 || parts[0] != "FILE_META"){
        cout << colorRed << "Invalid file metadata format" << reset << endl;
        return;
    }
    
    string fileName = parts[1];
    long long fileSize = stoll(parts[2]);
    string fullFileSHA1 = parts[3];
    int totalPieces = stoi(parts[4]);
    string groupId = parts[5];
    string destPath = parts[6];
    
    pthread_mutex_lock(&download_mutex);
    
    DownloadInfo* download = new DownloadInfo();
    download->fileName = fileName;
    download->fileSize = fileSize;
    download->fullFileSHA1 = fullFileSHA1;
    download->totalPieces = totalPieces;
    download->groupId = groupId;
    download->destPath = destPath;
    download->downloadedPieces.resize(totalPieces, false);
    
    for(int i = 7; i < parts.size(); i++){
        string pieceInfo = parts[i];
        
        size_t firstColon = pieceInfo.find(':');
        size_t secondColon = pieceInfo.find(':', firstColon + 1);
        
        if(firstColon == string::npos || secondColon == string::npos) continue;
        
        int pieceIndex = stoi(pieceInfo.substr(0, firstColon));
        string pieceHash = pieceInfo.substr(firstColon + 1, secondColon - firstColon - 1);
        string seedersStr = pieceInfo.substr(secondColon + 1);
        
        DownloadPieceInfo piece(pieceIndex, pieceHash);
        
        if(!seedersStr.empty()){
            stringstream seederStream(seedersStr);
            string seederInfo;
            while(getline(seederStream, seederInfo, ';')){
                size_t firstColon = seederInfo.find(':');
                size_t secondColon = seederInfo.find(':', firstColon + 1);
                
                if(firstColon != string::npos && secondColon != string::npos){
                    string userId = seederInfo.substr(0, firstColon);
                    string ip = seederInfo.substr(firstColon + 1, secondColon - firstColon - 1);
                    int port = stoi(seederInfo.substr(secondColon + 1));
                    piece.addSeeder(userId, ip, port);
                }
            }
        }
        
        download->pieces.push_back(piece);
    }
    
    activeDownloads[fileName] = download;
    pthread_mutex_unlock(&download_mutex);
    
    cout << fontBold << colorBlue << "Started download: " << fileName << " (" << fileSize << " bytes, " << totalPieces << " pieces)" << reset << endl;
    
    // Start download coordinator thread
    pthread_t workerThread;
    pthread_create(&workerThread, nullptr, downloadWorker, new string(fileName));
    pthread_detach(workerThread);
}

// this was the only fucntion updated to handle threead_pool
// PARALLEL DOWNLOAD WORKER - submits tasks to thread pool
void* downloadWorker(void* arg){
    string fileName = *(string*)arg;
    delete (string*)arg;
    
    pthread_mutex_lock(&download_mutex);
    auto it = activeDownloads.find(fileName);
    if(it == activeDownloads.end()){
        pthread_mutex_unlock(&download_mutex);
        return nullptr;
    }
    
    DownloadInfo* download = it->second;
    int totalPieces = download->totalPieces;
    pthread_mutex_unlock(&download_mutex);
    
    cout << fontBold << colorYellow << "Starting PARALLEL download for " << fileName << " using thread pool" << reset << endl;
    
    // Submit ALL pieces to thread pool for parallel download
    for(auto& piece : download->pieces){
        if(download->downloadedPieces[piece.pieceIndex]) continue;
        
        // Submit task for each seeder (thread pool will handle them in parallel) -only tries first seeder - im breaking out after the first seeder try
        for(auto& seeder : piece.seeders){
            DownloadTask* task = new DownloadTask(
                fileName,
                piece.pieceIndex,
                piece.sha1Hash,
                seeder
            );
            globalDownloadPool->addTask(task);
            break; // Try first seeder, retry logic below
        }
    }
    
    // Monitor progress
    // int lastReported = 0;
    while(true){ // try ->retry ->try ->retry and die
        sleep(2);
        
        pthread_mutex_lock(&download_mutex);
        int completed = 0;
        for(bool downloaded : download->downloadedPieces){
            if(downloaded) completed++;
        }
        pthread_mutex_unlock(&download_mutex);
        
        // // Show progress every 10%
        // int progressPercent = (completed * 100) / totalPieces;
        // if(progressPercent >= lastReported + 10){
        //     cout << fontBold << colorBlue << fileName << ": " 
        //          << progressPercent << "% complete (" << completed 
        //          << "/" << totalPieces << " pieces)" << reset << endl;
        //     lastReported = progressPercent;
        // }
        
        // Check if all pieces downloaded
        if(completed == totalPieces){
            mergePiecesToFile(fileName);
            download->isComplete = true;
            cout << fontBold << colorGreen << "Download completed: " << fileName << reset << endl;
            break;
        }
        
        // Retry failed pieces 
        if(globalDownloadPool->isIdle()){
            bool hasFailedPieces = false;
            for(auto& piece : download->pieces){
                if(!download->downloadedPieces[piece.pieceIndex]){
                    hasFailedPieces = true;
                    // Retry with next seeder
                    for(auto& seeder : piece.seeders){
                        DownloadTask* task = new DownloadTask(
                            fileName, piece.pieceIndex,
                            piece.sha1Hash, seeder
                        );
                        globalDownloadPool->addTask(task);
                        break;
                    }
                }
            }
            
            if(!hasFailedPieces) break; // All done
        }
    }
    
    return nullptr;
}

bool downloadPieceFromPeer(string& fileName, int pieceIndex, string& expectedHash, PieceSeederInfo& peer){
    // Check if already downloaded (race condition protection)
    pthread_mutex_lock(&download_mutex);
    auto it = activeDownloads.find(fileName);
    if(it == activeDownloads.end()){
        pthread_mutex_unlock(&download_mutex);
        return false;
    }
    if(it->second->downloadedPieces[pieceIndex]){
        pthread_mutex_unlock(&download_mutex);
        return true; // Already downloaded by another thread
    }
    pthread_mutex_unlock(&download_mutex);
    
    int peerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(peerSocket < 0) return false;
    
    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peer.port + 2000);
    inet_pton(AF_INET, peer.ip.c_str(), &peerAddr.sin_addr);
    
    timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(peerSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(peerSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    if(connect(peerSocket, (sockaddr*)&peerAddr, sizeof(peerAddr)) < 0){
        close(peerSocket);
        return false;
    }
    
    string request = "GET_PIECE|" + fileName + "|" + to_string(pieceIndex);
    if(writeAll(peerSocket, request.c_str(), request.size()) <= 0){
        close(peerSocket);
        return false;
    }
    
    string headerLine;
    if(!readLineFromSocket(peerSocket, headerLine)){
        close(peerSocket);
        return false;
    }
    
    if(headerLine.rfind("PIECE_DATA|", 0) != 0){
        close(peerSocket);
        return false;
    }
    
    size_t p1 = headerLine.find('|', 10);
    if(p1 == string::npos){
        close(peerSocket);
        return false;
    }
    size_t p2 = headerLine.find('|', p1 + 1);
    if(p2 == string::npos){
        close(peerSocket);
        return false;
    }
    
    int dataSize = stoi(headerLine.substr(p1 + 1, p2 - p1 - 1));
    if(dataSize < 0){ 
        close(peerSocket); 
        return false; 
    }
    
    char* pieceData = new char[dataSize];
    int totalRead = 0;
    while(totalRead < dataSize){
        long long n = read(peerSocket, pieceData + totalRead, dataSize - totalRead);
        if(n < 0){
            delete[] pieceData;
            close(peerSocket);
            return false;
        } else if(n == 0){
            usleep(1000);
            continue;
        }
        totalRead += n;
    }
    
    string pieceHash = calculateSHA1(string(pieceData, dataSize));
    if(pieceHash != expectedHash){
        cout << fontBold << colorRed << "Hash mismatch for piece " 
             << pieceIndex << " of " << fileName << reset << endl;
        delete[] pieceData;
        close(peerSocket);
        return false;
    }
    
    pthread_mutex_lock(&download_mutex);
    string destPath = activeDownloads[fileName]->destPath;
    pthread_mutex_unlock(&download_mutex);
    
    string tempFileName = destPath + ".part" + to_string(pieceIndex);
    ofstream tempFile(tempFileName, ios::binary);
    if(!tempFile.is_open()){
        delete[] pieceData;
        close(peerSocket);
        return false;
    }
    
    tempFile.write(pieceData, dataSize);
    tempFile.close();
    delete[] pieceData;
    close(peerSocket);
    
    // Mark as downloaded and notify tracker
    pthread_mutex_lock(&download_mutex);
    activeDownloads[fileName]->downloadedPieces[pieceIndex] = true;
    string groupId = activeDownloads[fileName]->groupId;
    pthread_mutex_unlock(&download_mutex);
    
    notifyTrackerPieceCompleted(groupId, fileName, pieceIndex);
    
    // cout << fontBold << colorGreen << "[Thread " << pthread_self() << "] "<< "Downloaded piece " << pieceIndex << " of " << fileName << " from " << peer.ip << ":" << peer.port << reset << endl;
    
    return true;
}

// mergePiecesToFile and notify functions remain the same
void mergePiecesToFile(string& fileName){
    pthread_mutex_lock(&download_mutex);
    auto it = activeDownloads.find(fileName);
    if(it == activeDownloads.end()){
        pthread_mutex_unlock(&download_mutex);
        return;
    }
    
    DownloadInfo* download = it->second;
    string destPath = download->destPath;
    int totalPieces = download->totalPieces;
    pthread_mutex_unlock(&download_mutex);
    
    ofstream finalFile(destPath, ios::binary);
    if(!finalFile.is_open()){
        cout << fontBold << colorRed << "Cannot create final file: " << destPath << reset << endl;
        return;
    }
    
    const int PIECE_SIZE = 512 * 1024;
    for(int i = 0; i < totalPieces; i++){
        string tempFileName = destPath + ".part" + to_string(i);
        ifstream tempFile(tempFileName, ios::binary);
        if(!tempFile.is_open()){
            cout << fontBold << colorRed << "Missing piece file: " << tempFileName << reset << endl;
            finalFile.close();
            return;
        }
        
        char buffer[PIECE_SIZE];
        tempFile.read(buffer, PIECE_SIZE);
        int bytesRead = tempFile.gcount();
        finalFile.write(buffer, bytesRead);
        
        tempFile.close();
        unlink(tempFileName.c_str());
    }
    
    finalFile.close();
    
    pthread_mutex_lock(&seed_mutex);
    SeedInfo* seedInfo = new SeedInfo(fileName, destPath, download->groupId, download->fileSize);
    seedInfo->fullFileSHA = download->fullFileSHA1;
    seedInfo->pieces = calculateFilePieces(destPath);
    seedingFiles[fileName] = seedInfo;
    pthread_mutex_unlock(&seed_mutex);
    
    cout << fontBold << colorGreen << "File merged successfully: " << destPath << reset << endl;
    notifyTrackerDownloadComplete(download->groupId, fileName);
}

void notifyTrackerPieceCompleted(string& groupId, string& fileName, int pieceIndex){
    extern pair<string,int> clientInfo;
    extern string currentUserId;
    
    string command = "piece_completed " + groupId + " " + fileName + " " + to_string(pieceIndex) + " " + currentUserId + " "  + clientInfo.first + " " + to_string(clientInfo.second) + "\n";
    
    pthread_mutex_lock(&tracker_mutex);
    if(trackerAlive && sockfd != -1){
        write(sockfd, command.c_str(), command.length());
    }
    pthread_mutex_unlock(&tracker_mutex);
}

void notifyTrackerDownloadComplete(string& groupId, string& fileName){
    extern pair<string,int> clientInfo;
    extern string currentUserId;
    
    string command = "download_complete " + groupId + " " + fileName + " " + currentUserId + " " + clientInfo.first + " " + to_string(clientInfo.second) + "\n";
    
    pthread_mutex_lock(&tracker_mutex);
    if(trackerAlive && sockfd != -1){
        write(sockfd, command.c_str(), command.length());
    }
    pthread_mutex_unlock(&tracker_mutex);
}

#endif // DOWNLOAD_H