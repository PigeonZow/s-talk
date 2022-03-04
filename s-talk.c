// some functions taken from http://beej.us/guide/bgnet/html/

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "s-talk.h"
#include "list.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define BACKLOG 10

// get IPv4 sockaddr
void *getInAddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    perror("not IPV4");
}

// setup server port
void setup_listen_port(int myPort)
{
    struct addrinfo hints, *servinfo;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_flags = AI_PASSIVE;    // use my IP

    if ((rv = getaddrinfo(NULL, myPort, &hints, &servinfo)) != 0)
    {
        perror("listener getaddrinfo bad");
        exit(EXIT_FAILURE);
    }

    // loop through all the results and bind to the first we can
    for (SERV_P = servinfo; SERV_P != NULL; SERV_P = SERV_P->ai_next)
    {
        if ((SERVFD = socket(SERV_P->ai_family, SERV_P->ai_socktype, SERV_P->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (bind(SERVFD, SERV_P->ai_addr, SERV_P->ai_addrlen) == -1)
        {
            close(SERVFD);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (SERV_P == NULL)
    {
        perror("listener failed to bind socket");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);
    printf("listener: waiting to recvfrom...\n");
}

char *receive_string()
{
    int numbytes;
    socklen_t addr_len;
    struct sockaddr_storage their_addr;
    char buf[MAX_CHARS_PER_LINE];
    char s[INET_ADDRSTRLEN]; // IPv4 size

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(SERVFD, buf, MAX_CHARS_PER_LINE - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));

    buf[numbytes] = '\0';
    return buf;
}

void setup_talk_port(char *remoteName, int remotePort)
{
    struct addrinfo hints, *servinfo;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(remoteName, remotePort, &hints, &servinfo)) != 0)
    {
        perror("talker getaddrinfo bad");
        exit(EXIT_FAILURE);
    }

    // loop through all the results and make a socket
    for (SOCK_P = servinfo; SOCK_P != NULL; SOCK_P = SOCK_P->ai_next)
    {
        if ((SOCKFD = socket(SOCK_P->ai_family, SOCK_P->ai_socktype, SOCK_P->ai_protocol)) == -1)
        {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (SOCK_P == NULL)
    {
        perror("talker: failed to create socket");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);
}

void send_string(char *buf)
{
    int numbytes;
    if ((numbytes = sendto(SOCKFD, buf, strlen(buf), 0, SOCK_P->ai_addr, SOCK_P->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(EXIT_FAILURE);
    }
    printf("talker: sent %d bytes\n", numbytes);
}

pthread_t thread_input, thread_send, thread_receive, thread_print;
pthread_cond_t send_ready, recv_ready;
pthread_mutex_t lock;
List *g_send_buf, *g_recv_buf;

// all threads contain while(active)
// false when reading "!" from stdin or receiving from the remote user
bool active = true;

// reads line of input from stream, appends to send_buf
// terminates the process if it reads the string "!"
void *input(void *arg)
{
    while (active)
    {
        char *input_buf = malloc(sizeof(char) * MAX_CHARS_PER_LINE);
        if (input_buf == NULL)
        {
            exit(EXIT_FAILURE);
        }
        fgets(input_buf, MAX_CHARS_PER_LINE, stdin);
        pthread_mutex_lock(&lock);
        if (!strcmp(input_buf, "!"))
        {
            active = false;
            free(input_buf);
        }
        else
        {
            if (List_append(g_send_buf, input_buf))
            {
                exit(EXIT_FAILURE);
            }
        }
        pthread_cond_signal(&send_ready);
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

// pops the first item of send_buf and sends it to the remote user
void *send(void *arg)
{
    while (active)
    {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&send_ready, &lock);
        List_first(g_send_buf);
        char *msg = (char *)List_remove(g_send_buf);
        // required for proper shutdown of this thread since input()
        // might signal send_ready with an empty list to unblock
        if (msg != NULL)
        {
            send_string(msg);
        }
        free(msg);
        pthread_mutex_unlock(&lock);
    }
}

// attempts to receive one line from the remote user, appending to recv_buff
// terminates the process if it receives the string "!"
void *receive(void *arg)
{
    while (active)
    {
        char *msg = malloc(sizeof(char) * MAX_CHARS_PER_LINE);
        if (msg == NULL)
        {
            exit(EXIT_FAILURE);
        }
        // [receive item, assumed to be non-null]
        receive_string();
        pthread_mutex_lock(&lock);
        if (!strcmp(msg, "!"))
        {
            active = false;
            free(msg);
        }
        else
        {
            if (List_append(g_recv_buf, msg))
            {
                exit(EXIT_FAILURE);
            }
        }
        pthread_cond_signal(&recv_ready);
        pthread_mutex_unlock(&lock);
    }
}

// pops the first item of recv_buff and displays it to the local user
void *print(void *arg)
{
    while (active)
    {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&recv_ready, &lock);
        List_first(g_recv_buf);
        char *output_buf = (char *)List_remove(g_recv_buf);
        // simply don't print anything if list is empty or item is NULL
        // required for proper shutdown of this thread since receive()
        // might signal recv_ready with an empty list to unblock
        if (output_buf != NULL)
        {
            puts(output_buf);
        }
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

struct addrinfo *SERV_P, *SOCK_P;
int SERVFD, SOCKFD; // local, remote
char * SEND_BUF;

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        exit(EXIT_FAILURE);
    }
    // remote connection info
    const int myPort = atoi(argv[1]);
    const char *remoteName = argv[2];
    const int remotePort = atoi(argv[3]);

    // setup sockets
    setup_listen_port(myPort);
    setup_talk_port(remoteName, remotePort);

    printf("Initializing lists... ");
    g_send_buf = List_create();
    g_recv_buf = List_create();
    if (!g_send_buf || !g_recv_buf) {
        exit(EXIT_FAILURE);
    }
    puts("DONE");
    printf("Initializing mutex... ");
    if (pthread_mutex_init(&lock, NULL)) {
        exit(EXIT_FAILURE);
    }
    printf("Initializing condition variables... ");
    if (pthread_cond_init(&send_ready, NULL)) {
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&recv_ready, NULL)) {
        exit(EXIT_FAILURE);
    }
    puts("DONE");
    bool error;
    error = pthread_create(&thread_input, NULL, &input, NULL);
    if (error) {
        exit(EXIT_FAILURE);
    }
    error = pthread_create(&thread_print, NULL, &print, NULL);
    if (error) {
        exit(EXIT_FAILURE);
    }
    error = pthread_create(&thread_receive, NULL, &receive, NULL);
    if (error) {
        exit(EXIT_FAILURE);
    }
    error = pthread_create(&thread_send, NULL, &send, NULL);
    if (error) {
        exit(EXIT_FAILURE);
    }
    pthread_join(thread_input, NULL);
    pthread_join(thread_print, NULL);
    pthread_join(thread_receive, NULL);
    pthread_join(thread_send, NULL);

    // cleanup
    pthread_cond_destroy(&send_ready);
    pthread_cond_destroy(&recv_ready);
    pthread_mutex_destroy(&lock);
    List_free(g_send_buf, free);
    List_free(g_recv_buf, free);

    return EXIT_SUCCESS;
}
