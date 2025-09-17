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
int currentTrackerNo = 0;

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
    currentTracker = trackers[0]; // initially the first tracker is the primary tracekr
    


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
    servAddr.sin_port = htons(tracker.second);
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
    string msg = operation + "|" + data;
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
    if(operation == "CREATE_USER"){
        User* u = new User(tokens[0], tokens[1]);
        users[tokens[0]] = u;
        //debug -
        cout << "sync : " << operation << data << endl;
    }else if(operation == "LOGIN"){
        User *u = users[tokens[1]];
        loggedInUsers[tokens[1]] = u;
        cout << "sync : " << operation << data << endl;
    }else if(operation == "CREATE_GROUP"){
        // for this comamnd i am passing - create_group <group id> <owner id>
        User *u = users[tokens[2]];
        Group *g = new Group(tokens[1]);
        g->addOwner(u->getUserId());
        groups[tokens[1]] = g;
    }else if(operation == "JOIN_GROUP"){
        // for this comamnd i am passing - join_group <group id> <owner id>
        User *u = users[tokens[2]];
        Group *g = groups[tokens[1]];
        g->addRequest(u->getUserId());
    }else if(operation == "LEAVE_GROUP"){
        // for this comamnd i am passing - leave_group <group id> <owner id>
        User *u = users[tokens[2]];
        Group *g = groups[tokens[1]];
        g->removeUser(u->getUserId());
    }else if(operation == "ACCEPT_REQUEST"){
        Group *g = groups[tokens[1]];
        g->acceptRequest(tokens[2]);
    }

    cout << "sync : " << operation << data << endl;

}


void* syncHandler(void* arg) {
    int sync_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (sync_socket < 0) {
        perror("sync socket");
        return NULL;
    }
    
    struct sockaddr_in sync_addr;
    sync_addr.sin_family = AF_INET;
    sync_addr.sin_addr.s_addr = INADDR_ANY;
    sync_addr.sin_port = htons(currentTracker.second + 1000);
    
    // yeh part of the code is optional -> sometimes hame msg aata hai ki port is already in use -> this part ensures that that doesnt happen !
    int opt = 1;
    setsockopt(sync_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(sync_socket, (struct sockaddr*)&sync_addr, sizeof(sync_addr)) < 0) {
        perror("sync bind");
        close(sync_socket);
        return NULL;
    }
    
    listen(sync_socket, 5);
    //debug -  cout << "Sync handler listening on port " << currentTracker.second + 1000 << endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(sync_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) continue;
        
        char buffer[1024];
        int n = read(client_sock, buffer, sizeof(buffer) - 1);
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
        close(client_sock);
    }
    
    close(sync_socket);
    return NULL;
}

void* healthChecker(void* arg) {

    while (true) {
        sleep(10); // check status every 10 sec

        vector<pair<string, int>> activeTrackers;
        
        for (const auto& tracker : trackers) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) continue;
            
            sockaddr_in servAddr;
            servAddr.sin_family = AF_INET;
            servAddr.sin_port = htons(tracker.second);
            inet_pton(AF_INET, tracker.first.c_str(), &servAddr.sin_addr);
            
            // this sets a timeout -> agar 2 sec ke under connect nhi hua toh forever wait nahi karega
            timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            /////////////////////////////

            // 2 sec mei reply nahi aaya toh - tata tata bye bye
            if(connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == 0) {
                activeTrackers.push_back(tracker);
            }
            close(sockfd);
        }
        
        trackers = activeTrackers;
    }
    return NULL;
}




#endif // SYNCHRONIZE_H
