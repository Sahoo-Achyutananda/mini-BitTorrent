#if !defined(CLIENT_CONSTRUCTS_H)
#define CLIENT_CONSTRUCTS_H

#include "constructs.h"
#include "colors.h"
// there are two aspects to the client -> it can download and it can share... <seeder and also a receiver>
// so, i made two classes for the same , also two DS, two thread_mutexes-



class DownloadInfo;
class SeedInfo;

unordered_map<string, DownloadInfo*> activeDownloads; // fileName -> DownloadInfo*
pthread_mutex_t download_mutex = PTHREAD_MUTEX_INITIALIZER;
unordered_map<string, SeedInfo*> seedingFiles; // fileName -> SeedInfo*
pthread_mutex_t seed_mutex = PTHREAD_MUTEX_INITIALIZER;

class DownloadInfo {
public :
    string fileName;
    string groupId;
    string destPath;
    long long fileSize;
    string fullFileSHA1;
    vector<pair<int, string>> pieces; // (index, hash)
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
void handleFileMetadata(string response) {
    // Format: FILE_META|fileName|fileSize|fullFileSHA1|totalPieces|piece1_index:hash|piece2_index:hash|...
    vector<string> parts;
    stringstream ss(response);
    string item;
    
    while(getline(ss, item, '|')) {
        parts.push_back(item);
    }
    
    if(parts.size() < 5 || parts[0] != "FILE_META") {
        cout << "Invalid file metadata format" << endl; // that shouldnt happen though
        return;
    }
    
    string fileName = parts[1];
    long long fileSize = stoll(parts[2]);
    string fullFileSHA1 = parts[3];
    int totalPieces = stoi(parts[4]);
    
    pthread_mutex_lock(&download_mutex);
    
    DownloadInfo* download = new DownloadInfo();
    download->fileName = fileName;
    download->fileSize = fileSize;
    download->fullFileSHA1 = fullFileSHA1;
    download->totalPieces = totalPieces;
    download->downloadedPieces.resize(totalPieces, false);
    
    // Parse piece information - yeh bhi tracker send karta hai
    // pieces aare passed after the 5th | -> look at the example format on top
    for(int i = 5; i < parts.size(); i++) {
        int colonPos = parts[i].find(':');
        if(colonPos != string::npos) {
            int pieceIndex = stoi(parts[i].substr(0, colonPos));
            string pieceHash = parts[i].substr(colonPos + 1);
            download->pieces.push_back({pieceIndex, pieceHash});
        }
    }
    
    activeDownloads[fileName] = download;
    pthread_mutex_unlock(&download_mutex);
    
    cout << fontBold << colorBlue << "Started download: " << fileName << " (" << fileSize << " bytes, " << totalPieces << " pieces)" << reset << endl;
}


#endif // CLIENT_CONSTRUCTS_H
