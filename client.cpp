#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
using namespace std;

// remember the process -
/*
1. client ko bind karne ki zaroorat nahi hai -> it has to directly connect !
2. 
*/
int main(int argc, char *argv[]){
    int sockfd, portno, n;
    sockaddr_in serv_addr;
    hostent *server;

    char buffer[256];

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        return 1;
    }

    server = gethostbyname(argv[1]);
    if(server == NULL){
        perror("gethostbyname");
        return 1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if(connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("connect");
        return 1;
    }
    string userName;

    while(1){
        bzero(buffer, 255);
        fgets(buffer, 255, stdin);
        n = write(sockfd, buffer, strlen(buffer));
        if(n < 0){
            perror("write");
            return 1;
        }
        bzero(buffer, 255);

        // a small piece of code to read data from the server - 
        n = read(sockfd, buffer, 255);
        if(n < 0){
            perror("read");
            return 1;
        }

        printf("Server : %s", buffer);
        
    }

    close(sockfd);
    return 0;
}