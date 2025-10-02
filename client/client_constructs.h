#if !defined(CLIENT_CONSTRUCTS_H)
#define CLIENT_CONSTRUCTS_H

#include <queue>
#include "./tracker/constructs.h"
#include "./tracker/colors.h"
#include "./tracker/synchronize.h" // sync message helper
// there are two aspects to the client -> it can download and it can share... <seeder and also a receiver>
// so, i made two classes for the same , also two DS, two thread_mutexes-
// #include "download.h"

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

class PieceSeederInfo {
public :
    string userId;
    string ip;
    int port;
    bool isAlive;
    PieceSeederInfo(string user, string ip, int port) : userId(user), ip(ip), port(port), isAlive(true) {}
};

class DownloadPieceInfo {
public:
    int pieceIndex;
    string sha1Hash;
    vector<PieceSeederInfo> seeders;
    bool isDownloaded;
    
    DownloadPieceInfo(int index, string hash) : pieceIndex(index), sha1Hash(hash), isDownloaded(false){}
    
    void addSeeder(string userId, string ip, int port){
        seeders.push_back(PieceSeederInfo(userId, ip, port));
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


class SeedPiece {
public :
    int pieceIndex;
    string sha1Hash;
    
    SeedPiece(int index, string hash){
        this->pieceIndex = index;
        this->sha1Hash = hash;
    }
};

class SeedInfo {
public :
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


void* downloadServer(void* arg);
void* handlePeerRequest(void* arg);
bool servePieceToClient(int clientSocket, string fileName, int pieceIndex);

// Download worker functions
void* downloadWorker(void* arg);
bool downloadPieceFromPeer(const string& fileName, int pieceIndex, const string& expectedHash, const PieceSeederInfo& peer);
void mergePiecesToFile(const string& fileName);
void notifyTrackerPieceCompleted(const string& groupId, const string& fileName, int pieceIndex);

void handlePieceCompleted(int newsockfd, vector<string>& tokens, string& clientName){
    // piece_completed <group_id> <file_name> <piece_index> <user_id> <client_ip> <client_port>
    if(tokens.size() != 7){
        writeToClient(newsockfd, "Usage: piece_completed <group_id> <file_name> <piece_index> <user_id> <client_ip> <client_port>");
        return;
    }

    string groupId = tokens[1];
    string fileName = tokens[2];
    int pieceIndex = stoi(tokens[3]);
    string userId = tokens[4];
    string clientIP = tokens[5];
    int clientPort = stoi(tokens[6]);
    
    if(clientName.empty()){
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }

    // Check if group and file exist
    if(groups.find(groupId) == groups.end()){
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    Group* g = groups[groupId];
    FileInfo* fileInfo = g->getFileInfo(fileName);
    if(fileInfo == nullptr){
        writeToClient(newsockfd, "File not found in group");
        return;
    }
    
    // Add client as seeder for this specific piece
    if(pieceIndex >= 0 && pieceIndex < fileInfo->pieces.size()){
        // Check if this seeder is already in the list for this piece
        bool alreadySeeder = false;
        for(const auto& seeder : fileInfo->pieces[pieceIndex].seeders){
            if(seeder.ip == clientIP && seeder.port == clientPort && seeder.userId == userId) {
                alreadySeeder = true;
                break;
            }
        }
        
        if(!alreadySeeder){
            fileInfo->pieces[pieceIndex].seeders.push_back(SeederInfo(userId, clientIP, clientPort));
            fileInfo->pieces[pieceIndex].isAvailable = true;
            
            // Sync this update with other trackers
            string syncData = groupId + " " + fileName + " " + to_string(pieceIndex) + " " + userId + " " + clientIP + " " + to_string(clientPort);
            syncMessageHelper("PIECE_COMPLETED", syncData);
            
            // cout << "Added " << userId << " as seeder for piece " << pieceIndex << " of " << fileName << endl;
            // writeToClient(newsockfd, "Piece seeder info updated successfully");
        }
    }
}


#endif // CLIENT_CONSTRUCTS_H
