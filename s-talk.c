// some functions taken from http://beej.us/guide/bgnet/html/

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "s-talk.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define PORT "3490"
#define BACKLOG 10 

void receiveString(void *socketID) {

}

void sendString(void *socketID) {

}

// get IPv4 sockaddr 
void * getInAddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } 
    perror("not IPV4");
}

int main(int argc, char * argv[]) {
    if (argc != 4) {
        exit(EXIT_FAILURE);
    }
    // remote connection info
    const int myPort = atoi(argv[1]);
    const char * remoteName = argv[2];
    const int remotePort = atoi(argv[3]);

    // getaddrinfo
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPV4
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_flags = AI_PASSIVE; // use this computer IP

    int rv;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        perror("getaddrinfo bad");
        exit(EXIT_FAILURE);
    }    

    int sockfd;
    struct addrinfo * i;
    // loop through results
    for(i = res; i != NULL; i = i->ai_next) {
        // create socket
        if ((sockfd = socket(i->ai_family, i->ai_socktype, i->ai_protocol)) == -1) {
            perror("server socket error");
            continue;
        }
        // bind 
        if (bind(sockfd, i->ai_addr, i->ai_addrlen) == -1) {
            close(sockfd);
            perror("server bind error");
            continue;
        }
        // connect
        if (connect(sockfd, i->ai_addr, i->ai_addrlen) == -1) {
            close(sockfd);
            perror("client connect error");
            continue;
        }
        break;
    }

    freeaddrinfo(res);

    if (i == NULL) {
        perror("client failed to connect");
        return 2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen error");
        exit(1);
    }

}   