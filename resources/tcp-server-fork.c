/* tcp-server-fork.c */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main()
{
  /* Create the listener socket as TCP socket (SOCK_STREAM) */
  int sd = socket( PF_INET, SOCK_STREAM, 0 );
    /* here, the sd is a socket descriptor (part of the fd table) */

  if ( sd == -1 )
  {
    perror( "socket() failed" );
    return EXIT_FAILURE;
  }


  /* socket structures */
  struct sockaddr_in server;
  server.sin_family = PF_INET;  /* AF_INET */

  server.sin_addr.s_addr = htonl( INADDR_ANY );
    /* allow any IP address to connect */

  unsigned short port = 8123;

  /* htons() is host-to-network short for data marshalling */
  /* Internet is big endian; Intel is little endian */
  server.sin_port = htons( port );
  int len = sizeof( server );

  /* attempt to bind (or associate) port 8123 with the socket */
  if ( bind( sd, (struct sockaddr *)&server, len ) == -1 )
  {
    perror( "bind() failed" );
    return EXIT_FAILURE;
  }


  /* identify this port as a TCP listener port */
  if ( listen( sd, 5 ) == -1 )
  {
    perror( "listen() failed" );
    return EXIT_FAILURE;
  }

  printf( "SERVER: TCP listener socket bound to port %d\n", port );


  struct sockaddr_in client;
  int fromlen = sizeof( client );

  int n;
  char buffer[ BUFFER_SIZE ];

  while ( 1 )
  {
    /* TODO: Check for ombie processes using waitpid( ... WNOHANG) */
    printf( "SERVER: Blocked on accept()\n" );
    int newsd = accept( sd, (struct sockaddr *)&client, (socklen_t *)&fromlen );

    printf( "SERVER: Accepted new client connection on newsd %d\n", newsd );

    /* we have just established a TCP connection between server and a
        remote client; below implements the application-layer protocol */

    /* the sd variable is listener socket descriptor that we use to accept
        new incoming client connections (port 8123) */

    /* the newsd variable is the means of communicating via recv()/send()
        or read()/write() with the connected client */

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork() failed");
	return EXIT_FAILURE;
    }

    if (pid > 0)
    {
        close( newsd );
    }
    else
    {
        do
        {
          printf( "CHILD: Blocked on recv()\n" );
    
          /* recv() call will block until we receive data (n > 0)
              or there's an error (n == -1)
               or the client closed its socket (n == 0) */
          n = recv( newsd, buffer, BUFFER_SIZE, 0 );   /* or read() */
    
          if ( n == -1 )
          {
            perror( "recv() failed" );
            return EXIT_FAILURE;
          }
          else if ( n == 0 )
          {
            printf( "CHILD: Rcvd 0 from recv(); closing socket...\n" );
          }
          else /* n > 0 */
          {
            buffer[n] = '\0';   /* assume this is text data */
            printf( "CHILD: Rcvd message from %s: %s\n",
                     inet_ntoa( (struct in_addr)client.sin_addr ),
                     buffer );
    
            /* send ACK message back to client */
            n = send( newsd, "ACK\n", 4, 0 );
    
            if ( n != 4 )
            {
              perror( "send() failed" );
              return EXIT_FAILURE;
            }
          }
        }
        while ( n > 0 );

        close( newsd );
	return EXIT_SUCCESS;
    }
  } /* End of CHILD process code */

  return EXIT_SUCCESS;
}
