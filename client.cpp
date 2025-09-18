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

#include "colors.h"

using namespace std;

// remember the process -
/*
1. client ko bind karne ki zaroorat nahi hai -> it has to directly connect !
2. 
*/

vector<pair<string,int>> trackers;

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

// just the same as before lol
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


int main(int argc, char *argv[]){

    if (argc < 3) {
        cerr << fontBold << colorRed << "Usage: " << argv[0] << " client_ip:port tracker_info.txt" << reset << endl;
        return 1;
    }

    pair<string,int> clientInfo = parseIpPort(argv[1]);
    string clientIp = clientInfo.first;
    int clientPort = clientInfo.second;

    cout << fontBold << colorGreen  << "Client running with IP " << clientIp << " and port " << clientPort << reset << endl;

    int sockfd, n;
    sockaddr_in serv_addr;
    hostent *server;

    char buffer[256];

    parseTrackerInfoFile(string(argv[2]));
    int trackerIndex = 0;

    //debug - 
    for(auto t : trackers){
        cout << t.first << " " << t.second << endl;
    }

    while(trackerIndex < trackers.size()){
        string ipadr = trackers[trackerIndex].first;
        int portno = trackers[trackerIndex].second;

        cout << fontBold << colorGreen << "Trying tracker " << ipadr << ":" << portno << reset << endl;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0){
            perror("socket");
            return 1;
        }

        // server = gethostbyname(argv[1]); // old code
        // if(server == NULL){
        //     perror("gethostbyname");
        //     return 1;
        // }

        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port   = htons(portno);

        if (inet_pton(AF_INET, ipadr.c_str(), &serv_addr.sin_addr) <= 0) {
            perror("Invalid address/ Address not supported");
            close(sockfd);
            trackerIndex++;
            continue;
        }

        if(connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
            perror("connect");
            close(sockfd);
            trackerIndex++;
            continue;
        }

        cout << fontBold << colorGreen << "Connected to tracker " << ipadr << ":" << portno << endl;
        while(1){
            bzero(buffer, 255);
            fgets(buffer, 255, stdin);
            n = write(sockfd, buffer, strlen(buffer));
            if(n < 0){
                perror("write");
                close(sockfd);
                trackerIndex++;
                break;
            }
            bzero(buffer, 255);

            // a small piece of code to read data from the server - 
            n = read(sockfd, buffer, 255);
            if(n < 0){
                perror("read");
                close(sockfd);
                trackerIndex++;
                break;
            }

            printf("Server : %s", buffer);
            
        }
    }

    close(sockfd);
    return 0;
}