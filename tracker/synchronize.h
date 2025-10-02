#if !defined(SYNCHRONIZE_H)
#define SYNCHRONIZE_H

#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;

#include "constructs.h"

vector<pair<string, int>> trackers;
pair<string,int> currentTracker;
vector<pair<string,int>> activeTrackers;
int currentTrackerNo = 0;

queue<string> pendingMessages;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t dsLock = PTHREAD_MUTEX_INITIALIZER;

bool isPrimary = false;

void parseTrackerInfoFile(string fileName){
    ifstream file(fileName);
    string l;

    while(getline(file,l)){
        int pos = l.find(':');
        if(pos != string::npos){
            string ip = l.substr(0,pos);
            int port = stoi(l.substr(pos+1));
            trackers.push_back({ip,port});
        }
    }
    file.close();
}

void sendSyncInfo(pair<string,int> tracker, string message){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        pthread_mutex_lock(&queueLock);
        pendingMessages.push(message);
        pthread_mutex_unlock(&queueLock);
        return;
    }

    // added some timeout to prevent hanging
    timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(tracker.second + 1000);
    inet_pton(AF_INET, tracker.first.c_str(), &servAddr.sin_addr);

    if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0){
        message += "\n";
        int result = write(sockfd, message.c_str(), message.length());
        if(result < 0){
            pthread_mutex_lock(&queueLock);
            pendingMessages.push(message);
            pthread_mutex_unlock(&queueLock);
        }
    } else {
        pthread_mutex_lock(&queueLock);
        pendingMessages.push(message);
        pthread_mutex_unlock(&queueLock);
    }
    
    close(sockfd);
}

void syncMessageHelper(string operation, string data){
    if(isPrimary == false){
        return;
    }

    string msg = operation + "|" + data;

    if(operation == "UPLOAD_FILE"){
        cout << "[SYNC OUT] " << operation << " " << data.substr(0,100) << "and more ..." << endl;
    } else if(operation == "PIECE_COMPLETED"){
        // silent
    } else {
        cout << "[SYNC OUT] " << msg << endl;
    }

    for(auto &tracker : trackers){
        if(tracker.second == currentTracker.second){
            continue;
        }
        sendSyncInfo(tracker, msg);
    }
}

void processSyncMessage(string operation, string data){
    vector<string> tokens = tokenizeString(data);

    pthread_mutex_lock(&dsLock);

    if(operation == "CREATE_USER"){
        if(tokens.size() == 2){
            if(users.find(tokens[0]) == users.end()){
                User* u = new User(tokens[0], tokens[1]);
                users[tokens[0]] = u;
            }
        }
    }
    else if(operation == "LOGIN"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end()){
            User *u = users[tokens[1]];
            loggedInUsers[tokens[1]] = u;
        }
    }
    else if(operation == "LOGOUT"){
        if(tokens.size() == 1){
            string userId = tokens[0];
            loggedInUsers.erase(userId);
            
            // Remove as seeder from all files
            for(auto& [fileName, fileInfo] : allFiles){
                fileInfo->removeSeederByUser(userId);
            }
            
            // Clean up empty files
            for(auto it = allFiles.begin(); it != allFiles.end(); ){
                if(it->second->seeders.empty()){
                    if(groups.find(it->second->groupId) != groups.end()){
                        groups[it->second->groupId]->removeSharedFile(it->first);
                    }
                    delete it->second;
                    it = allFiles.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    else if(operation == "CREATE_GROUP"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end()){
            User *u = users[tokens[1]];
            Group *g = new Group(tokens[0]);
            g->addOwner(u->getUserId());
            groups[tokens[0]] = g;
        }
    }
    else if(operation == "JOIN_GROUP"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end() && groups.find(tokens[0]) != groups.end()){
            Group *g = groups[tokens[0]];
            g->addRequest(tokens[1]);
        }
    }
    else if(operation == "LEAVE_GROUP"){
        if(tokens.size() == 2){
            string groupId = tokens[0];
            string userId = tokens[1];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                
                // Remove user as seeder from all files in this group
                vector<string> filesToRemove;
                for(auto& [fileName, fileInfo] : allFiles){
                    if(fileInfo->groupId == groupId){
                        fileInfo->removeSeederByUser(userId);
                        
                        if(fileInfo->seeders.empty()){
                            filesToRemove.push_back(fileName);
                        }
                    }
                }
                
                // Clean up files with no seeders
                for(const string& fileName : filesToRemove){
                    g->removeSharedFile(fileName);
                    delete allFiles[fileName];
                    allFiles.erase(fileName);
                }
                
                g->removeUser(userId);
            }
        }
    }
    else if(operation == "ACCEPT_REQUEST"){
        if(tokens.size() == 2 && groups.find(tokens[0]) != groups.end()){
            Group *g = groups[tokens[0]];
            g->acceptRequest(tokens[1]);
        }
    }
    else if(operation == "PIECE_COMPLETED"){
        if(tokens.size() == 6){
            string groupId = tokens[0];
            string fileName = tokens[1];
            int pieceIndex = stoi(tokens[2]);
            string userId = tokens[3];
            string clientIP = tokens[4];
            int clientPort = stoi(tokens[5]);
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                FileInfo* fileInfo = g->getFileInfo(fileName);
                
                if(fileInfo && pieceIndex >= 0 && pieceIndex < fileInfo->pieces.size()){
                    bool alreadySeeder = false;
                    for(const auto& seeder : fileInfo->pieces[pieceIndex].seeders){
                        if(seeder.userId == userId && seeder.ip == clientIP && seeder.port == clientPort){
                            alreadySeeder = true;
                            break;
                        }
                    }
                    
                    if(!alreadySeeder){
                        fileInfo->pieces[pieceIndex].seeders.push_back(
                            SeederInfo(userId, clientIP, clientPort)
                        );
                        fileInfo->pieces[pieceIndex].isAvailable = true;
                    }
                }
            }
        }
    }
    else if(operation == "UPLOAD_FILE"){
        if(tokens.size() >= 9){
            string groupId = tokens[0];
            string fileName = tokens[1];
            string filePath = tokens[2];
            long long fileSize = stoll(tokens[3]);
            string uploaderId = tokens[4];
            string fullFileSHA1 = tokens[5];
            string userId = tokens[6];
            int clientPort = stoi(tokens[7]);
            string clientIP = tokens[8];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                
                FileInfo* fileInfo = new FileInfo(fileName, filePath, fileSize, uploaderId, groupId);
                fileInfo->fullFileSHA1 = fullFileSHA1;
                
                for(int i = 9; i < tokens.size(); i++){
                    string pieceStr = tokens[i];
                    int pos = pieceStr.find(':');
                    if(pos != string::npos){
                        int index = stoi(pieceStr.substr(0, pos));
                        string sha = pieceStr.substr(pos + 1);
                        
                        FilePiece piece(index, sha);
                        piece.isAvailable = true;
                        piece.seeders.push_back(SeederInfo(userId, clientIP, clientPort));
                        
                        fileInfo->pieces.push_back(piece);
                    }
                }
                
                fileInfo->addSeeder(userId, clientIP, clientPort);
                g->addSharedFile(fileName, fileInfo);
                allFiles[fileName] = fileInfo;
            }
        }
    }
    else if(operation == "REMOVE_SEEDER"){
        if(tokens.size() == 5){
            string groupId = tokens[0];
            string fileName = tokens[1];
            string userId = tokens[2];
            string clientIP = tokens[3];
            int clientPort = stoi(tokens[4]);
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                FileInfo* fileInfo = g->getFileInfo(fileName);
                
                if(fileInfo){
                    for(auto& piece : fileInfo->pieces){
                        for(auto it = piece.seeders.begin(); it != piece.seeders.end(); ){
                            if(it->ip == clientIP && it->port == clientPort && it->userId == userId){
                                it = piece.seeders.erase(it);
                            } else {
                                ++it;
                            }
                        }
                    }
                    
                    for(auto it = fileInfo->seeders.begin(); it != fileInfo->seeders.end(); ){
                        if(it->ip == clientIP && it->port == clientPort && it->userId == userId){
                            it = fileInfo->seeders.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
    }
    else if(operation == "TRANSFER_OWNERSHIP"){
        if(tokens.size() == 3){
            string groupId = tokens[0];
            string newOwnerId = tokens[2];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                g->transferOwnership(newOwnerId);
            }
        }
    }
    else if(operation == "DELETE_GROUP"){
        if(tokens.size() == 1){
            string groupId = tokens[0];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                
                for(auto it = allFiles.begin(); it != allFiles.end(); ){
                    if(it->second->groupId == groupId){
                        delete it->second;
                        it = allFiles.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                delete g;
                groups.erase(groupId);
            }
        }
    }
    else if(operation == "REMOVE_FILE"){
        if(tokens.size() == 2){
            string groupId = tokens[0];
            string fileName = tokens[1];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                g->removeSharedFile(fileName);
                allFiles.erase(fileName);
            }
        }
    }
    
    pthread_mutex_unlock(&dsLock);
    
    if(operation != "PIECE_COMPLETED"){
        cout << "[SYNC IN] " << operation << " " << data << endl;
    }
}

void* syncHandler(void* arg){
    int syncSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (syncSocket < 0){
        perror("socket");
        return NULL;
    }
    
    sockaddr_in syncAddr;
    syncAddr.sin_family = AF_INET;
    syncAddr.sin_addr.s_addr = INADDR_ANY;
    syncAddr.sin_port = htons(currentTracker.second + 1000);
    
    int opt = 1;
    setsockopt(syncSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(syncSocket, (struct sockaddr*)&syncAddr, sizeof(syncAddr)) < 0){
        perror("bind");
        close(syncSocket);
        return NULL;
    }
    
    listen(syncSocket, 5);
    
    while (true){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int clientSock = accept(syncSocket, (struct sockaddr*)&client_addr, &client_len);
        
        if (clientSock < 0) continue;
        
        string message;
        char buffer[4096];
        
        while(true) {
            int n = read(clientSock, buffer, sizeof(buffer) - 1);
            if(n <= 0) break;
            
            buffer[n] = '\0';
            message.append(buffer, n);
            
            // Check if we got complete message (ends with \n)
            if(message.back() == '\n') {
                message.pop_back();  // Remove trailing \n
                break;
            }
        }
        
        if (!message.empty()){
            int pos = message.find('|');
            if (pos != string::npos){
                string operation = message.substr(0, pos);
                string data = message.substr(pos + 1);
                processSyncMessage(operation, data);
            }
        }
        close(clientSock);
    }
    
    close(syncSocket);
    return NULL;
}

bool containsPrimary(const vector<pair<string, int>>& activeTrackers){
    for (const auto& tracker : activeTrackers){
        if (tracker.second == trackers[0].second){
            return true;
        }
    }
    return false;
}

void* healthChecker(void* arg){
    while(1){
        sleep(5);

        vector<pair<string, int>> actTrack;
        
        for (const auto& tracker : trackers){
            if (tracker == currentTracker) continue;
            
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) continue;
            
            sockaddr_in servAddr;
            servAddr.sin_family = AF_INET;
            servAddr.sin_port = htons(tracker.second + 1000);
            inet_pton(AF_INET, tracker.first.c_str(), &servAddr.sin_addr);
            
            timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0){
                actTrack.push_back(tracker);
            }
            close(sockfd);
        }
        
        activeTrackers = actTrack;

        if(isPrimary){
            continue;
        } else {
            if(activeTrackers.empty() || !containsPrimary(activeTrackers)){
                isPrimary = true;
                cout << "Promoted to Primary Tracker" << endl;
            }
        }
    }
    return NULL;
}

void* messageFlusher(void* arg){
    while (true){
        sleep(5);

        pthread_mutex_lock(&queueLock);
        bool isEmpty = pendingMessages.empty();
        pthread_mutex_unlock(&queueLock);

        if (isEmpty || activeTrackers.empty()){
            continue;
        }

        pthread_mutex_lock(&queueLock);
        queue<string> temp;
        while (!pendingMessages.empty()){
            string msg = pendingMessages.front();
            pendingMessages.pop();

            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0){
                temp.push(msg);
                continue;
            }

            sockaddr_in servAddr;
            servAddr.sin_family = AF_INET;
            servAddr.sin_port = htons(activeTrackers[0].second + 1000);
            inet_pton(AF_INET, activeTrackers[0].first.c_str(), &servAddr.sin_addr);
            
            if (connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0){
                msg += "\n";
                write(sockfd, msg.c_str(), msg.length());
                cout << "[FLUSHED] Sent queued message: " << msg << endl;
            } else {
                temp.push(msg);
            }
            close(sockfd);
        }
        pendingMessages = temp;
        pthread_mutex_unlock(&queueLock);
    }
    return NULL;
}

#endif // SYNCHRONIZE_H