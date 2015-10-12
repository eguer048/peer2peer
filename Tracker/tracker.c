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

typedef struct p
{
   int sockfd;
   struct sockaddr_in addr;
}Peer;

void syserr(char* msg) { perror(msg); exit(-1); }

PeerFile * head=0; 
PeerFile * curr=0;
pthread_mutex_t peerLock;

void service(void* temp)
{
   char buffer[256];
   int status, n;
   char command[20];
   memset(buffer, 0, sizeof(command));
   Peer* peer = (Peer*) temp;
   int sock = peer->sockfd;
   struct sockaddr_in peer_addr = peer->addr;
   
	for(;;)
	{
	   pthread_mutex_lock(&peerLock);
		//clear the buffer and then receive command
		memset(buffer, 0, BUFFSIZE);
		n = recv(sock, buffer, BUFFSIZE, 0); 
		if(n < 0) syserr("Can't receive from client."); 
		sscanf(buffer, "%s", command);
		printf("SERVER GOT MESSAGE: %s\n", buffer);
		
		if (!strcmp(command, "list"))
		{
		   curr = head;
		   strcpy(buffer, "Start");
		   printf("Tracker is sending: %s\n", buffer);
		   send(sock, &buffer, BUFFSIZE, 0);
		   while (curr)
		   {
		      send(sock, curr->filename, sizeof(curr->filename), 0);
		      send(sock, curr->ipAddr, sizeof(curr->ipAddr), 0);
		      send(sock, curr->portno, sizeof(int), 0);
		      curr = curr->next;
		   }
		   strcpy(buffer, "Done");
		   send(sock, &buffer, BUFFSIZE, 0);
		}
		else if (!strcmp(command, "exit"))
		{
		   printf("Socket received an exit request from peer: %d\n", sock);
		   //remove files belonging to tracker
		   status = 1;
		   send(sock, &status, sizeof(int), 0);
		   break;
		}
		else
         printf("Unknown command\n");
      pthread_mutex_unlock(&peerLock);
	}
}

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
	pthread_t peerThread;
	
	//initialize thread stuff;
	pthread_mutex_init(&peerLock, NULL);

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
		temp = (Peer*)malloc(sizeof(Peer));
      temp->sockfd = newsockfd;
      temp->addr = clt_addr;
      pthread_create(&peerThread, NULL, service, (void *) temp);	
   }
   close(sockfd); 
   return 0;
}
