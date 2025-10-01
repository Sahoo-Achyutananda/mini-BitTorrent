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

// fucntion to take tracker info from the tracker_info.txt file
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
    // currentTracker = trackers[0]; // initially the first tracker is the primary tracekr
    
    file.close();
}

void sendSyncInfo(pair<string,int> tracker ,string message){
    int sockfd = socket(AF_INET, SOCK_STREAM,0);

    if(sockfd < 0){
        perror("sockfd");
        return;
    }

    sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(tracker.second + 1000);
    inet_pton(AF_INET, tracker.first.c_str(), &servAddr.sin_addr);

    if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0){
        write(sockfd, message.c_str(), message.length());
    }else{
        // perror("connect");
        // return;
        pthread_mutex_lock(&queueLock);
        pendingMessages.push(message);
        pthread_mutex_unlock(&queueLock);
    }
    
    close(sockfd);
}   


void syncMessageHelper(string operation, string data){

    if(isPrimary == false){ // sirf primary sync msg bhej sakta
        return;
    }

    string msg = operation + "|" + data;

    // debug - 
    cout << "[SYNC OUT] " << msg << endl;

    for(auto &tracker : trackers){
        // do not send sync messgae to the current tracker
        if(tracker.second == currentTracker.second){
            continue;
        }else{
            sendSyncInfo(tracker, msg);
        }
    }
}

// this just parses the sync messages and updates the other trackers - 
void processSyncMessage(string operation, string data){
    vector<string> tokens = tokenizeString(data);
    // debug -
    for(auto &str : tokens){
        cout << str << " ";
    }
    cout << endl;

    pthread_mutex_lock(&dsLock); // protect Data structuressssssssssss // added this coz i think it was leading to seg fault

    if(operation == "CREATE_USER"){
        if(tokens.size() == 2){
            if(users.find(tokens[0]) == users.end()){
                User* u = new User(tokens[0], tokens[1]);
                users[tokens[0]] = u;
            }
        }
    }else if(operation == "LOGIN"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end()){
            User *u = users[tokens[1]];
            loggedInUsers[tokens[1]] = u;
        }
    }else if(operation == "CREATE_GROUP"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end()){
            User *u = users[tokens[1]];
            Group *g = new Group(tokens[0]);
            g->addOwner(u->getUserId());
            groups[tokens[0]] = g;
        }else{
            cout << "something went wrong" << endl;
        }
    }else if(operation == "JOIN_GROUP"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end() && groups.find(tokens[0]) != groups.end()){
            Group *g = groups[tokens[0]];
            g->addRequest(tokens[1]);
        }
    }else if(operation == "LEAVE_GROUP"){
        if(tokens.size() == 2 && users.find(tokens[1]) != users.end() && groups.find(tokens[0]) != groups.end()){
            Group *g = groups[tokens[0]];
            g->removeUser(tokens[1]);
        }
    }else if(operation == "ACCEPT_REQUEST"){
        if(tokens.size() == 2 && groups.find(tokens[0]) != groups.end()){
            Group *g = groups[tokens[0]];
            g->acceptRequest(tokens[1]);
        }
    }else if(operation == "UPLOAD_FILE"){

        if(tokens.size() >= 6){
            string groupId = tokens[0];
            string fileName = tokens[1];
            string filePath = tokens[2];
            long long fileSize = stoll(tokens[3]);
            string uploaderId = tokens[4];
            string fullFileSHA1 = tokens[5];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                
                // Create file info (without calculating pieces since we don't have the actual file)
                FileInfo* fileInfo = new FileInfo(fileName, filePath, fileSize, uploaderId, groupId);
                fileInfo->fullFileSHA1 = fullFileSHA1;
                
                // Add to group and global storage
                g->addSharedFile(fileName, fileInfo);
                allFiles[fileName] = fileInfo;
                
                cout << "[SYNC] File uploaded: " << fileName << " in group " << groupId << endl;
            }
        }
    }
    else if(operation == "STOP_SHARE"){
        // Format: groupId fileName uploaderId
        if(tokens.size() >= 3){
            string groupId = tokens[0];
            string fileName = tokens[1];
            string uploaderId = tokens[2];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                g->removeSharedFile(fileName);
                allFiles.erase(fileName);
                
                cout << "[SYNC] File sharing stopped: " << fileName << " in group " << groupId << endl;
            }
        }
    }
    else if(operation == "UPLOAD_FILE"){
        // Format jo bhejte hai : groupId fileName filePath fileSize uploaderId fullFileSHA1 clientPort clientIP piece0:hash piece1:hash ...
        if(tokens.size() >= 8){
            string groupId = tokens[0];
            string fileName = tokens[1];
            string filePath = tokens[2];
            long long fileSize = stoll(tokens[3]);
            string uploaderId = tokens[4];
            string fullFileSHA1 = tokens[5];
            int clientPort = stoi(tokens[6]);
            string clientIP = tokens[7];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                
                FileInfo* fileInfo = new FileInfo(fileName, filePath, fileSize, uploaderId, groupId);
                fileInfo->fullFileSHA1 = fullFileSHA1;
                
                // Parse and add piece information
                for(int i = 8; i < tokens.size(); i++){
                    string pieceStr = tokens[i];
                    int pos = pieceStr.find(':');
                    if(pos != string::npos){
                        int index = stoi(pieceStr.substr(0, pos));
                        string sha = pieceStr.substr(pos + 1);
                        
                        FilePiece piece(index, sha);
                        piece.isAvailable = true;
                        piece.seeders.push_back({clientPort, clientIP}); // Add initial seeder
                        
                        fileInfo->pieces.push_back(piece);
                    }
                }
                
                fileInfo->addSeeder(clientPort, clientIP);
                
                // Add to group and global storage
                g->addSharedFile(fileName, fileInfo);
                allFiles[fileName] = fileInfo;
                
                cout << "[SYNC IN] File uploaded: " << fileName << " in group " << groupId << " with " << fileInfo->pieces.size() << " pieces" << endl; // debug purpose only
            }
        }
    }
    else if(operation == "REMOVE_SEEDER"){
        // Format: groupId fileName clientIP clientPort
        if(tokens.size() == 4){
            string groupId = tokens[0];
            string fileName = tokens[1];
            string clientIP = tokens[2];
            int clientPort = stoi(tokens[3]);
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                FileInfo* fileInfo = g->getFileInfo(fileName);
                
                if(fileInfo){
                    // Remove from all pieces
                    for(auto& piece : fileInfo->pieces){
                        for(auto it = piece.seeders.begin(); it != piece.seeders.end(); ){
                            if(it->second == clientIP && it->first == clientPort){
                                it = piece.seeders.erase(it);
                            } else {
                                ++it;
                            }
                        }
                    }
                    
                    // Remove from main seeder list
                    for(auto it = fileInfo->seeders.begin(); it != fileInfo->seeders.end(); ){
                        if(it->second == clientIP && it->first == clientPort){
                            it = fileInfo->seeders.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    
                    cout << "[SYNC] Removed seeder from " << fileName << endl;
                }
            }
        }
    }
    else if(operation == "REMOVE_FILE"){
        // Format: groupId fileName
        if(tokens.size() == 2){
            string groupId = tokens[0];
            string fileName = tokens[1];
            
            if(groups.find(groupId) != groups.end()){
                Group* g = groups[groupId];
                g->removeSharedFile(fileName);
                allFiles.erase(fileName);
                
                cout << "[SYNC] Removed file: " << fileName << endl;
            }
        }
    }
    pthread_mutex_unlock(&dsLock);
    cout << "[SYNC IN] " << operation << " " << data << endl;
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
    
    // yeh part of the code is optional -> sometimes hame msg aata hai ki port is already in use -> this part ensures that that doesnt happen !
    int opt = 1; // -> it just sets it to true !
    setsockopt(syncSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //////////////////////////////////////

    if (bind(syncSocket, (struct sockaddr*)&syncAddr, sizeof(syncAddr)) < 0){
        perror("bind");
        close(syncSocket);
        return NULL;
    }
    
    listen(syncSocket, 5);
    //debug -  cout << "Sync handler listening on port " << currentTracker.second + 1000 << endl;
    
    while (true){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int clientSock = accept(syncSocket, (struct sockaddr*)&client_addr, &client_len);
        
        if (clientSock < 0) continue;
        
        char buffer[1024];
        int n = read(clientSock, buffer, sizeof(buffer) - 1);
        if (n > 0){
            buffer[n] = '\0';
            string message(buffer);
            
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
        if (tracker.second == trackers[0].second){ // I Assuming tracker 0 is designated primary
            return true;
        }
    }
    return false;
}

void* healthChecker(void* arg){

    while(1){
        sleep(5); // check status every 5 sec

        vector<pair<string, int>> actTrack;
        
        for (const auto& tracker : trackers){
            if (tracker == currentTracker) continue; // fixed the ghost connection issue LOL !
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) continue;
            
            sockaddr_in servAddr;
            servAddr.sin_family = AF_INET;
            servAddr.sin_port = htons(tracker.second + 1000); // this was the cause of the seg fault bug lol
            inet_pton(AF_INET, tracker.first.c_str(), &servAddr.sin_addr);
            
            // this sets a timeout -> agar 2 sec ke under connect nhi hua toh forever wait nahi karega
            timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            /////////////////////////////

            // 2 sec mei reply nahi aaya toh - tata tata bye bye
            if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0){
                actTrack.push_back(tracker);
            }
            close(sockfd);
        }
        
        activeTrackers = actTrack;

        // debug - showing actiev trackers in a line -
        // cout << "Active trackers: ";
        // for(auto t : activeTrackers){
        //     cout << t.first << " " << t.second << " ";
        // }
        // currentTracker = activeTrackers[0];

        if(isPrimary){
            continue;
        }else{
            if(activeTrackers.empty()|| !containsPrimary(activeTrackers)){
                isPrimary = true;
                cout << "Promoted to Primary Tracker" << endl;
            }
        }
    }
    return NULL;
}

void* messageFlusher(void* arg){
    while (true){
        sleep(5); // retry every 5 sec

        if (pendingMessages.empty() || activeTrackers.empty()){ // prelim check 
            pthread_mutex_unlock(&queueLock);
            continue;
        }

        pthread_mutex_lock(&queueLock);
        int qSize = pendingMessages.size();
        pthread_mutex_unlock(&queueLock);

        if (qSize == 0) continue;

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
                write(sockfd, msg.c_str(), msg.length());
                cout << "[FLUSHED] Sent queued message: " << msg << endl;
            } else {
                temp.push(msg); // phirse fail hua toh keep it in the queue !
            }
            close(sockfd);
        }
        pendingMessages = temp;
        pthread_mutex_unlock(&queueLock);
    }
    return NULL;
}

#endif // SYNCHRONIZE_H
