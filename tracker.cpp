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

using namespace std;

// functions declarations -
void * handleConnections(void *arg);
void writeToClient(int sockfd, string msg);
// bool checkUserStatus(string uid);
// bool checkUserLoginStatus(string uid);
class User;
extern unordered_map<string, User*> users;

 
class User{
private : 
    string userId;
    string password;
    bool isOwner;
public:
    User(string userId, string password){
        this->userId = userId;
        this->password = password;
        this->isOwner = false;
    }

    string getPassword(){
        return this->password;
    }

    string getUserId(){
        return this->userId;
    }

    bool getIsOwner(){
        return this->isOwner;
    }

    void makeOwner(string userId){
        this->isOwner = true;
    }
};


class Group{
private:
    string groupId;
    string ownerId;
    unordered_map<string, User*> groupUsers;

public:
    Group(string groupId){
        this->groupId = groupId;
    }

    void addOwner(string ownerId){
        this->ownerId = ownerId;
        groupUsers[ownerId] = users[ownerId];
    }
};


// maybe i'll use it - so i kept it !
class System{
public:
     
    
};

unordered_map<string, User*> users;
unordered_map<string, Group*> groups;
unordered_map<string, User*> loggedInUsers; 
unordered_map<string, Group*> groupOwners; //  initially planned using <User, Group> -> found that it'll increase complexity -> needed to write some custom logic to handle USER as a key
   

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
                    clientName = tokens[1];
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
                    cout << fontBold << colorGreen << "Login successful !" << clientName << reset << endl;
                    writeToClient(newsockfd, ("Login successful : %s",clientName));
                }
            }
        }else{
            writeToClient(newsockfd, "Invalid Command ... \nValid Commands :\n1. create_user <userid> <password>\n2. login <userid> <password>");
        }
        // else if(tokens[0] == "create_group"){
        //     if(tokens.size() != 2){
        //         cerr << fontBold << colorRed << "Usage : create_group <group_id>" << reset << endl;
        //     }else{
        //         if(checkUserLoginStatus){
        //             cerr << fontBold << colorRed << "UserName doesnot exists" << reset << endl;
        //         }else{
        //             User *u = users[tokens[1]];
        //             loggedInUsers[tokens[1]] = u;
        //         }
        //     }
        // }        
        

        // -----------------------------------------
        
        // a small piece of code to write to the client - currently not using it
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