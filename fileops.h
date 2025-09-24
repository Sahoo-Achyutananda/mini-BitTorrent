#if !defined(FILEOPS_H)
#define FILEOPS_H


#include "constructs.h"
#include "sha.h"
#include "synchronize.h"
#include <openssl/sha.h>
#include <sys/stat.h>

// File piece calculation
vector<FilePiece> calculateFilePieces(const string& filePath) {
    vector<FilePiece> pieces;
    ifstream file(filePath, ios::binary);
    if(!file.is_open()) return pieces;
    
    const int PIECE_SIZE = 512 * 1024; // 512KB
    char buffer[PIECE_SIZE];
    int pieceIndex = 0;
    
    while(file.read(buffer, PIECE_SIZE) || file.gcount() > 0) { // gcount returns the total bytes read i the last unformatted input operation
        int bytesRead = file.gcount();
        string pieceData(buffer, bytesRead);
        string pieceHash = calculateSHA1(pieceData);
        
        pieces.push_back(FilePiece(pieceIndex++, pieceHash));
    }
    
    file.close();
    return pieces;
}

long long getFileSize(const string& filePath) {
    struct stat stat_buf;
    int rc = stat(filePath.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : 0;
}

void handleUploadFile(int newsockfd, vector<string>& tokens, string& clientName) {
    // upload_file <group_id> <file_path>
    if(tokens.size() != 3) {
        writeToClient(newsockfd, "Usage: upload_file <group_id> <file_path>");
        return;
    }
    
    if(clientName.empty()) {
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    string filePath = tokens[2];
    
    // Check if group exists
    if(groups.find(groupId) == groups.end()) {
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    // Check if user is member of group
    Group* g = groups[groupId];
    if(!g->checkUserExistance(clientName)) {
        writeToClient(newsockfd, "You are not a member of this group");
        return;
    }
    
    // Check if file exists
    ifstream file(filePath);
    if(!file.is_open()) {
        writeToClient(newsockfd, "File not found: " + filePath);
        return;
    }
    file.close();
    
    // Get file name from path
    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    
    // Check if file already exists in group
    if(g->getFileInfo(fileName) != nullptr) {
        writeToClient(newsockfd, "File already exists in group");
        return;
    }
    
    // Calculate file size
    long long fileSize = getFileSize(filePath);
    if(fileSize == 0) {
        writeToClient(newsockfd, "Invalid file or empty file");
        return;
    }
    
    // Create file info
    FileInfo* fileInfo = new FileInfo(fileName, filePath, fileSize, clientName, groupId);
    
    // Calculate SHA1 and pieces
    fileInfo->fullFileSHA1 = calculateFileSHA1(filePath);
    fileInfo->pieces = calculateFilePieces(filePath);
    
    // Add to group and global storage
    g->addSharedFile(fileName, fileInfo);
    allFiles[fileName] = fileInfo;
    
    // Sync with other trackers
    string syncData = groupId + " " + fileName + " " + filePath + " " + 
                     to_string(fileSize) + " " + clientName + " " + fileInfo->fullFileSHA1;
    syncMessageHelper("UPLOAD_FILE", syncData);
    
    cout << "File uploaded: " << fileName << " by " << clientName << " in group " << groupId << endl;
    writeToClient(newsockfd, "File uploaded successfully");
}

void handleListFiles(int newsockfd, vector<string>& tokens, string& clientName) {
    // list_files <group_id>
    if(tokens.size() != 2) {
        writeToClient(newsockfd, "Usage: list_files <group_id>");
        return;
    }
    
    if(clientName.empty()) {
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    
    // Check if group exists
    if(groups.find(groupId) == groups.end()) {
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    // Check if user is member of group
    Group* g = groups[groupId];
    if(!g->checkUserExistance(clientName)) {
        writeToClient(newsockfd, "You are not a member of this group");
        return;
    }
    
    vector<string> files = g->getFileList();
    if(files.empty()) {
        writeToClient(newsockfd, "No files in this group");
    } else {
        for(const string& file : files) {
            writeToClient(newsockfd, file);
        }
    }
}

void handleDownloadFile(int newsockfd, vector<string>& tokens, string& clientName) {
    // download_file <group_id> <file_name> <destination_path>
    if(tokens.size() != 4) {
        writeToClient(newsockfd, "Usage: download_file <group_id> <file_name> <destination_path>");
        return;
    }
    
    if(clientName.empty()) {
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    string fileName = tokens[2];
    string destPath = tokens[3];
    
    // Check if group exists
    if(groups.find(groupId) == groups.end()) {
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    // Check if user is member of group
    Group* g = groups[groupId];
    if(!g->checkUserExistance(clientName)) {
        writeToClient(newsockfd, "You are not a member of this group");
        return;
    }
    
    // Check if file exists in group
    FileInfo* fileInfo = g->getFileInfo(fileName);
    if(fileInfo == nullptr) {
        writeToClient(newsockfd, "File not found in group");
        return;
    }
    
    // Send file metadata to client for download
    string response = "FILE_META|" + fileName + "|" + to_string(fileInfo->fileSize) + 
                     "|" + fileInfo->fullFileSHA1 + "|" + to_string(fileInfo->pieces.size());
    
    // Add piece information
    for(const auto& piece : fileInfo->pieces) {
        response += "|" + to_string(piece.pieceIndex) + ":" + piece.sha1Hash;
    }
    
    writeToClient(newsockfd, response);
}

void handleStopShare(int newsockfd, vector<string>& tokens, string& clientName) {
    // stop_share <group_id> <file_name>
    if(tokens.size() != 3) {
        writeToClient(newsockfd, "Usage: stop_share <group_id> <file_name>");
        return;
    }
    
    if(clientName.empty()) {
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    string fileName = tokens[2];
    
    // Check if group exists
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
    
    // Check if user is the uploader
    if(fileInfo->uploaderId != clientName) {
        writeToClient(newsockfd, "You can only stop sharing your own files");
        return;
    }
    
    // Remove from group and global storage
    g->removeSharedFile(fileName);
    allFiles.erase(fileName);
    
    // Sync with other trackers
    syncMessageHelper("STOP_SHARE", groupId + " " + fileName + " " + clientName);
    
    cout << "File sharing stopped: " << fileName << " by " << clientName << endl;
    writeToClient(newsockfd, "File sharing stopped successfully");
}


#endif // FILEOPS_H
