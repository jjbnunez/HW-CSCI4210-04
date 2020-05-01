#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

//This program supports a maximum of 32 clients
//at once. Also, the port is specified here as a
//macro for convenience.
#define PORT 9876
#define MAX_CLIENTS 32

//Helper function to get the maximum
//between two integers.
int max(int x, int y)
{
    if (x > y)
    {
        return x;
    }
    return y;
}

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
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd == -1)
    {
        perror("MAIN: ERROR socket() call failed for TCP\n");
        exit(EXIT_FAILURE);
    }

    //Initialize the UDP socket object, and
    //validate the descriptor result.
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd == -1)
    {
        perror("MAIN: ERROR socket() call failed for UDP\n");
        exit(EXIT_FAILURE);
    }

    //Initialize a server struct for the TCP
    //connections. It defines the kind of
    //traffic it will accept, and it specifies
    //that it can accept traffic from any IP
    //address. Note that AF_INET refers to IPv4. 
    struct sockaddr_in tcp_server;
    tcp_server.sin_family = AF_INET;
    tcp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Initialize a server struct for the UDP
    //connections. Ditto as above.
    struct sockaddr_in udp_server;
    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    //Specify the target port and lengths.
    //htons() and htonl() convert the provided
    //integer arguments from host byte order to
    //network byte order.
    uint16_t port = PORT; 
    tcp_server.sin_port = htons(port);
    udp_server.sin_port = htons(port);
    int tcp_length = sizeof(tcp_server);
    int udp_length = sizeof(udp_server);

    //Bind the TCP and UDP addresses to their
    //respective file descriptors. Specifying
    //the length of the sockaddr_in struct is
    //necessary to pass into the bind()
    //function.
    if (bind(tcp_fd, (struct sockaddr *)&tcp_server, tcp_length ) < 0)
    {
        perror("MAIN: ERROR bind() call failed for TCP\n");
        exit(EXIT_FAILURE);
    }
    if (bind(udp_fd, (struct sockaddr *)&udp_server, udp_length ) < 0)
    {
        perror("MAIN: ERROR bind() call failed for UDP\n");
        exit(EXIT_FAILURE);
    }

    //Set up the passive listener for the TCP
    //socket in particular. We CANNOT also set
    //up a listener for the UDP socket at the
    //same time, since they both listen on the
    //same port. The select() function will
    //handle this for us instead.
    int listener = listen(tcp_fd, MAX_CLIENTS - 1);
    if (listener == -1)
    {
        perror("MAIN: ERROR listen() call failed for TCP\n");
        exit(EXIT_FAILURE);
    }
    printf("MAIN: Listening for TCP connections on port: %d\n", PORT);
    printf("MAIN: Listening for UDP datagrams on port: %d\n", PORT);

    //Terminate.
    return 0;
}