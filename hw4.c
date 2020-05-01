#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>

#define PORT 9876

int main(int argc, char** argv)
{
    //Ensure no buffered output for stdout and stderr.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    //Initialize the socket object and error-check the result.
    int socket_descriptor = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_descriptor == -1)
    {
        perror("MAIN: socket() call failed.\n");
        return EXIT_FAILURE;
    }
    
    //Terminate.
    return 0;
}