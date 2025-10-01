#if !defined(FILEOPS_H)
#define FILEOPS_H

#include <openssl/sha.h>
#include <sys/stat.h>

#include "./tracker/constructs.h"
#include "./tracker/synchronize.h"
#include "client_constructs.h"
#include "sha.h"

// File piece calculation
vector<SeedPiece> calculateFilePieces(const string& filePath){
    vector<SeedPiece> pieces;
    ifstream file(filePath, ios::binary);
    if(!file.is_open()) return pieces;
    
    const int PIECE_SIZE = 512 * 1024; // 512KB
    char buffer[PIECE_SIZE];
    int pieceIndex = 0;
    
    while(file.read(buffer, PIECE_SIZE) || file.gcount() > 0){ // gcount returns the total bytes read i the last unformatted input operation
        int bytesRead = file.gcount();
        string pieceData(buffer, bytesRead);
        string pieceHash = calculateSHA1(pieceData);
        // cout << "Debug : " << pieceHash << endl;
        pieces.push_back(SeedPiece(pieceIndex++, pieceHash));
    }
    
    file.close();
    return pieces;
}

long long getFileSize(const string& filePath){
    struct stat stat_buf;
    int rc = stat(filePath.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : 0;
}

void handleUploadFileTracker(int newsockfd, vector<string>& tokens, string& clientName){
    //     string message = "upload_file " + groupId + " " + fileName + " " + filePath + " " + to_string(fileSize) + " " + seedInfo->fullFileSHA + " " + cleintPOrt + " " + clientip + " " hashes ;
    //                         0                1                2                3                 4                              5                        6                 7             8 to many                
    if(clientName.empty()){
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    // for(auto tok : tokens){
    //     cout << colorRed << fontBold << tok << " " << reset;
    // }
    // cout << endl;

    string groupId = tokens[1];
    string filePath = tokens[3];
    
    // Check if group exists
    if(groups.find(groupId) == groups.end()){
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    // Check if user is member of group
    Group* g = groups[groupId];
    if(!g->checkUserExistance(clientName)){
        writeToClient(newsockfd, "You are not a member of this group");
        return;
    }
    
    // Get file name from path
    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    
    // Check if file already exists in group
    if(g->getFileInfo(fileName) != nullptr){
        writeToClient(newsockfd, "File already exists in group");
        return;
    }
    
    // Calculate file size
    long long fileSize = getFileSize(filePath);
    if(fileSize == 0){
        writeToClient(newsockfd, "Invalid file or empty file");
        return;
    }

    // Create file info
    FileInfo* fileInfo = new FileInfo(fileName, filePath, fileSize, clientName, groupId);
    
    // Calculate SHA1 and pieces
    fileInfo->fullFileSHA1 = tokens[5]; // iski zaroorat pad sakta maybe in the future

    for(int i = 8; i < tokens.size(); i++){
        string l = tokens[i];
        
        int pos = l.find(':');
        if(pos != string::npos){
            int index = stoi(l.substr(0,pos));
            string sha = l.substr(pos+1);
            // cout << "debug - " << index << " " << sha << endl;
            fileInfo->pieces.push_back(FilePiece(index,sha));
        }
    }

    fileInfo->addSeeder(stoi(tokens[6]),tokens[7]); // add seeder to the seeder list
    // Add to group and global storage
    g->addSharedFile(fileName, fileInfo);
    allFiles[fileName] = fileInfo;
    
    // Sync with other trackers
    string syncData = groupId + " " + fileName + " " + filePath + " " + to_string(fileSize) + " " + clientName + " " + fileInfo->fullFileSHA1 + " " + tokens[6] + " " + tokens[7];
    syncMessageHelper("UPLOAD_FILE", syncData);
    
    cout << "File uploaded: " << fileName << " by " << clientName << " in group " << groupId << endl;
    writeToClient(newsockfd, "File uploaded successfully");
}


// the client has to do the difficult part
void handleUploadFileClient(int newsockfd, vector<string>& tokens, pair<string,int> clientInfo){
    // upload_file <group_id> <file_path>
    if(tokens.size() != 3){
        cout << colorRed << fontBold << "Usage : upload_file <group_id> <file_path>" << reset << endl;
        return;
    }
    
    string groupId = tokens[1];
    string filePath = tokens[2];
    
    // Check if file exists
    ifstream file(filePath);
    if(!file.is_open()){
        cout << colorRed << fontBold << "File not found !" << reset << endl;
        return;
    }
    file.close();
    
    // Get file name from path
    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    
    // Calculate file size
    long long fileSize = getFileSize(filePath);
    if(fileSize == 0){
        cout << colorRed << fontBold << "File is empty" << reset << endl;
        return;
    }
    
    // Create file info
    SeedInfo* seedInfo = new SeedInfo(fileName, filePath, groupId, fileSize);
    
    // Calculate SHA1 and pieces
    seedInfo->fullFileSHA = calculateFileSHA1(filePath); // iski zaroorat pad sakta maybe in the future
    seedInfo->pieces = calculateFilePieces(filePath);
    
    string message = "upload_file " + groupId + " " + fileName + " " + filePath + " " + to_string(fileSize) + " " + seedInfo->fullFileSHA + " " + to_string(clientInfo.second) + " " + clientInfo.first + " ";
    // attaching the piecewise hashes too - 
    for(auto sp : seedInfo->pieces){
        message += to_string(sp.pieceIndex);
        message += ":";
        message += sp.sha1Hash;
        message += " ";
    }

    message+='\n';

    int n = write(newsockfd, message.c_str(), message.size());
    if(n <= 0){
        perror("Error sending upload info to tracker");
        // Remove from seeding files if send failed
        pthread_mutex_lock(&seed_mutex);
        seedingFiles.erase(fileName);
        pthread_mutex_unlock(&seed_mutex);
    } else {
        // cout << "debug : " << message << endl;
        pthread_mutex_lock(&seed_mutex);
        seedingFiles[fileName] = seedInfo;
        pthread_mutex_unlock(&seed_mutex);
        cout << colorGreen << fontBold << "File metadata sent to tracker successfully!" << reset << endl;
    }
}

void handleListFiles(int newsockfd, vector<string>& tokens, string& clientName){
    // list_files <group_id>
    if(tokens.size() != 2){
        writeToClient(newsockfd, "Usage: list_files <group_id>");
        return;
    }
    
    if(clientName.empty()){
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    
    // Check if group exists
    if(groups.find(groupId) == groups.end()){
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    // Check if user is member of group
    Group* g = groups[groupId];
    if(!g->checkUserExistance(clientName)){
        writeToClient(newsockfd, "You are not a member of this group");
        return;
    }
    
    vector<string> files = g->getFileList();
    if(files.empty()){
        writeToClient(newsockfd, "No files in this group");
    } else {
        for(const string& file : files){
            writeToClient(newsockfd, file);
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
///////// The following commands will share info that is to be intercepted by the client -> pehle aisa nahi hita tha 
void handleDownloadFile(int newsockfd, vector<string>& tokens, string& clientName){
    // download_file <group_id> <file_name> <destination_path>
    if(tokens.size() != 4){
        writeToClient(newsockfd, "Usage: download_file <group_id> <file_name> <destination_path>");
        return;
    }
    
    if(clientName.empty()){
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    string fileName = tokens[2];
    string destPath = tokens[3];
    
    // Check if group exists
    if(groups.find(groupId) == groups.end()){
        writeToClient(newsockfd, "Group doesn't exist");
        return;
    }
    
    // Check if user is member of group
    Group* g = groups[groupId];
    if(!g->checkUserExistance(clientName)){
        writeToClient(newsockfd, "You are not a member of this group");
        return;
    }
    
    // Check if file exists in group
    FileInfo* fileInfo = g->getFileInfo(fileName);
    if(fileInfo == nullptr){
        writeToClient(newsockfd, "File not found in group");
        return;
    }
    
    // Send file metadata to client for download -> i have to include the code for seeders and leechers - -> then share the seeder info !
    // there was an update where whenever a piece is completely available after download, the 
    // string response = "FILE_META|" + fileName + "|" + to_string(fileInfo->fileSize) + "|" + fileInfo->fullFileSHA1 + "|" + to_string(fileInfo->pieces.size());
    string response = "FILE_META|" + fileName + "|" + to_string(fileInfo->fileSize) + 
                 "|" + fileInfo->fullFileSHA1 + "|" + to_string(fileInfo->pieces.size()) +
                 "|" + groupId + "|" + destPath;

    for(auto& piece : fileInfo->pieces){
        response += "|" + to_string(piece.pieceIndex) + ":" + piece.sha1Hash + ":";
        // index:hash:ip:port;ip:port;ip:port
        // Add seeder list for this specific piece
        for(int i = 0; i < piece.seeders.size(); i++){
            response += piece.seeders[i].second + ":" + to_string(piece.seeders[i].first);
            if(i < piece.seeders.size() - 1)
                response += ";";
            // else{
            //     response += "|";
            // }
        }
    }
    
    // cout << response << endl;

    writeToClient(newsockfd, response); // this has to be intercepted by the client -> in the previous commands intercepted there were only message showing
    // need to add command reading capabilities to the client too .... Ahhh !! here we go again
}

void handleStopShare(int newsockfd, vector<string>& tokens, string& clientName){
    // stop_share <group_id> <file_name>
    if(tokens.size() != 3){
        writeToClient(newsockfd, "Usage: stop_share <group_id> <file_name>");
        return;
    }
    
    if(clientName.empty()){
        writeToClient(newsockfd, "No user is logged in!");
        return;
    }
    
    string groupId = tokens[1];
    string fileName = tokens[2];
    
    // Check if group exists
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
    
    // Check if user is the uploader
    if(fileInfo->uploaderId != clientName){
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
