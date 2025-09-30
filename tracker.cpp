#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> // for inet_ntoa -> prints the ipv4 address of the client -> that's its use so far ! - 13sept
#include <pthread.h>

#include "colors.h"
#include "constructs.h"
#include "synchronize.h"
#include "sha.h"
#include "fileops.h"

// functions declarations -
void * handleConnections(void *arg);
void writeToClient(int sockfd, string msg);
void* handleTrackerCommands(void* arg);

bool shutdownServer = false;

int main(int argc, char *argv[]){
    // set<User> users;
    // set<Group> groups;
    if (argc != 3) {
        cerr << fontBold << colorRed << "Usage: " << argv[0] << " <port> <tracker_info_file> <tracker_id>" << reset << endl;
        return 1;
    }

    string trackerFileInfo = argv[1];
    currentTrackerNo = atoi(argv[2]);
    parseTrackerInfoFile(trackerFileInfo);

    isPrimary = (currentTrackerNo == 0);

    currentTracker = trackers[currentTrackerNo];

    pthread_t sync_thread, health_thread;
    pthread_create(&sync_thread, NULL, syncHandler, NULL);
    pthread_create(&health_thread, NULL, healthChecker, NULL);
    pthread_detach(sync_thread);
    pthread_detach(health_thread);

    int sockfd, newsockfd, portno, n;

    sockaddr_in servAddr ;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0){
        perror("socket");
        return 1;
    }

    bzero((char *)&servAddr, sizeof(servAddr));
    portno = currentTracker.second;

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(portno);

    if(bind(sockfd, (sockaddr *)&servAddr, sizeof(servAddr))< 0){
        perror("bind");
        return 1;
    }

    listen(sockfd, 5);
    cout << "Tracker Listening on Port No : " << portno << endl;
    
    pthread_t trackerThread;
    pthread_create(&trackerThread, NULL, handleTrackerCommands, NULL);
    pthread_detach(trackerThread);

    pthread_t flusherThread;
    pthread_create(&flusherThread, NULL, messageFlusher, NULL);
    pthread_detach(flusherThread);

    while(1){
        socklen_t clilen;
        sockaddr_in cliAddr;

        cout << "Tracker is running, waiting for requests ... " << endl;
        clilen = sizeof(cliAddr);
        newsockfd = accept(sockfd, (sockaddr *)&cliAddr, &clilen);

        if(newsockfd < 0){
            perror("accept");
            return 1;
        }
        cout << "Accepted Client with IP : " << inet_ntoa(cliAddr.sin_addr) << endl;
        
        pthread_t thread;
        int * newSock = new int(newsockfd);
        // pthread_create(&thread, NULL, handleConnections, NULL); this is confusing
        pthread_create(&thread, NULL, handleConnections, (void *)newSock);
        pthread_detach(thread);
    }

    close(newsockfd);
    close(sockfd);

    return 0;
}


void* handleTrackerCommands(void* arg) {
    string current_user; // Console's own session
    
    while (true) {
        string input;
        cin >> input;
        
        if (input.empty()) continue;
        
        vector<string> tokens = tokenizeString(input);
        
        if(tokens[0] == "list_users"){
            int n = users.size();
            for(auto &[userId, User] : users){
                cout << userId << endl;
            }
        }else if(tokens[0] == "user_count"){
            int n = users.size();
            cout << n << endl;
        }else if(tokens[0] == "list_loggedin_users"){
            int n = loggedInUsers.size();
            for(auto &[userId, User] : loggedInUsers){
                cout << userId << endl;
            }
        }else if(tokens[0] == "loggedin_user_count"){
            int n = loggedInUsers.size();
            cout << n << endl;
        }else if(tokens[0] == "list_groups"){
            int n = groups.size();
            for(auto&[groupId, Group] : groups){
                cout << groupId << endl;
            }
        }else if(tokens[0] == "list_group_details"){
            int n = groups.size();
            for(auto&[groupId, g] : groups){
                cout << groupId << " " << g->getGroupUserCount() << endl;
            }
        }
        else if(input == "quit"){ // there is a problem here
            // shutdownServer = true;
            cout << "Shutting Down" << endl;
            exit(0);
            // break;
        }
        else{
            continue;
        }   
    }

    return nullptr;
}

void *handleConnections(void *arg){
    int newsockfd = *(int *)arg; // typecasting it as an integer pointer and then dereferencing it !
    delete (int*)arg;

    string clientName;
    char buffer[255];
    int n;
    
    while(1){
        bzero(buffer, 255);
        n = read(newsockfd, buffer, 255);
        if(n < 0){
            perror("read");
            return NULL;
        }
        // debug -
        // printf("Client : %s", buffer);
        // ----------------------------------------
        // string cmd(buffer); // this gave me seg fault -> cause i was reading more than the actual bytes that was read
        string cmd(buffer, n);
        
        // debug -
        // cout << cmd << endl;
        vector<string> tokens = tokenizeString(cmd);
        // cout << tokens[0] << endl;

        if(tokens[0] == "create_user"){
            // cout << "Client wants to create a new user !!! " << endl;
            if(tokens.size() != 3){
                // cerr << fontBold << colorRed << "Usage : create_user <user_name> <password>" << reset << endl;
                writeToClient(newsockfd, "Usage : create_user <user_name> <password>");
            }else{
                //creating a new user only if userName is unique !
                if(users.find(tokens[1]) == users.end()){
                    User *u = new User(tokens[1], tokens[2]);
                    users[tokens[1]] = u;
                    cout << "user created" << endl;
                    writeToClient(newsockfd, "User created successfully");
                    syncMessageHelper("CREATE_USER", tokens[1] + " " + tokens[2]);
                }else{
                    // cerr << fontBold << colorRed << "UserID already exists" << reset << endl;
                    writeToClient(newsockfd, "UserID already exists");
                }
            }
        }
        else if(tokens[0] == "login"){
            if(tokens.size() != 3){
                // cerr << fontBold << colorRed << "Usage : login <user_name> <password>" << reset << endl;
                writeToClient(newsockfd, "Usage : login <user_name> <password>");
            }else{
                if(users.find(tokens[1]) == users.end()){
                    // cerr << fontBold << colorRed << "UserName doesnot exists" << reset << endl;
                    writeToClient(newsockfd, "UserID doesnt exist");
                }else{
                    User *u = users[tokens[1]];
                    if(u->getPassword() != tokens[2]){
                        // cerr << fontBold << colorRed << "Password is incorrect" << reset << endl;
                        writeToClient(newsockfd, "Password is incorrect");
                    }
                    loggedInUsers[tokens[1]] = u;
                    clientName = tokens[1];
                    syncMessageHelper("LOGIN", tokens[1] + " " + tokens[2]);
                    cout << fontBold << colorGreen << "Login successful !" << clientName << reset << endl;
                    writeToClient(newsockfd, "Login successful");
                }
            }
        }else if(tokens[0] == "create_group"){
            if(tokens.size() != 2){
                cerr << fontBold << colorRed << "Usage : create_group <group_id>" << reset << endl;
            }else{
                if(clientName.empty()){
                    // cout << clientName << endl; // debug
                    // cerr << fontBold << colorRed << "No user is logged-in" << reset << endl;
                    writeToClient(newsockfd, "No user is Logged In !");
                }else{
                    // fetch the logged in user data -
                    User *u = users[clientName];
                    // create a new group -
                    Group *g = new Group(tokens[1]);
                    // adding a logged in user as a owner
                    g->addOwner(u->getUserId());
                    groups[tokens[1]] = g;
                    syncMessageHelper("CREATE_GROUP", tokens[1] + " " + clientName);
                    cout << fontBold << colorGreen << "Group Created successfully with - groupId " << tokens[1] << reset << endl;
                    writeToClient(newsockfd, "Group Created Successfully !");
                }
            }
        }else if(tokens[0] == "join_group"){
            if(tokens.size() != 2){
                cerr << fontBold << colorRed << "Usage : join_group <group_id>" << reset << endl;
            }else{
                if(clientName == ""){
                    cerr << fontBold << colorRed << "No user is logged-in" << reset << endl;
                }else{
                    // fetch the logged in user data -
                    User *u = users[clientName];
                    // get group -
                    Group *g = groups[tokens[1]];

                    if(g->checkUserExistance(clientName)){
                        // cerr << fontBold << colorRed << "User already exists in the group !" << reset << endl;
                        writeToClient(newsockfd, "User already exists in the group !");
                    }else{
                        // add the logged in user to the group
                        g->addRequest(u->getUserId());
                        syncMessageHelper("JOIN_GROUP", tokens[1] + " " + clientName);
                        cout << fontBold << colorGreen << clientName << " requested to join " << tokens[1] << reset << endl;
                        writeToClient(newsockfd, "Successfully Requested ! ");
                    }
                }
            }
        }else if(tokens[0] == "leave_group"){
            if(tokens.size() != 2){
                // cerr << fontBold << colorRed << "Usage : leave_group <group_id>" << reset << endl;
                writeToClient( newsockfd,"Usage : leave_group <group_id>");
            }else{
                if(clientName == ""){
                    // cerr << fontBold << colorRed << "No user is logged-in" << reset << endl;
                    writeToClient( newsockfd, "No user is logged in !");
                }else{
                    // fetch the logged in user data -
                    User *u = users[clientName];
                    // get group -
                    Group *g = groups[tokens[1]];

                    g->removeUser(u->getUserId());
                    syncMessageHelper("LEAVE_GROUP", tokens[1] + " " + clientName);
                    cout << fontBold << colorGreen << clientName << "successfully Removed from group" << tokens[1] << reset << endl;
                    writeToClient(newsockfd, "Successfully Removed from Group !");
                }
            }
        }else if(tokens[0] == "list_groups"){
            int n = groups.size();
            if(n == 0){
                writeToClient(newsockfd, "No Groups Exists !");
            }else{
                string t = "";
                for(auto&[groupId, Group] : groups){
                    t += groupId;
                    t += '\n';
                }
                writeToClient(newsockfd, t);
            }
        }else if(tokens[0] == "list_requests"){
            // yeh sirf group ka owner execute kar sakta hai ! so i check if the client ie userId is a groupOwner or not !
            if(groupOwners.find(clientName) == groupOwners.end()){
                writeToClient(newsockfd, "This is a priviledged command & You are not a Group Owner");
            }
            else{
                if(tokens.size() != 2){
                    // cerr << fontBold << colorRed << "Usage : list_requests <group_id>" << reset << endl;
                    writeToClient(newsockfd, "Usage : list_requests <group_id>");
                }else{
                    if(groups.find(tokens[1]) == groups.end()){
                        writeToClient(newsockfd, "Group doesnt exist");
                    }else{
                        Group *g = groups[tokens[1]];
                        vector<string> res = g->getRequests();
                        for(auto str : res){
                            writeToClient(newsockfd, str);
                        }
                    }
                }
            }
        }else if(tokens[0] == "accept_request"){
            // accept_request <groupid> <userid>
            // cout << "debug : " << tokens[0] << " " << tokens[1] << " " << tokens[2] << endl;
            if(tokens.size() != 3){
                writeToClient(newsockfd, "Usage : accept_request <groupid> <userid>");
            }else{
                if(isGroup(tokens[1]) == false){
                    writeToClient(newsockfd, "Group Id doesnt exist !");
                }else if(isUser(tokens[2]) == false){
                    writeToClient(newsockfd, "User does not exist");
                }else{
                    if(isGroupOwner(clientName)){
                        Group *g = groups[tokens[1]];
                        g->acceptRequest(tokens[2]);
                        syncMessageHelper("ACCEPT_REQUEST", tokens[1] + " " + tokens[2]);
                        writeToClient(newsockfd, "Request Accepted");
                    }else{
                        writeToClient(newsockfd, "This is a priviledged instruction - You are not an owner.");
                    }
                }
            }
        }else if(tokens[0] == "whoami"){
            // for debug purpose only
            if(clientName.empty()) writeToClient(newsockfd, "LOGIN NAME NOT REGISTERED !");
            writeToClient(newsockfd, clientName);
        }else if(tokens[0] == "upload_file"){
            handleUploadFileTracker(newsockfd, tokens, clientName);
        }
        else if(tokens[0] == "list_files"){
            handleListFiles(newsockfd, tokens, clientName);
        }
        else if(tokens[0] == "download_file"){
            handleDownloadFile(newsockfd, tokens, clientName);
        }
        else if(tokens[0] == "stop_share"){
            handleStopShare(newsockfd, tokens, clientName);
        }
        else if(tokens[0] == "piece_completed"){
            handlePieceCompleted(newsockfd, tokens, clientName);
        }
        else{
            writeToClient(newsockfd, "Invalid Command ... \nValid Commands :\n1. create_user <userid> <password>\n2. login <userid> <password>");
        }  

        // -----------------------------------------
        
        // a small piece of code to write to the client - currently not using it -infact i built a new function
        // fgets(buffer, 255, stdin);

        // n = write(newsockfd, buffer, strlen(buffer));
        // if(n < 0){
        //     perror("write");
        //     return 1;
        // }
    }

    close(newsockfd);
    return NULL;
}