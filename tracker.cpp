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

// functions declarations -
void * handleConnections(void *arg);
void writeToClient(int sockfd, string msg);
void* handleTrackerCommands(void* arg);
// bool checkUserStatus(string uid);
// bool checkUserLoginStatus(string uid);

vector<string> tokenizeString(string s){
    vector<string> result;
    stringstream stream(s);

    string temp;
    while(getline(stream, temp, ' ')){
        result.push_back(temp);
    }
    return result;
}

int main(int argc, char *argv[]){
    // set<User> users;
    // set<Group> groups;
    

    int sockfd, newsockfd, portno, n;
    

    sockaddr_in serv_addr ;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0){
        perror("socket");
        return 1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if(bind(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr))< 0){
        perror("bind");
        return 1;
    }

    listen(sockfd, 5);
    cout << "Tracker Listening on Port No : " << portno << endl;
    
    pthread_t trackerThread;
    pthread_create(&trackerThread, NULL, handleTrackerCommands, NULL);
    pthread_detach(trackerThread);

    while(1){
        socklen_t clilen;
        sockaddr_in cli_addr;

        cout << "Tracker is running, waiting for requests ... " << endl;
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (sockaddr *)&cli_addr, &clilen);

        if(newsockfd < 0){
            perror("accept");
            return 1;
        }
        cout << "Accepted Client with IP : " << inet_ntoa(cli_addr.sin_addr) << endl;
        
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

void writeToClient(int sockfd, string msg){
    msg+='\n';
    write(sockfd, msg.c_str(), msg.size());
}

// bool checkUserStatus(string uid){
//     if(users[uid] != users.end()) return true;
//     return false;
// }

// bool checkUserLoginStatus(string uid){
//     if(loggedInUsers[uid] != loggedInUsers.end()) return true;
//     return false;
// }


void* handleTrackerCommands(void* arg) {
    cout << "Tracker console started. Type 'help' for commands." << endl;
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
        else{
            continue;
        }   
    }
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
                    cout << fontBold << colorGreen << "Login successful !" << clientName << reset << endl;
                    writeToClient(newsockfd, "Login successful");
                }
            }
        }else if(tokens[0] == "create_group"){
            if(tokens.size() != 2){
                cerr << fontBold << colorRed << "Usage : create_group <group_id>" << reset << endl;
            }else{
                if(clientName == ""){
                    // cerr << fontBold << colorRed << "No user is logged-in" << reset << endl;
                    writeToClient(newsockfd, "No user is Logged In !");
                }else{
                    // fetch the logged in user data -
                    User *u = users[clientName];
                    // create a new group -
                    Group *g = new Group(tokens[1]);
                    // adding a logged in user as a owner
                    g->addOwner(u->getUserId());
                    writeToClient(newsockfd, "No user is Logged In !");
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
                        cerr << fontBold << colorRed << "User already exists in the group !" << reset << endl;
                        writeToClient(newsockfd, "User already exists in the group !");
                    }else{
                        // add the logged in user to the group
                        g->addRequest(u->getUserId());
                        cout << fontBold << colorGreen << clientName << " requested to join " << tokens[1] << reset << endl;
                        writeToClient(newsockfd, "Successfully Added to Group !");
                    }
                }
            }
        }else if(tokens[0] == "leave_group"){
            if(tokens.size() != 2){
                cerr << fontBold << colorRed << "Usage : leave_group <group_id>" << reset << endl;
            }else{
                if(clientName == ""){
                    cerr << fontBold << colorRed << "No user is logged-in" << reset << endl;
                }else{
                    // fetch the logged in user data -
                    User *u = users[clientName];
                    // get group -
                    Group *g = groups[tokens[1]];

                    g->removeUser(u->getUserId());
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