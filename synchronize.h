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

// fucntion to take tracker info from the tracker_info.txt file
void parseTrackerInfo(string fileName){
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

void 





#endif // SYNCHRONIZE_H
