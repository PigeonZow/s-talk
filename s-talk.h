#ifndef FUNCTIONS_S_TALK
#define FUNCTIONS_S_TALK

#define MAX_CHARS_PER_LINE 4096

// listen socket 
void setup_listen_port(char *remotePort);

// talk socket
void setup_talk_port(char *remoteName, char *remotePort);

// receive string 
char *receive_string();

// send string
void send_string(char *buf);

void *input(void *arg);

void *dispatch(void *arg);

void *receive(void *arg);

void *print(void *arg);

#endif