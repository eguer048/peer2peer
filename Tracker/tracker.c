#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#define BUFFSIZE 256

typedef struct s
{
	char filename[30];
	int portno;
	char ipAddr[sizeof(struct sockaddr_in)];
	struct Peerfile * next;	
} PeerFile;

typedef struct s
{
   int sockfd;
   struct sockaddr_in addr;
}Peer;

void syserr(char* msg) { perror(msg); exit(-1); }
void service(int sock, PeerFile *head);

PeerFile * head=0; 
PeerFile * curr=0;

int main(int argc, char* argv[])
{
	//declarations
   int sockfd, newsockfd, portno;
	int peerInAddr, peerPortno;
   struct sockaddr_in serv_addr, clt_addr;
   socklen_t addrlen;
	char buffer[256];
	char peerAddr[sizeof(serv_addr)];
	Peer *temp;

	//check for correct number of parameters
	if (argc == 1)
		portno = 5000;
	else if (argc == 2)
		portno = atoi(argv[1]);
   else 
   { 
      fprintf(stderr,"Usage: %s <port>\n", argv[0]);
      return 1;
   } 
   
	//create socket for server
   sockfd = socket(AF_INET, SOCK_STREAM, 0); 
   if(sockfd < 0) syserr("can't open socket"); 
   printf("create socket...\n");

	//store the information of the server
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);

	//bind to the socket.
   if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
        syserr("can't bind");
   printf("bind socket to port %d...\n", portno);

	//wait for requests
   listen(sockfd, 5); 

   for(;;) 
   {
		//accept requests that come to the server's port number
      printf("Waiting on port %d...\n", portno);
      addrlen = sizeof(clt_addr); 
      newsockfd = accept(sockfd, (struct sockaddr*)&clt_addr, &addrlen);
      if(newsockfd < 0) syserr("can't accept");

      
		peerInAddr = clt_addr.sin_addr.s_addr;
		inet_ntop(AF_INET, &peerInAddr, peerAddr, addrlen);
		printf("The IP address of the client is: %s\n", peerAddr);
		peerPortno = ntohs(clt_addr.sin_port);
		printf("The port of the client is: %d\n", peerPortno);

		recv(newsockfd, &buffer, BUFFSIZE, 0);
		while (strcmp(buffer, "Done") != 0)
		{
			curr = (PeerFile *)malloc(sizeof(PeerFile));
			strcpy(curr->filename, buffer);
			printf("Filename is :%s \n", curr->filename);
			curr->portno = peerPortno;
			strcpy(curr->ipAddr, peerAddr);
			curr->next = head;
			head = curr;
			curr = NULL;

			recv(newsockfd, &buffer, BUFFSIZE, 0); 	
     	}
		printf("Peer at %s:%d has finished transmitting its contents.\n", peerAddr, peerPortno);
		
		
   }
   close(sockfd); 
   return 0;
}

void service(void* temp)
{
   char buffer[256];
   int status, n;
   char command[20];
   memset(buffer, 0, sizeof(command));
   
	for(;;)
	{
		//clear the buffer and then receive command
		memset(buffer, 0, BUFFSIZE);
		n = recv(sock, buffer, BUFFSIZE, 0); 
		if(n < 0) syserr("Can't receive from client."); 
		sscanf(buffer, "%s", command);
		printf("SERVER GOT MESSAGE: %s\n", buffer);
		
		if (!strcmp(command, "list"))
		{
		   
		}
		else if (!strcmp(command, "exit"))
		{
		   printf("Socket received an exit request from peer: %d\n", sock);
		   status = 1;
		   send(sock, &status, sizeof(int), 0);
		   exit(0);
		}
		else
         printf("Unknown command\n");
	}
}
