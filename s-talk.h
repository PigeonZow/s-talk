#ifndef FUNCTIONS_S_TALK
#define FUNCTIONS_S_TALK

#define MAX_CHARS_PER_LINE 4096

// receive string 
void receiveString(void *socketID);

// send string
void sendString(void *socketID);

#endif