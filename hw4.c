#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 32

int main(int argc, char** argv)
{
    //Ensure no buffered output for stdout and stderr.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    //Set up variables for select.
    fd_set read_fds;
    int client_sockets[MAX_CLIENTS]; /* client socket fd list */
    int client_socket_index = 0;
    
    //Initialize the TCP socket object, and error-check the descriptor result.
    int tcp_descriptor = socket(PF_INET, SOCK_STREAM, 0);
    if (tcp_descriptor == -1)
    {
        perror("MAIN: ERROR socket() call failed for TCP.\n");
        exit(EXIT_FAILURE);
    }

    //Initialize the UDP socket object, and error-check the descriptor result.
    int udp_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_descriptor == -1)
    {
        perror("MAIN: ERROR socket() call failed for UDP.\n");
        exit(EXIT_FAILURE);
    }

    //Initialize a server struct for the TCP connections.
    struct sockaddr_in tcp_server;
    tcp_server.sin_family = PF_INET;
    tcp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Initialize a server struct for the UDP connections.
    struct sockaddr_in udp_server;
    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Specify the target port and lengths.
    unsigned short port = 9876;
    tcp_server.sin_port = htons(port);
    udp_server.sin_port = htons(port);
    int tcp_length = sizeof(tcp_server);
    int udp_length = sizeof(udp_server);

    //Bind TCP and UDP ports.
    if (bind(tcp_descriptor, (struct sockaddr *)&tcp_server, tcp_length ) < 0)
    {
        perror("MAIN: ERROR bind() call failed for TCP.\n");
        exit(EXIT_FAILURE);
    }
    if (bind(udp_descriptor, (struct sockaddr *)&udp_server, udp_length ) < 0)
    {
        perror("MAIN: ERROR bind() call failed for UDP.\n");
        exit(EXIT_FAILURE);
    }

    //Terminate.
    return 0;
}