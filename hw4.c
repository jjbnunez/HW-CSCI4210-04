#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 9876

int main(int argc, char** argv)
{
    //Ensure no buffered output for stdout and stderr.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    //Initialize the TCP socket object, and error-check the descriptor result.
    int tcp_descriptor = socket(PF_INET, SOCK_STREAM, 0);
    if (tcp_descriptor == -1)
    {
        perror("MAIN: ERROR socket() call failed for TCP.\n");
        return EXIT_FAILURE;
    }

    //Initialize the TCP socket object, and error-check the descriptor result.
    int udp_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_descriptor == -1)
    {
        perror("MAIN: ERROR socket() call failed for UDP.\n");
        return EXIT_FAILURE;
    }

    //Initialize a server struct for the TCP connections.
    struct sockaddr_in tcp_server;
    tcp_server.sin_family = PF_INET;
    tcp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Initialize a server struct for the UDP connections.
    struct sockaddr_in udp_server;
    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Specify the target port.
    unsigned short port = 9876;

    //Terminate.
    return 0;
}