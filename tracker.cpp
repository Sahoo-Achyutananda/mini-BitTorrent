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

using namespace std;

class Group{
private:
    string groupId;
    string ownerId;
public:
    Group(string groupId){
        this->groupId = groupId;
    }

    void addOwner(string ownerId){
        this->ownerId = ownerId;
    }
};

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

    void makeOwner(string userId){
        this->isOwner = true;
    }
};


class System{
public:
     
    
};

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
    set<User> users;
    set<Group> groups;
    unordered_map<string, Group> groupOwners; //  initially planned using <User, Group> -> found that it'll increase complexity -> needed to write some custom logic to handle USER as a key
    

    int sockfd, newsockfd, portno, n;
    char buffer[255];

    sockaddr_in serv_addr , cli_addr ;
    socklen_t clilen;
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
    clilen = sizeof(cli_addr);
    cout << "Tracker Listening on Port No : " << portno << endl;
    newsockfd = accept(sockfd, (sockaddr *)&cli_addr, &clilen);

    if(newsockfd < 0){
        perror("accept");
        return 1;
    }
    cout << "Accepted Client with IP : " << inet_ntoa(cli_addr.sin_addr) << endl;

    while(1){
        cout << "Tracker is running, waiting for requests ... " << endl;
        bzero(buffer, 255);
        n = read(newsockfd, buffer, 255);
        if(n < 0){
            perror("read");
            return 1;
        }
        printf("Client : %s", buffer);
        // ----------------------------------------
        // string cmd(buffer); // this gave me seg fault -> cause i was reading more than the actual bytes that was read
        string cmd(buffer, n);
        
        // debug -
        cout << cmd << endl;
        vector<string> tokens = tokenizeString(cmd);
        cout << tokens[0] << endl;

        if(tokens[0] == "create_user"){
            cout << "Client wants to create a new user !!! " << endl;
        }

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
    close(sockfd);

    return 0;
}