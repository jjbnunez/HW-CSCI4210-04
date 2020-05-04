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
int num_clients = 0;

//Global string buffer.
char buffer[BUFFER_SIZE];

//Global variables for select() behavior.
fd_set read_fds;
int tcp_fd;
int udp_fd;

//Global variables to handle client socket
//descriptors for outbound traffic.
struct sockaddr_in client;
int client_sockaddr_length;

//Global mutex locker.
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//Gets a string after a command
char* get_message(char* message, int start)
{
	char* temp = malloc(sizeof(char[BUFFER_SIZE]));
	int stop_index = 0;

	for (int i = start; i < BUFFER_SIZE; i++)
	{
		if (message[i] == ' ' || message[i] == '\n')
		{
			stop_index = i;
			break;
		}
		/*
		else if (!isalnum(message[i]))
		{
			strcpy(temp, ".");
			return temp;
		}*/
		else
		{
			continue;
		}
	}
	int length = stop_index - start;
	for (int i = 0; i < length; i++)
	{
		temp[i] = message[i + start];
	}
	return temp;
}

char* get_message_send(char* message, int start)
{
	char* temp = malloc(sizeof(char[BUFFER_SIZE]));
	int stop_index = 0;

	for (int i = start; i < BUFFER_SIZE; i++)
	{
		if (message[i] == '\n')
		{
			stop_index = i;
			break;
		}
		/*
		else if (!isalnum(message[i]))
		{
			strcpy(temp, ".");
			return temp;
		}*/
		else
		{
			continue;
		}
	}
	int length = stop_index - start;
	for (int i = 0; i < length; i++)
	{
		temp[i] = message[i + start];
	}
	return temp;
}

//Helper function to derive the passed-in
//command from the outside.
int has_valid_command(char* message, int length)
{
	char temp[BUFFER_SIZE];
	int stop_index = 0;

	for (int i = 0; i < length; i++)
	{
		if (message[i] == ' ' || message[i] == '\n')
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

	if (strcmp(temp, "LOGIN") == 0) return 1;
	if (strcmp(temp, "WHO") == 0) return 2;
	if (strcmp(temp, "LOGOUT") == 0) return 3;
	if (strcmp(temp, "SEND") == 0) return 4;
	if (strcmp(temp, "BROADCAST") == 0) return 5;

	return 0;
}

//Helper function to find the index of a TCP
//thread in the client threads array.
int get_thread_index(pthread_t thread_id)
{
#ifdef DEBUG_MODE
	printf("Child <%ld>: DEBUG get_thread_index() num_clients is %d\n", pthread_self(), num_clients);
#endif
	for (int i = 0; i < num_clients; i++)
	{
		if (client_threads[i] == thread_id)
			return i;
	}
	return -1;
}

char* set_who_list()
{
	char* temp = malloc(sizeof(char) * ((32 * 16) + 4));
	strncpy(temp, "OK!\n", 4);
	printf("Child <%ld>: DEBUG set_who_list() num_clients is %d\n", pthread_self(), num_clients);
	for (int i = 0; i < num_clients; i++)
	{
		strcat(temp, client_names[i]);
		strcat(temp, "\n");
	}
#ifdef DEBUG_MODE
	printf("Child <%ld>: DEBUG set_who_list() is %s\n", pthread_self(), temp);
#endif
	return temp;
}

//Send back an ACK for the
//TCP connection and ensure
//the send() call matched
//the message length.
void send_ok(int fd, int received_bytes)
{
	received_bytes = send(fd, OK, 4, 0);
	if (received_bytes != 4)
	{
		fprintf(stderr, "Child <%ld>: ERROR send() failed\n", pthread_self());
	}
}

void send_msg(int fd, int received_bytes, char* msg) {
	received_bytes = send(fd, msg, strlen(msg), 0);
	if (received_bytes != strlen(msg))
	{
		fprintf(stderr, "Child <%ld>: ERROR send() failed\n", pthread_self());
	}
}

char* get_send_msg(char* userid, char* msglen, char*message) {
	char* send_message = malloc(sizeof(char[1024 + 16 + 16]));
	strcpy(send_message, "FROM ");
	strcat(send_message, userid);
	strcat(send_message, " ");
	strcat(send_message, msglen);
	strcat(send_message, " ");
	strcat(send_message, message);
	strcat(send_message, "\n");

	return send_message;
}

int is_valid_username(char* username)
{
	//Obviously, return if the function received
	//NULL as a message. Seg faults are bad.
	if (username == NULL)
	{
		return 0;
	}

	//Determine the length of the message. 
	int length = strlen(username);

	//The username is invalid if the message is
	//too short or too long.
	if (length < 4 || length > 16)
	{
		return 0;
	}

	//This function expects there NOT to be a
	//newline character at the end of a message.
	for (int i = 0; i < length; i++)
	{
		if (!isalnum(username[i]))
		{
			return 0;
		}
	}

	//The trials have been passed.
	return 1;
}

int is_valid_message_length(char* message_length)
{
	//Obviously, return if the function received
	//NULL as a message. Seg faults are bad.
	if (message_length == NULL)
	{
		return 0;
	}

	//Determine the length of the message.
	int length = strlen(message_length);

	//This function expects there NOT to be a
	//newline character at the end of a message.
	for (int i = 0; i < length; i++)
	{
		if (!isdigit(message_length[i]))
		{
			return 0;
		}
	}

	//The trials have been passed.
	return 1;
}

int get_username_index(char* username)
{
	//Seg faults are bad.
	if (username == NULL)
	{
		return -1;
	}

	//If the username exists in the username
	//list, return that index.
	for (int i = 0; i < num_clients; i++)
	{
		if (strcmp(username, client_names[i]) == 0)
		{
			return i;
		}
	}

	//If not found, return -1.
	return -1;
}
void udpeepee() {

}
//Starts a new thread for a TCP connection.
void* socket_thread(void *arg)
{
	//Detach the thread immediately since the
	//thread doesn't need to be joined.
	pthread_detach(pthread_self());

	pthread_mutex_lock(&lock);

	//Check if arg is valid. If not,
	//immediately close the thread.
	if (!arg)
	{
		fprintf(stderr, "Child <%ld>: ERROR bad arg passed into pthread_create()\n", pthread_self());
		pthread_mutex_unlock(&lock);
		pthread_exit(NULL);
	}

#ifdef DEBUG_MODE
	printf("Child <%ld>: DEBUG num_clients is %d\n", pthread_self(), num_clients);
#endif

	//Capture the new socket file descriptor.
	int fd = *((int *)arg);

	client_sockets[num_clients] = fd;
	client_threads[num_clients] = pthread_self();
	strncpy(client_names[num_clients], "reserved", 16);
	num_clients++;
	pthread_mutex_unlock(&lock);

	//Establish a local string buffer.
	char local_buffer[BUFFER_SIZE];

	//Need to capture the number of received
	//bytes from the recv() calls each time.
	int received_bytes = -1;

	//Initialize current thread index variable.
	int current_thread_index = -1;

	while (1)
	{
		pthread_mutex_lock(&lock);
		FD_CLR(fd, &read_fds);
		FD_SET(fd, &read_fds);
		pthread_mutex_unlock(&lock);

#ifdef DEBUG_MODE
		printf("Child <%ld>: DEBUG num_clients is %d\n", pthread_self(), num_clients);
#endif

		if (FD_ISSET(fd, &read_fds))
		{
			//Store the recv() retval.
			received_bytes = recv(fd, local_buffer, BUFFER_SIZE - 1, 0);

#ifdef DEBUG_MODE
			printf("Child <%ld>: DEBUG received %d bytes\n", pthread_self(), received_bytes);
#endif

			//If it is less than 0, that means
			//recv() failed. However, do not
			//exit.
			if (received_bytes < 0)
			{
				perror("MAIN: ERROR client recv() failed\n");
				pthread_exit(NULL);
			}

			//Else, if 0 was received, that
			//means the client just closed
			//the connection. Remove its
			//file descriptor from the list
			//of active clients.
			else if (received_bytes == 0)
			{
				pthread_mutex_lock(&lock);
				printf("Child <%ld>: Client disconnected\n", pthread_self(), fd);
				FD_CLR(fd, &read_fds);
				close(fd);
				for (int i = 0; i < num_clients; i++)
				{
					if (fd == client_sockets[i])
					{
						for (int j = i; j < num_clients - 1; j++)
						{
							client_sockets[j] = client_sockets[j + 1];
							client_threads[j] = client_threads[j + 1];
							strncpy(client_names[j], client_names[j + 1], 16);
						}
						client_sockets[num_clients] = 0;
						client_threads[num_clients] = 0;
						strncpy(client_names[num_clients], "\0", 16);
						num_clients--;
						break;  /* all done */
					}
				}
				pthread_mutex_unlock(&lock);
				pthread_exit(NULL);
			}

			//Else, it is clearly greater
			//than 0. This means there is a
			//message, and we know how long that
			//message is.
			else
			{
				//Apply a null terminator to
				//the end of the expected
				//message space.
				local_buffer[received_bytes] = '\0';

				//Announce that message was
				//received, and from where.
				printf(
					"Child <%ld>: Rcvd message from %s: %s\n",
					pthread_self(),
					inet_ntoa((struct in_addr)client.sin_addr),
					local_buffer
				);

				//Determine the command received
				//and its validity.
				int command = has_valid_command(local_buffer, strlen(local_buffer));

				char* message;
				//char* userid;

				//LOGIN
				if (command == 1)
				{
					message = get_message(local_buffer, 6);

					//Lock before everything,
					//but unlock after each
					//case.
					pthread_mutex_lock(&lock);

					if (is_valid_username(message) == 0)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Invalid userid %s\n", pthread_self(), message);
						pthread_exit(NULL);
					}
					else if (get_username_index(message) != -1)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Already connected\n", pthread_self());
						pthread_exit(NULL);
					}
					else
					{
						current_thread_index = get_thread_index(pthread_self());
#ifdef DEBUG_MODE
						printf("Child <%ld>: DEBUG current_thread_index is %d\n", pthread_self(), current_thread_index);
#endif
						strncpy(client_names[current_thread_index], message, 16);
						pthread_mutex_unlock(&lock);
						printf("Child <%ld>: Rcvd LOGIN request for userid %s\n", pthread_self(), message);
						send_ok(fd, received_bytes);
					}
					free(message);
				}

				//WHO
				if (command == 2)
				{
					printf("Child <%ld>: Rcvd WHO request\n", pthread_self());

					char* list = set_who_list();

					received_bytes = send(fd, list, strlen(list), 0);

					if (received_bytes != strlen(list))
						fprintf(stderr, "Child <%ld>: ERROR WHO request failed\n", pthread_self());

					free(list);
				}

				//LOGOUT
				if (command == 3)
				{
					pthread_mutex_lock(&lock);

					printf("Child <%ld>: Rcvd LOGOUT request\n", pthread_self());

					send_ok(fd, received_bytes);
					FD_CLR(fd, &read_fds);
					close(fd);

					for (int i = 0; i < num_clients; i++)
					{
						if (fd == client_sockets[i])
						{
							for (int j = i; j < num_clients - 1; j++)
							{
								client_sockets[j] = client_sockets[j + 1];
								client_threads[j] = client_threads[j + 1];
								strncpy(client_names[j], client_names[j + 1], 16);
							}
							client_sockets[num_clients] = 0;
							client_threads[num_clients] = 0;
							strncpy(client_names[num_clients], "\0", 16);
							num_clients--;
							break;
						}
					}

					pthread_mutex_unlock(&lock);

					pthread_exit(NULL);
				}

				//SEND
				if (command == 4)
				{
					printf("Child <%ld>: Rcvd SEND request to userid %s\n", pthread_self(), local_buffer);

					char local_buffer_2[BUFFER_SIZE];
					received_bytes = recv(fd, local_buffer_2, BUFFER_SIZE - 1, 0);

					char* userid = get_message(local_buffer, 5);
					int send_index = -1;

					pthread_mutex_lock(&lock);

					for (int i = 0; i < num_clients; i++)
					{
						if (strcmp(userid, client_names[i]) == 0)
							send_index = i;
					}

					if (send_index == -1)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Unknown userid %s\n", pthread_self(), message);
						continue;
					}

					int offset1 = strlen(userid);

					char* msglenstr = get_message(local_buffer, 6 + offset1);

					if (is_valid_message_length(msglenstr) == 0)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Invalid msglen %s\n", pthread_self(), msglenstr);
						exit(EXIT_FAILURE);
					}

					int msglen = atoi(msglenstr);

					if (msglen < 1 || msglen > 990)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Invalid msglen %s\n", pthread_self(), msglenstr);
						exit(EXIT_FAILURE);
					}

					//int offset2 = strlen(msglenstr);
					char* message = get_message_send(local_buffer_2, 0);

					char my_user_id[16];
					current_thread_index = get_thread_index(pthread_self());
					strcpy(my_user_id, client_names[current_thread_index]);

					char* send_message = get_send_msg(my_user_id, msglenstr, message);
					send_ok(fd, received_bytes);
					send_msg(client_sockets[send_index], received_bytes, send_message);
					pthread_mutex_unlock(&lock);

					free(send_message);
					free(message);
					free(msglenstr);
					free(userid);
				}

				//BROADCAST
				if (command == 5)
				{
					char local_buffer_3[BUFFER_SIZE];
					printf("Child <%ld>: Rcvd BROADCAST request\n", pthread_self());
					received_bytes = recv(fd, local_buffer_3, BUFFER_SIZE - 1, 0);
					char* msglenstr = get_message(local_buffer, 10);

					int send_index = 0;

					if (is_valid_message_length(msglenstr) == 0)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Invalid msglen %s\n", pthread_self(), msglenstr);
						exit(EXIT_FAILURE);
					}

					int msglen = atoi(msglenstr);

					if (msglen < 1 || msglen > 990)
					{
						pthread_mutex_unlock(&lock);
						fprintf(stderr, "Child <%ld>: ERROR Invalid msglen %s\n", pthread_self(), msglenstr);
						exit(EXIT_FAILURE);
					}
					char my_user_id[16];
					current_thread_index = get_thread_index(pthread_self());
					strcpy(my_user_id, client_names[current_thread_index]);

					char* message = get_message_send(local_buffer_3, 0);
					char* send_message = get_send_msg(my_user_id, msglenstr, message);
					while (strcmp(client_names[send_index], "\0") != 0 && send_index != 16) {
						if (strcmp(client_names[send_index], my_user_id) != 0) {
							send_msg(client_sockets[send_index], received_bytes, send_message);
						}
						send_index++;
					}
					pthread_mutex_unlock(&lock);

				}

				if (command == 0)
				{
					printf("Child <%ld>: ERROR received invalid command \'%s\'", pthread_self(), local_buffer);
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
	if (bind(tcp_fd, (struct sockaddr *)&tcp_server, tcp_length) < 0)
	{
		perror("MAIN: ERROR bind() call failed for TCP\n");
		exit(EXIT_FAILURE);
	}
	if (bind(udp_fd, (struct sockaddr *)&udp_server, udp_length) < 0)
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

	//Zero the file descriptor set.
	FD_ZERO(&read_fds);

	//Main loop. Here's where the magic happens.
	while (1)
	{
		//Refresh the TCP and UDP file
		//descriptors in the socket descriptor
		//set.
		pthread_mutex_lock(&lock);
		FD_CLR(tcp_fd, &read_fds);
		FD_CLR(udp_fd, &read_fds);
		FD_SET(tcp_fd, &read_fds);
		FD_SET(udp_fd, &read_fds);
		pthread_mutex_unlock(&lock);

		//Define a wait time for the select()
		//call, so that if it takes long enough
		//while waiting, it restarts the loop.
		struct timeval timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 500;

		//The select() function determines how
		//many file descriptors are ready, given
		//the set of file descriptors and the
		//size of the set.
		int ready = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);

		//Debug statement to determine the
		//output of select().
#ifdef DEBUG_MODE
		printf("MAIN: DEBUG select() finished and got %d ready\n", ready);
#endif

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

			//Debug statement to determine if a
			//new socket was accepted.
#ifdef DEBUG_MODE
			printf("MAIN: DEBUG accept() accepted new_sock %d\n", new_sock);
#endif
			pthread_t client_thread;
			pthread_mutex_lock(&lock);
			if (pthread_create(&client_thread, NULL, socket_thread, &new_sock) != 0)
			{
				perror("MAIN: pthread_create() failed to create thread\n");
				exit(EXIT_FAILURE);
			}
			pthread_mutex_unlock(&lock);
		}

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
				BUFFER_SIZE - 1,
				0,
				(struct sockaddr *) &client,
				(socklen_t *)&client_sockaddr_length
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

				//Determine the complete
				//validity of the message
				//received.
				int command = has_valid_command(buffer, strlen(buffer));

				//char* message;
				//char* userid;

				//WHO 
				if (command == 2)
				{
					printf("MAIN: Rcvd WHO request\n");

					char* list = set_who_list();

					received_bytes = sendto(udp_fd, list, strlen(list), 0, (struct sockaddr *) &client, client_sockaddr_length);

					if (received_bytes != strlen(list))
						perror("MAIN: ERROR WHO request failed\n");

					free(list);
				}
				//SEND
				if (command == 4)
				{
					perror("MAIN: ERROR SEND not supported over UDP\n");
				}
				//BROADCAST
				if (command == 5)
				{
					char local_buffer_3[BUFFER_SIZE];
					printf("MAIN: Rcvd BROADCAST request\n");
					received_bytes = recv(udp_fd, local_buffer_3, BUFFER_SIZE - 1, 0);
					char* msglenstr = get_message(buffer, 10);

					int send_index = 0;

					if (is_valid_message_length(msglenstr) == 0)
					{
						fprintf(stderr, "MAIN: ERROR Invalid msglen %s\n", msglenstr);
						exit(EXIT_FAILURE);
					}

					int msglen = atoi(msglenstr);

					if (msglen < 1 || msglen > 990)
					{
						fprintf(stderr, "MAIN: ERROR Invalid msglen %s\n", msglenstr);
						exit(EXIT_FAILURE);
					}
					char my_user_id[16];
					strcpy(my_user_id, "UDP-client");

					char* message = get_message_send(local_buffer_3, 0);
					char* send_message = get_send_msg(my_user_id, msglenstr, message);
					while (strcmp(client_names[send_index], "\0") != 0 && send_index != 16) {
						if (strcmp(client_names[send_index], my_user_id) != 0) {
							send_msg(client_sockets[send_index], received_bytes, send_message);
						}
						send_index++;
					}
				}

				if (command == 0)
				{
					printf("MAIN: ERROR received invalid command \'%s\'", buffer);
				}
			}
		}
	}

	//Terminate.
	return EXIT_SUCCESS;
}