#if !defined(SYNCHRONIZE_H)
#define SYNCHRONIZE_H

#include "constructs.h"
#include<bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

vector<pair<string, int>> trackers;
pair<string,int> currentTracker;
vector<pair<string,int>> activeTrackers;
int currentTrackerNo = 0;

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

    if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0) {
        write(sockfd, message.c_str(), message.length());
    }else{
        perror("connect");
        return;
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

    pthread_mutex_lock(&dsLock); // protect Data structuressssssssssss // added this coz i think it was leading to seg fault

    if(operation == "CREATE_USER"){
        if(tokens.size() >= 2){
            if(users.find(tokens[0]) == users.end()){
                User* u = new User(tokens[0], tokens[1]);
                users[tokens[0]] = u;
            }
        }
    }else if(operation == "LOGIN"){
        if(tokens.size() >= 2 && users.find(tokens[1]) != users.end()){
            User *u = users[tokens[1]];
            loggedInUsers[tokens[1]] = u;
        }
    }else if(operation == "CREATE_GROUP"){
        if(tokens.size() >= 3 && users.find(tokens[2]) != users.end()){
            User *u = users[tokens[2]];
            Group *g = new Group(tokens[1]);
            g->addOwner(u->getUserId());
            groups[tokens[1]] = g;
        }
    }else if(operation == "JOIN_GROUP"){
        if(tokens.size() >= 3 && users.find(tokens[2]) != users.end() && groups.find(tokens[1]) != groups.end()){
            Group *g = groups[tokens[1]];
            g->addRequest(tokens[2]);
        }
    }else if(operation == "LEAVE_GROUP"){
        if(tokens.size() >= 3 && users.find(tokens[2]) != users.end() && groups.find(tokens[1]) != groups.end()){
            Group *g = groups[tokens[1]];
            g->removeUser(tokens[2]);
        }
    }else if(operation == "ACCEPT_REQUEST"){
        if(tokens.size() >= 3 && groups.find(tokens[1]) != groups.end()){
            Group *g = groups[tokens[1]];
            g->acceptRequest(tokens[2]);
        }
    }

    pthread_mutex_unlock(&dsLock);

    cout << "[SYNC IN] " << operation << " " << data << endl;
}


void* syncHandler(void* arg) {
    int syncSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (syncSocket < 0) {
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
    
    if (bind(syncSocket, (struct sockaddr*)&syncAddr, sizeof(syncAddr)) < 0) {
        perror("bind");
        close(syncSocket);
        return NULL;
    }
    
    listen(syncSocket, 5);
    //debug -  cout << "Sync handler listening on port " << currentTracker.second + 1000 << endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int clientSock = accept(syncSocket, (struct sockaddr*)&client_addr, &client_len);
        
        if (clientSock < 0) continue;
        
        char buffer[1024];
        int n = read(clientSock, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            string message(buffer);
            
            int pos = message.find('|');
            if (pos != string::npos) {
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

bool containsPrimary(const vector<pair<string, int>>& activeTrackers) {
    for (const auto& tracker : activeTrackers) {
        if (tracker.second == trackers[0].second) { // I Assuming tracker 0 is designated primary
            return true;
        }
    }
    return false;
}

void* healthChecker(void* arg) {

    while(1){
        sleep(10); // check status every 10 sec

        vector<pair<string, int>> actTrack;
        
        for (const auto& tracker : trackers) {
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
            if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0) {
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




#endif // SYNCHRONIZE_H
