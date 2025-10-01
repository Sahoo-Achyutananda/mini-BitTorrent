#if !defined(DOWNLOAD_H)
#define DOWNLOAD_H

#include "client_constructs.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include "sha.h"
#include <arpa/inet.h>
#include "fileops.h"
#include "client.h"
#include "utils.h"

extern pair<string, int> clientInfo;

void* downloadServer(void* arg);
void* handlePeerRequest(void* arg);
bool servePieceToClient(int clientSocket, string fileName, int pieceIndex);

// Download worker functions
void* downloadWorker(void* arg);
bool downloadPieceFromPeer(string& fileName, int pieceIndex, string& expectedHash, PieceSeederInfo& peer);
void mergePiecesToFile(string& fileName);
void notifyTrackerPieceCompleted(string& groupId, string& fileName, int pieceIndex);

// Initialize download server
void initializeDownloadServer(int basePort){
    if(downloadServerRunning) return;
    
    downloadServerPort = basePort + 2000; // Download port offset
    downloadServerRunning = true;
    
    pthread_create(&downloadServerThread, nullptr, downloadServer, nullptr);
    pthread_detach(downloadServerThread);
    
    cout << fontBold << colorGreen << "Download server started on port " << downloadServerPort << reset << endl;
}

// Download server implementation
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
    
    cout << fontBold << colorBlue << "Download server listening on port " << downloadServerPort << reset << endl;
    
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
    
    // cout << request << endl ; // debug

    // Parse request: "GET_PIECE|fileName|pieceIndex"
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
    
    if(servePieceToClient(clientSocket, fileName, pieceIndex)){
        // cout << fontBold << colorBlue << "Served piece " << pieceIndex << " of " << fileName << reset << endl;
    }
    
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
    
    // Read piece from file
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
    
    // Send response header and data
    string response = "PIECE_DATA|" + to_string(bytesRead) + "|\n";
    // write(clientSocket, response.c_str(), response.length());
    // write(clientSocket, pieceData, bytesRead);
    if(writeAll(clientSocket, response.c_str(), response.size()) < 0){
        cout << colorRed << fontBold  << "Something went wron - download.h - serverPIeceToCLient - line 173" << endl;
    }
    if (writeAll(clientSocket, pieceData, bytesRead) < 0){
        cout << colorRed << fontBold  << "Something went wron - download.h - serverPIeceToCLient - line 176" << endl;
    }
    
    delete[] pieceData;
    return true;
}

void handleFileMetadata(string response){
    // Format: FILE_META|fileName|fileSize|fullFileSHA1|totalPieces|groupId|destPath|piece0:hash:ip1:port1;ip2:port2|piece1:hash:ip1:port1;ip2:port2|...
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
    
    // Parse piece information with seeder details
    for(int i = 7; i < parts.size(); i++){
        string pieceInfo = parts[i];
        
        // Parse: "pieceIndex:hash:ip1:port1;ip2:port2"
        size_t firstColon = pieceInfo.find(':');
        size_t secondColon = pieceInfo.find(':', firstColon + 1);
        
        if(firstColon == string::npos || secondColon == string::npos) continue;
        
        int pieceIndex = stoi(pieceInfo.substr(0, firstColon));
        string pieceHash = pieceInfo.substr(firstColon + 1, secondColon - firstColon - 1);
        string seedersStr = pieceInfo.substr(secondColon + 1);
        
        DownloadPieceInfo piece(pieceIndex, pieceHash);
        
        // Parse seeder list: "ip1:port1;ip2:port2"
        if(!seedersStr.empty()){
            stringstream seederStream(seedersStr);
            string seederInfo;
            while(getline(seederStream, seederInfo, ';')){
                size_t colonPos = seederInfo.find(':');
                if(colonPos != string::npos){
                    string ip = seederInfo.substr(0, colonPos);
                    int port = stoi(seederInfo.substr(colonPos + 1));
                    piece.addSeeder(ip, port);
                }
            }
        }
        
        download->pieces.push_back(piece);
    }
    
    activeDownloads[fileName] = download;
    pthread_mutex_unlock(&download_mutex);
    
    cout << fontBold << colorBlue << "Started download: " << fileName << " (" << fileSize << " bytes, " << totalPieces << " pieces)" << reset << endl;
    
    // Start download worker thread
    pthread_t workerThread;
    pthread_create(&workerThread, nullptr, downloadWorker, new string(fileName));
    pthread_detach(workerThread);
}

// Download worker implementation -runs on the client that requested the file
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
    pthread_mutex_unlock(&download_mutex);
    
    cout << fontBold << colorYellow << "Starting download worker for " << fileName << reset << endl;
    
    // Download all pieces
    for(auto piece : download->pieces){
        // cout << colorYellow << piece.pieceIndex << " " << piece.sha1Hash << reset << endl ; // debug
        // for(auto seeder : piece.seeders){
        //     cout << seeder.ip << " " << seeder.port << endl;
        // }
        if(download->downloadedPieces[piece.pieceIndex]) continue;
        
        bool pieceDownloaded = false;
        for(auto seeder : piece.seeders){
            // cout << colorBlue << seeder.ip << " " << seeder.isAlive << " " << seeder.port << " " << reset << endl;
            if(downloadPieceFromPeer(fileName, piece.pieceIndex, piece.sha1Hash, seeder)){
                pthread_mutex_lock(&download_mutex);
                download->downloadedPieces[piece.pieceIndex] = true;
                pthread_mutex_unlock(&download_mutex);
                
                notifyTrackerPieceCompleted(download->groupId, fileName, piece.pieceIndex);
                pieceDownloaded = true;
                break;
            }
        }
        
        if(!pieceDownloaded){
            cout << fontBold << colorRed << "Failed to download piece " << piece.pieceIndex << " of " << fileName << reset << endl;
        }
    }
    
    // Check if all pieces downloaded
    bool allDownloaded = true;
    for(bool downloaded : download->downloadedPieces){
        if(!downloaded){
            allDownloaded = false;
            break;
        }
    }
    
    if(allDownloaded){
        mergePiecesToFile(fileName);
        download->isComplete = true;
        cout << fontBold << colorGreen << "Download completed: " << fileName << reset << endl;
    }
    
    return nullptr;
}

bool downloadPieceFromPeer(string& fileName, int pieceIndex, string& expectedHash, PieceSeederInfo& peer){
    int peerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(peerSocket < 0) return false;
    
    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peer.port + 2000); // Download server port -> each client will have this
    // cout << "debug : connected to" << peer.port + 2000 << endl; // debug

    inet_pton(AF_INET, peer.ip.c_str(), &peerAddr.sin_addr);
    
    // Set timeout
    timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(peerSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(peerSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    if(connect(peerSocket, (sockaddr*)&peerAddr, sizeof(peerAddr)) < 0){
        close(peerSocket);
        return false;
    }
    
    // Send piece request
    string request = "GET_PIECE|" + fileName + "|" + to_string(pieceIndex);
    cout << colorRed << request << reset << endl;

    if(writeAll(peerSocket, request.c_str(), request.size()) <= 0){
        close(peerSocket);
        return false;
    }
        
    // Read response header

    string headerLine;
    if (!readLineFromSocket(peerSocket, headerLine)){
        close(peerSocket);
        return false;
    }

    // headerLine should be like: PIECE_DATA|<size>|
    if (headerLine.rfind("PIECE_DATA|", 0) != 0){
        // cout << colorGreen << headerLine << reset << endl;  // debug
        close(peerSocket);
        return false;
    }
    size_t p1 = headerLine.find('|', 10); // after "PIECE_DATA"
    if (p1 == string::npos){
        close(peerSocket);
        return false;
    }
    size_t p2 = headerLine.find('|', p1 + 1);
    if (p2 == string::npos){
        close(peerSocket);
        return false;
    }
    int dataSize = stoi(headerLine.substr(p1 + 1, p2 - p1 - 1));
    if (dataSize < 0){ close(peerSocket); return false; }

    // Allocate buffer and read exactly dataSize bytes
    char* pieceData = new char[dataSize];

    int totalRead = 0;
    while(totalRead < dataSize){
        long long n = read(peerSocket, pieceData + totalRead, dataSize - totalRead);
        if(n < 0){
            perror("read failed");
            delete[] pieceData;
            close(peerSocket);
            return false;
        } else if(n == 0){
            // Connection closed but we haven't read full piece
            usleep(1000); // wait a tiny bit and retry
            continue;
        }
        totalRead += n;
    }

    // char buffer[1024];
    // int n = read(peerSocket, buffer, sizeof(buffer) - 1);
    // if(n <= 0){
    //     close(peerSocket);
    //     return false;
    // }
    
    // buffer[n] = '\0';
    // string response(buffer, n); // getting an error in the last piece !
    
    // if(response.find("ERROR") == 0){
    //     close(peerSocket);
    //     return false;
    // }
    
    // // Parse data size from response
    // size_t firstPipe = response.find('|');
    // size_t secondPipe = response.find('|', firstPipe + 1);
    // if(firstPipe == string::npos || secondPipe == string::npos){
    //     close(peerSocket);
    //     return false;
    // }
    
    // int dataSize = stoi(response.substr(firstPipe + 1, secondPipe - firstPipe - 1));
    // size_t headerSize = secondPipe + 1;
    
    // // Read piece data
    // char* pieceData = new char[dataSize];
    // int dataAlreadyRead = min(n - (int)headerSize, dataSize);
    // if(dataAlreadyRead > 0){
    //     memcpy(pieceData, buffer + headerSize, dataAlreadyRead);
    // }
    
    // int totalRead = dataAlreadyRead;
    // while(totalRead < dataSize){
    //     n = read(peerSocket, pieceData + totalRead, dataSize - totalRead);
    //     if(n <= 0) break;
    //     totalRead += n;
    // }
    
    // close(peerSocket);
    
    // if(totalRead != dataSize){
    //     delete[] pieceData;
    //     return false;
    // }
    
    // Verify hash
    string pieceHash = calculateSHA1(string(pieceData, dataSize));
    if(pieceHash != expectedHash){
        cout << fontBold << colorRed << "Hash mismatch for piece " << pieceIndex << " of " << fileName << reset << endl;
        // debug - 
        // cout << pieceHash << " " << expectedHash << endl;
        delete[] pieceData;
        return false;
    }
    
    // Save piece to temporary file
    pthread_mutex_lock(&download_mutex);
    string destPath = activeDownloads[fileName]->destPath;
    pthread_mutex_unlock(&download_mutex);

    cout << colorRed << destPath << reset << endl;

    string tempFileName = destPath + ".part" + to_string(pieceIndex);
    ofstream tempFile(tempFileName, ios::binary);
    if(!tempFile.is_open()){
        delete[] pieceData;
        return false;
    }
    
    tempFile.write(pieceData, dataSize);
    tempFile.close();
    delete[] pieceData;
    close(peerSocket); // forgot htis line - got max open files error
    cout << fontBold << colorGreen << "Downloaded piece " << pieceIndex << " of " << fileName << " from " << peer.ip << ":" << peer.port << reset << endl;
    
    return true;
}

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
        unlink(tempFileName.c_str()); // Delete temporary file
    }
    
    finalFile.close();
    
    // Add to seeding files
    pthread_mutex_lock(&seed_mutex);
    SeedInfo* seedInfo = new SeedInfo(fileName, destPath, download->groupId, download->fileSize);
    seedInfo->fullFileSHA = download->fullFileSHA1;
    seedInfo->pieces = calculateFilePieces(destPath);
    seedingFiles[fileName] = seedInfo;
    pthread_mutex_unlock(&seed_mutex);
    
    cout << fontBold << colorGreen << fileName << "File merged successfully: " << destPath << reset << endl;
}

void notifyTrackerPieceCompleted(string& groupId, string& fileName, int pieceIndex){
    // Get client info (you'll need to pass this from main)
    extern pair<string,int> clientInfo; // Declare as extern
    
    string command = "piece_completed " + groupId + " " + fileName + " " + to_string(pieceIndex) + " " + clientInfo.first + " " + to_string(clientInfo.second) + "\n";
    
    pthread_mutex_lock(&tracker_mutex);
    if(trackerAlive && sockfd != -1){
        write(sockfd, command.c_str(), command.length());
    }
    pthread_mutex_unlock(&tracker_mutex);
}


#endif // DOWNLOAD_H
