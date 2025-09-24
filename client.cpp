#include <iostream>
#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <pthread.h>

#include "colors.h"

using namespace std;

vector<pair<string,int>> trackers;
int trackerIndex = 0;
int sockfd = -1;
bool trackerAlive = false;
pthread_mutex_t tracker_mutex = PTHREAD_MUTEX_INITIALIZER;

// Parse tracker info file
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

pair<string,int> parseIpPort(string s) {
    int pos = s.find(':');
    if (pos == string::npos) {
        cerr << "Invalid ip:port format -> " << s << endl;
        exit(1);
    }
    string ip = s.substr(0, pos);
    int port = stoi(s.substr(pos + 1));
    return {ip, port};
}

// Heartbeat thread: checks tracker health via tracker_port + 1000
void* heartbeat(void* arg){
    while(true){
        sleep(1);
        pthread_mutex_lock(&tracker_mutex);
        if(trackerAlive && sockfd != -1){
            string ip = trackers[trackerIndex].first;
            int port = trackers[trackerIndex].second + 1000; // health port

            int healthSock = socket(AF_INET, SOCK_STREAM, 0);
            if(healthSock < 0){
                perror("Health socket");
                trackerAlive = false;
                close(sockfd);
                pthread_mutex_unlock(&tracker_mutex);
                continue;
            }

            sockaddr_in healthAddr{};
            healthAddr.sin_family = AF_INET;
            healthAddr.sin_port = htons(port);
            inet_pton(AF_INET, ip.c_str(), &healthAddr.sin_addr);

            if(connect(healthSock, (struct sockaddr*)&healthAddr, sizeof(healthAddr)) < 0){
                cout << fontBold << colorRed << "Tracker " << ip << ":" << port-1000 
                     << " down! Switching..." << reset << endl;
                trackerAlive = false;
                close(sockfd);
            }

            close(healthSock);
        }
        pthread_mutex_unlock(&tracker_mutex);
    }
    return nullptr;
}

int main(int argc, char *argv[]){

    if (argc < 3) {
        cerr << fontBold << colorRed << "Usage: " << argv[0] << " client_ip:port tracker_info.txt" << reset << endl;
        return 1;
    }

    pair<string,int> clientInfo = parseIpPort(argv[1]);
    cout << fontBold << colorGreen  << "Client running with IP " << clientInfo.first 
         << " and port " << clientInfo.second << reset << endl;

    parseTrackerInfoFile(string(argv[2]));

    // Start heartbeat thread
    pthread_t hbThread;
    pthread_create(&hbThread, nullptr, heartbeat, nullptr);
    pthread_detach(hbThread);

    while(trackerIndex < trackers.size()){
        string ipadr = trackers[trackerIndex].first;
        int portno = trackers[trackerIndex].second;

        cout << fontBold << colorGreen << "Trying tracker " << ipadr << ":" << portno << reset << endl;

        pthread_mutex_lock(&tracker_mutex);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0){
            perror("socket");
            pthread_mutex_unlock(&tracker_mutex);
            trackerIndex++;
            continue;
        }

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(portno);
        if(inet_pton(AF_INET, ipadr.c_str(), &serv_addr.sin_addr) <= 0){
            perror("Invalid address / not supported");
            close(sockfd);
            pthread_mutex_unlock(&tracker_mutex);
            trackerIndex++;
            continue;
        }

        if(connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
            perror("connect failed");
            close(sockfd);
            pthread_mutex_unlock(&tracker_mutex);
            trackerIndex++;
            continue;
        }

        cout << fontBold << colorGreen << "Connected to tracker " << ipadr << ":" << portno << reset << endl;
        trackerAlive = true;
        pthread_mutex_unlock(&tracker_mutex);

        // Main input loop
        char buffer[256];
        while(trackerAlive){
            if(!fgets(buffer, 256, stdin)){
                cout << "stdin error\n";
                break;
            }

            pthread_mutex_lock(&tracker_mutex);
            int n = write(sockfd, buffer, strlen(buffer));
            if(n <= 0){
                cout << "Write failed, switching tracker...\n";
                trackerAlive = false;
                close(sockfd);
            }
            pthread_mutex_unlock(&tracker_mutex);

            pthread_mutex_lock(&tracker_mutex);
            n = read(sockfd, buffer, 255);
            if(n <= 0){
                cout << "Tracker disconnected!\n";
                trackerAlive = false;
                close(sockfd);
            } else {
                buffer[n] = '\0';
                cout << fontBold << colorBlue << buffer << reset << endl;
            }
            pthread_mutex_unlock(&tracker_mutex);
        }

        trackerIndex++; // move to next tracker
    }

    cout << fontBold << colorRed << "No more trackers available. Exiting..." << reset << endl;
    return 0;
}
