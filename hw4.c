#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

//This program supports a maximum of 32 clients
//at once.
#define PORT 9876
#define MAX_CLIENTS 32

int main(int argc, char** argv)
{
    //Ensure no buffered output for stdout and
    //stderr. This is useful for testing on
    //Submitty.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    //Announce that the main process is running.
    printf("MAIN: Started server\n");

    //Set up variables for select() behavior.
    fd_set read_fds;
    int client_sockets[MAX_CLIENTS];
    int client_socket_index = 0;
    
    //Initialize the TCP socket object, and
    //validate the socket descriptor. For
    //context, "fd" stands for "file descriptor". 
    int tcp_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_descriptor == -1)
    {
        perror("MAIN: ERROR socket() call failed for TCP.\n");
        exit(EXIT_FAILURE);
    }

    //Initialize the UDP socket object, and
    //validate the descriptor result.
    int udp_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_descriptor == -1)
    {
        perror("MAIN: ERROR socket() call failed for UDP.\n");
        exit(EXIT_FAILURE);
    }

    //Initialize a server struct for the TCP
    //connections. We define the kind of
    //traffic it will accept, and we specify
    //that it can accept traffic from any IP
    //address.
    struct sockaddr_in tcp_server;
    tcp_server.sin_family = AF_INET;
    tcp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Initialize a server struct for the UDP
    //connections. Ditto as above.
    struct sockaddr_in udp_server;
    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Specify the target port and lengths.
    tcp_server.sin_port = htons(PORT);
    udp_server.sin_port = htons(PORT);
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

    //Set up TCP listener.
    int tcp_listener = listen(tcp_descriptor, MAX_CLIENTS - 1);
    if (tcp_listener == -1)
    {
        perror("MAIN: ERROR listen() call failed for TCP.\n");
        exit(EXIT_FAILURE);
    }
    printf("MAIN: Listening for TCP connections on port: %d\n", PORT);
    printf("MAIN: Listening for UDP datagrams on port: %d\n", PORT);

    //Terminate.
    return 0;
}