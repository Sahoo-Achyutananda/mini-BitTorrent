#if !defined(CLIENT_CONSTRUCTS_H)
#define CLIENT_CONSTRUCTS_H

#include "constructs.h"
#include "colors.h"
#include "synchronize.h" // sync message helper
// there are two aspects to the client -> it can download and it can share... <seeder and also a receiver>
// so, i made two classes for the same , also two DS, two thread_mutexes-

class DownloadInfo;
class SeedInfo;

unordered_map<string, DownloadInfo*> activeDownloads; // fileName -> DownloadInfo*
pthread_mutex_t download_mutex = PTHREAD_MUTEX_INITIALIZER;
unordered_map<string, SeedInfo*> seedingFiles; // fileName -> SeedInfo*
pthread_mutex_t seed_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t downloadServerThread;
bool downloadServerRunning = false;
int downloadServerPort = 0;

int trackerIndex = 0;
int sockfd = -1;
bool trackerAlive = false;
pthread_mutex_t tracker_mutex = PTHREAD_MUTEX_INITIALIZER;

struct PieceSeederInfo {
    string ip;
    int port;
    bool isAlive;
    
    PieceSeederInfo(string ip, int port) : ip(ip), port(port), isAlive(true) {}
};

class DownloadPieceInfo {
public:
    int pieceIndex;
    string sha1Hash;
    vector<PieceSeederInfo> seeders;
    bool isDownloaded;
    
    DownloadPieceInfo(int index, string hash) : pieceIndex(index), sha1Hash(hash), isDownloaded(false) {}
    
    void addSeeder(string ip, int port) {
        seeders.push_back(PieceSeederInfo(ip, port));
    }
};

class DownloadInfo {
public :
    string fileName;
    string groupId;
    string destPath;
    long long fileSize;
    string fullFileSHA1;
    vector<DownloadPieceInfo> pieces; // (index, hash)->pehle tha
    vector<bool> downloadedPieces;
    int totalPieces;
    bool isComplete;
    
    DownloadInfo(){
        this->isComplete = false;
        this->totalPieces = 0;
    }
};


struct SeedPiece {
    int pieceIndex;
    string sha1Hash;
    
    SeedPiece(int index, string hash){
        this->pieceIndex = index;
        this->sha1Hash = hash;
    }
};

struct SeedInfo {
    string fileName;
    string filePath;
    string groupId;
    string fullFileSHA;
    long long fileSize;
    vector<SeedPiece> pieces;
    
    SeedInfo(string name, string path, string groupId, long long size){
        this->fileName = name;
        this->filePath = path;
        this->groupId = groupId;
        this->fileSize = size; 
    }
};

// function to parse the file's data receiveed from the tracker !!
// void handleFileMetadata(string response) {
//     // Format: FILE_META|fileName|fileSize|fullFileSHA1|totalPieces|piece1_index:hash|piece2_index:hash|...
//     vector<string> parts;
//     stringstream ss(response);
//     string item;
    
//     while(getline(ss, item, '|')) {
//         parts.push_back(item);
//     }
    
//     if(parts.size() < 5 || parts[0] != "FILE_META") {
//         cout << "Invalid file metadata format" << endl; // that shouldnt happen though
//         return;
//     }
    
//     string fileName = parts[1];
//     long long fileSize = stoll(parts[2]);
//     string fullFileSHA1 = parts[3];
//     int totalPieces = stoi(parts[4]);
    
//     pthread_mutex_lock(&download_mutex);
    
//     DownloadInfo* download = new DownloadInfo();
//     download->fileName = fileName;
//     download->fileSize = fileSize;
//     download->fullFileSHA1 = fullFileSHA1;
//     download->totalPieces = totalPieces;
//     download->downloadedPieces.resize(totalPieces, false);
    
//     // Parse piece information - yeh bhi tracker send karta hai
//     // pieces aare passed after the 5th | -> look at the example format on top
//     for(int i = 5; i < parts.size(); i++) {
//         int colonPos = parts[i].find(':');
//         if(colonPos != string::npos) {
//             int pieceIndex = stoi(parts[i].substr(0, colonPos));
//             string pieceHash = parts[i].substr(colonPos + 1);
//             download->pieces.push_back({pieceIndex, pieceHash});
//         }
//     }
    
//     activeDownloads[fileName] = download;
//     pthread_mutex_unlock(&download_mutex);
    
//     cout << fontBold << colorBlue << "Started download: " << fileName << " (" << fileSize << " bytes, " << totalPieces << " pieces)" << reset << endl;
// }

void* downloadServer(void* arg);
void* handlePeerRequest(void* arg);
bool servePieceToClient(int clientSocket, string fileName, int pieceIndex);

// Download worker functions
void* downloadWorker(void* arg);
bool downloadPieceFromPeer(const string& fileName, int pieceIndex, const string& expectedHash, const PieceSeederInfo& peer);
void mergePiecesToFile(const string& fileName);
void notifyTrackerPieceCompleted(const string& groupId, const string& fileName, int pieceIndex);

// void handleFileMetadata(string response) {
//     // Format: FILE_META|fileName|fileSize|fullFileSHA1|totalPieces|groupId|destPath|piece0:hash:ip1:port1;ip2:port2|piece1:hash:ip1:port1;ip2:port2|...
//     vector<string> parts;
//     stringstream ss(response);
//     string item;
    
//     while(getline(ss, item, '|')) {
//         parts.push_back(item);
//     }
    
//     if(parts.size() < 7 || parts[0] != "FILE_META") {
//         cout << colorRed << "Invalid file metadata format" << reset << endl;
//         return;
//     }
    
//     string fileName = parts[1];
//     long long fileSize = stoll(parts[2]);
//     string fullFileSHA1 = parts[3];
//     int totalPieces = stoi(parts[4]);
//     string groupId = parts[5];
//     string destPath = parts[6];
    
//     pthread_mutex_lock(&download_mutex);
    
//     DownloadInfo* download = new DownloadInfo();
//     download->fileName = fileName;
//     download->fileSize = fileSize;
//     download->fullFileSHA1 = fullFileSHA1;
//     download->totalPieces = totalPieces;
//     download->groupId = groupId;
//     download->destPath = destPath;
//     download->downloadedPieces.resize(totalPieces, false);
    
//     // Parse piece information with seeder details
//     for(int i = 7; i < parts.size(); i++) {
//         string pieceInfo = parts[i];
        
//         // Parse: "pieceIndex:hash:ip1:port1;ip2:port2"
//         size_t firstColon = pieceInfo.find(':');
//         size_t secondColon = pieceInfo.find(':', firstColon + 1);
        
//         if(firstColon == string::npos || secondColon == string::npos) continue;
        
//         int pieceIndex = stoi(pieceInfo.substr(0, firstColon));
//         string pieceHash = pieceInfo.substr(firstColon + 1, secondColon - firstColon - 1);
//         string seedersStr = pieceInfo.substr(secondColon + 1);
        
//         DownloadPieceInfo piece(pieceIndex, pieceHash);
        
//         // Parse seeder list: "ip1:port1;ip2:port2"
//         if(!seedersStr.empty()) {
//             stringstream seederStream(seedersStr);
//             string seederInfo;
//             while(getline(seederStream, seederInfo, ';')) {
//                 size_t colonPos = seederInfo.find(':');
//                 if(colonPos != string::npos) {
//                     string ip = seederInfo.substr(0, colonPos);
//                     int port = stoi(seederInfo.substr(colonPos + 1));
//                     piece.addSeeder(ip, port);
//                 }
//             }
//         }
        
//         download->pieces.push_back(piece);
//     }
    
//     activeDownloads[fileName] = download;
//     pthread_mutex_unlock(&download_mutex);
    
//     cout << fontBold << colorBlue << "Started download: " << fileName << " (" << fileSize << " bytes, " << totalPieces << " pieces)" << reset << endl;
    
//     // // Start download worker thread
//     // pthread_t workerThread;
//     // pthread_create(&workerThread, nullptr, downloadWorker, new string(fileName));
//     // pthread_detach(workerThread);
// }


void handlePieceCompleted(int newsockfd, vector<string>& tokens, string& clientName) {
    // piece_completed <group_id> <file_name> <piece_index> <client_ip> <client_port>
    if(tokens.size() != 6) {
        writeToClient(newsockfd, "Usage: piece_completed <group_id> <file_name> <piece_index> <client_ip> <client_port>");
        return;
    }
    
    if(clientName.empty()) {
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    string fileName = tokens[2];
    int pieceIndex = stoi(tokens[3]);
    string clientIP = tokens[4];
    int clientPort = stoi(tokens[5]);
    
    // Check if group and file exist
    if(groups.find(groupId) == groups.end()) {
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    Group* g = groups[groupId];
    FileInfo* fileInfo = g->getFileInfo(fileName);
    if(fileInfo == nullptr) {
        writeToClient(newsockfd, "File not found in group");
        return;
    }
    
    // Add client as seeder for this specific piece
    if(pieceIndex >= 0 && pieceIndex < fileInfo->pieces.size()) {
        // Check if this seeder is already in the list for this piece
        bool alreadySeeder = false;
        for(const auto& seeder : fileInfo->pieces[pieceIndex].seeders) {
            if(seeder.second == clientIP && seeder.first == clientPort) {
                alreadySeeder = true;
                break;
            }
        }
        
        if(!alreadySeeder) {
            fileInfo->pieces[pieceIndex].seeders.push_back({clientPort, clientIP});
            fileInfo->pieces[pieceIndex].isAvailable = true;
            
            // Sync this update with other trackers
            string syncData = groupId + " " + fileName + " " + to_string(pieceIndex) + " " + clientIP + " " + to_string(clientPort) + " " + clientName;
            syncMessageHelper("PIECE_COMPLETED", syncData);
            
            cout << "Added " << clientName << " as seeder for piece " << pieceIndex << " of " << fileName << endl;
            writeToClient(newsockfd, "Piece seeder info updated successfully");
        } else {
            writeToClient(newsockfd, "Already registered as seeder for this piece");
        }
    } else {
        writeToClient(newsockfd, "Invalid piece index");
    }
}


#endif // CLIENT_CONSTRUCTS_H
