#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
using namespace std;

int main(int argc, char * argv[]){
    int sockfd, newsockfd, portno, n;
    char buffer[255];

    sockaddr_in serv_addr , cli_addr ;
    // sockaddr_in is a struct that stores ipv4 address + port number - check notes.txt - ive written a lot
    socklen_t clilen;
    // socklen_t is the same as unsigned int - used to store the socket address length
    // socket address : ipv4 address + port number

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // AF_INET means - Address Family = Internet 
    // it indicates the socket API that i want to use IPV4 addresses

    // SOCK_STREAM - Socket Type = BYte Stream : it is telling the socket that i want to use a stream based connection (TCP)
    // i can use SOCK_DGRAM - for UDP

    if(sockfd < 0){
        perror("socket");
        return 1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    // bzero is used to assign all byte to be 0 .. fir any datatype
    // modern c++ use memset
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

    newsockfd = accept(sockfd, (sockaddr *)&cli_addr, &clilen);

    if(newsockfd < 0){
        perror("accept");
        return 1;
    }

    while(1){
        bzero(buffer, 255);
        n = read(newsockfd, buffer, 255);
        if(n < 0){
            perror("read");
            return 1;
        }

        // cout << buffer << endl;
        printf("Client : %s", buffer);
        bzero(buffer, 255);

        fgets(buffer, 255, stdin);

        n = write(newsockfd, buffer, strlen(buffer));
        if(n < 0){
            perror("write");
            return 1;
        }
        int l = strncmp("bye", buffer, 3);
        if(l == 0){
            break;
        }

    }

    close(newsockfd);
    close(sockfd);

    return 0;

}

