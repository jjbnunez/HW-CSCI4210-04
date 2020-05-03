#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define PORT 9876
#define MAX_CLIENTS 32
#define BUFFER_SIZE 1024
#define OK "OK!\n"

//Global variables for list of clients.
int client_sockets[MAX_CLIENTS];
char client_names[MAX_CLIENTS][16];
pthread_t client_threads[MAX_CLIENTS];

//Global variables for select() behavior.
fd_set read_fds;
int tcp_fd;
int udp_fd;
int client_socket_index = 0;

//Global variables to handle client socket
//descriptors for outbound traffic.
struct sockaddr_in client;
int client_sockaddr_length;

//Global mutex locker.
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//Helper function to derive the passed-in
//command from the outside.
int has_valid_command(char* message, int length)
{
    char temp[BUFFER_SIZE];
    int stop_index = 0;

    for (int i = 0; i < length; i++)
    {
        if (message[i] == ' ')
        {
            stop_index = i;
            break;
        }
        else if (!isalnum(message[i]))
        {
            return 0;
        }
        else
        {
            continue;
        }
    }

    for (int i = 0; i < stop_index; i++)
    {
        temp[i] = message[i];
    }
    temp[stop_index] = '\0';

    if  (strcmp(temp, "LOGIN") == 0 ||
        strcmp(temp, "WHO") == 0|| 
        strcmp(temp, "LOGOUT") == 0|| 
        strcmp(temp, "SEND") == 0|| 
        strcmp(temp, "BROADCAST") == 0){
            return stop_index;
		}  

    return 0;
}

//Helper function to find the index of a TCP
//client connection in the sockets array.
int find_index_of_socket(int socket)
{
    for (int i = 0; i < client_socket_index; i++)
    {
        if (client_sockets[i] == socket)
            return i;
    }
    return -1;
}

//Starts a new thread for a TCP connection.
void* socket_thread(void *arg)
{
    //Check if arg is valid. If not,
    //immediately close the thread.
    if (!arg)
    {
        pthread_exit(0);
    }

    //Else, capture the new socket file
    //descriptor.
    int fd = *((int *)arg);

    //Place the new socket in the list of client
    //sockets.
    pthread_mutex_lock(&lock);
    client_sockets[client_socket_index++] = fd;
    pthread_mutex_unlock(&lock);

    //Temp variable for number of bytes pending.
    int n  = -1;
    
    //Initialize current socket index variable.
    //int current_socket_index = -1;

    //Local string buffer for the thread.
    char buffer[BUFFER_SIZE];

    //Lock before searching for the index.
    while(1)
    {
        //First, zero the file descriptor set
        //on every loop, and then set the TCP
        //and UDP file descriptors in it.
        FD_ZERO(&read_fds);
        FD_SET(tcp_fd, &read_fds);
        FD_SET(udp_fd, &read_fds);

        //Then, also set all of the active TCP
        //connections to the file descriptor
        //set. These two steps essentially
        //refresh the file descriptor set with
        //the most up-to-date information about
        //all pending and active connections.
        for (int i = 0; i < client_socket_index; i++)
        {
            FD_SET(client_sockets[i], &read_fds);
        }
     
        if (FD_ISSET(fd, &read_fds))
        {
            //"n" stores the recv() retval.
            n = recv(fd, buffer, BUFFER_SIZE - 1, 0);

            //If "n" is less than 0, that
            //means recv() failed. However,
            //do not exit.
            if (n < 0)
            {
                perror("MAIN: ERROR client recv() failed\n");
            }

            //Else, if "n" equals 0, that
            //means the client just closed
            //the connection. Remove its
            //file descriptor from the list
            //of active clients.
            else if (n == 0)
            {
                pthread_mutex_lock(&lock);
                //Announce closure of the file descriptor.
                printf("CHILD %ld: Client on fd %d closed connection\n", pthread_self(), fd);
                close(fd);

                //Shift all of the clients
                //ahead of the removed
                //client to the left by one
                //step to manage space.
                for (int k = 0; k < client_socket_index; k++)
                {
                    if (fd == client_sockets[k])
                    {
                        for (int m = k ; m < client_socket_index - 1 ; m++)
                        {
                            client_sockets[m] = client_sockets[m + 1];
                        }
                        client_sockets[client_socket_index] = 0;
                        client_socket_index--;
                        break;
                    }
                }
                pthread_mutex_unlock(&lock);
                pthread_exit(0);
            }

            //Else, "n" is clearly greater
            //than 0. This means there is a
            //message, and "n" represents
            //how long that message is.
            else
            {
                //Apply a null terminator to
                //the end of the expected
                //message space.
                buffer[n] = '\0';

                //Announce that message was
                //received, and from where.
                printf(
                    "CHILD %ld: Rcvd message from %s: %s\n",
                    pthread_self(),
                    inet_ntoa((struct in_addr)client.sin_addr),
                    buffer
                );
                int command =  has_valid_command(buffer, strlen(buffer));
                if (command  == 1) printf("Child <%ld>: Rcvd LOGIN request for userid %s\n",pthread_self(),buffer);
                //Send back an ACK for the
                //TCP connection and ensure
                //the send() call matched
                //the message length.
                n = send(fd, OK, 4, 0);
                if (n != 4)
                {
                    fprintf(stderr, "CHILD %ld: ERROR send() failed\n", pthread_self());
                }
            }
        }
    }
}

//Main function.
int main(int argc, char** argv)
{
    //Ensure no buffered output for stdout and
    //stderr. This is useful for testing on
    //Submitty.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    //String buffer for the main() function.
    char buffer[BUFFER_SIZE];

    //Announce that the main process is running.
    printf("MAIN: Started server\n");
    
    //Initialize the TCP socket object, and
    //validate the socket descriptor. For
    //context, "fd" stands for "file descriptor".
    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd == -1)
    {
        perror("MAIN: ERROR socket() call failed for TCP\n");
        exit(EXIT_FAILURE);
    }

    //Initialize the UDP socket object, and
    //validate the descriptor result.
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
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
    //socket in particular. You CANNOT also set
    //up a listener for the UDP socket at the
    //same time, since they both listen on the
    //same port. The select() function will
    //handle this instead.
    int listener = listen(tcp_fd, MAX_CLIENTS - 1);
    if (listener == -1)
    {
        perror("MAIN: ERROR listen() call failed for TCP\n");
        exit(EXIT_FAILURE);
    }
    printf("MAIN: Listening for TCP connections on port: %d\n", PORT);
    printf("MAIN: Listening for UDP datagrams on port: %d\n", PORT);

    //Main loop. Here's where the magic happens.
    while(1)
    {
        //First, zero the file descriptor set
        //on every loop, and then set the TCP
        //and UDP file descriptors in it.
        FD_ZERO(&read_fds);
        FD_SET(tcp_fd, &read_fds);
        FD_SET(udp_fd, &read_fds);

        //Then, also set all of the active TCP
        //connections to the file descriptor
        //set. These two steps essentially
        //refresh the file descriptor set with
        //the most up-to-date information about
        //all pending and active connections.
        for (int i = 0; i < client_socket_index; i++)
        {
            FD_SET(client_sockets[i], &read_fds);
        }

        //Then, define a wait time for the
        //select() call, so that if it takes
        //long enough while waiting, it restarts
        //the loop.
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 500;

        //The select() function determines how
        //many file descriptors are ready, given
        //the set of file descriptors and the
        //size of the set.
        int ready = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);

        //If select() returns 0, it just means
        //that it timed out. Simply start the
        //loop again.
        if (ready == 0)
        {
            continue;
        }

        //If select() returns -1, it means that
        //select() encountered an error. Exit.
        if (ready == -1)
        {
            perror("MAIN: ERROR select() failed during main loop\n");
            return EXIT_FAILURE;
        }

        //Here's a debug statement to track how
        //many file descriptors are ready.
        #ifdef DEBUG_MODE
        printf("MAIN: select() found %d ready file descriptors\n", ready);
        #endif

        //If the UDP file descriptor is ready
        //with a pending connection, receive the
        //datagram.
        if (FD_ISSET(udp_fd, &read_fds))
        {
            //The recvfrom() function can handle
            //single datagrams. The received
            //message can then be parsed.
            client_sockaddr_length = sizeof(client); 
            int received_bytes = recvfrom(
                udp_fd,
                buffer,
                BUFFER_SIZE-1,
                0,
                (struct sockaddr *) &client,
                (socklen_t *) &client_sockaddr_length
            );
            
            //If the number of received_bytes
            //reads as less than 0, that
            //indicates an error.
            if (received_bytes < 0)
            {
                perror("MAIN: ERROR client recvfrom() failed\n");
            }

            //Else, data must be parsed. Need to
            //determine what kind of message was
            //received.
            else
            {
                //Mandatory acknowledgement log
                //of a datagram being received.
                printf(
                    "MAIN: Rcvd incoming UDP datagram from: %s\n",
                    inet_ntoa(client.sin_addr)
                );

                //Null-terminate the end of the
                //received message. Dirty, but
                //it works.
                buffer[received_bytes] = '\0';

                //TODO: determine the complete
                //validity of the message
                //received.
                has_valid_command(buffer, strlen(buffer));

                //TODO: send various repsonses
                //based on validity and type of
                //command.
                sendto(
                    udp_fd,
                    OK,
                    4,
                    0,
                    (struct sockaddr *) &client,
                    client_sockaddr_length
                );
            }
        }

        //If the TCP file descriptor is ready
        //with a pending connection, run
        //accept() on it. Then, add the new
        //file descriptor to the array of
        //currently active client connections.
        if (FD_ISSET(tcp_fd, &read_fds))
        {
            client_sockaddr_length = sizeof(client);
            int new_sock = accept(
                    tcp_fd,
                    (struct sockaddr *)&client,
                    (socklen_t *)&client_sockaddr_length
                );

            if (pthread_create(&client_threads[client_socket_index], NULL, socket_thread, &new_sock) != 0)
            {
                perror("MAIN: pthread_create() failed to create thread\n");
                exit(EXIT_FAILURE);
            }
        }
        /*
        //Now, cycle through all of the active
        //client connections.
        for (int i = 0; i < client_socket_index; i++)
        {   
            //Capture the current file
            //descriptor. Likewise, c a
            //variable "n" for storing recv()
            //results. The default for "n" is
            //-1.
            int fd = client_sockets[i];
            int n = -1;

            //If the current file descriptor is
            //ready with a pending message, run
            //recv() on it and store its message
            //in the buffer.
            if (FD_ISSET(fd, &read_fds))
            {
                //"n" stores the recv() retval.
                n = recv(fd, buffer, BUFFER_SIZE - 1, 0);

                //If "n" is less than 0, that
                //means recv() failed. However,
                //do not exit.
                if (n < 0)
                {
                    perror("MAIN: ERROR client recv() failed\n");
                }

                //Else, if "n" equals 0, that
                //means the client just closed
                //the connection. Remove its
                //file descriptor from the list
                //of active clients.
                else if (n == 0)
                {
                    //Announce closure of the file descriptor.
                    printf("MAIN: Client on fd %d closed connection\n", fd);
                    close(fd);

                    //Shift all of the clients
                    //ahead of the removed
                    //client to the left by one
                    //step to manage space.
                    for (int k = 0; k < client_socket_index; k++)
                    {
                        if (fd == client_sockets[k])
                        {
                            for (int m = k ; m < client_socket_index - 1 ; m++)
                            {
                                client_sockets[m] = client_sockets[m + 1];
                            }
                            client_sockets[client_socket_index] = 0;
                            client_socket_index--;
                            break;
                        }
                    }
                }

                //Else, "n" is clearly greater
                //than 0. This means there is a
                //message, and "n" represents
                //how long that message is.
                else
                {
                    //Apply a null terminator to
                    //the end of the expected
                    //message space.
                    buffer[n] = '\0';

                    //Announce that message was
                    //received, and from where.
                    printf(
                        "MAIN: Rcvd message from %s: %s\n",
                        inet_ntoa((struct in_addr)client.sin_addr),
                        buffer
                    );

                    //Send back an ACK for the
                    //TCP connection and ensure
                    //the send() call matched
                    //the message length.
                    n = send( fd, "ACK\n", 4, 0 );
                    if ( n != 4 )
                    {
                        perror( "MAIN: ERROR send() failed\n");
                    }
                }
            }
        }
        */
    }

    //Terminate.
    return 0;
}