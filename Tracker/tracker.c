#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#define BUFFSIZE 256

void syserr(char* msg) { perror(msg); exit(-1); }
void sendall(int tempfd, int newsockfd, char* buffer);
void * service(void * s);
void recvall(int tempfd, int newsockfd, int size, char* buffer);

typedef struct s
{
	char * filename;
	int portno;
	uint32_t clientIP;
	struct Peerfile * next;	
} PeerFile;

typedef struct p
{
   int sockfd;
   struct sockaddr_in* addr;
}Peer;

PeerFile * head=NULL; 
PeerFile * curr=NULL;
PeerFile * tail=NULL;
pthread_mutex_t peerLock;
pthread_t peerThread;
int listLen = 0;

int main(int argc, char* argv[])
{
	//declarations
   int sockfd, newsockfd, portno;
   struct sockaddr_in serv_addr, clt_addr;
   socklen_t addrlen;
	char buffer[256];
	Peer *temp;
	
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

		temp = (Peer*)malloc(sizeof(Peer));
      temp->sockfd = newsockfd;
      temp->addr = &clt_addr;
      
      pthread_mutex_init(&peerLock, NULL); 
      pthread_create(&peerThread, NULL, service, (void *) temp);	
   }
   close(sockfd); 
   return 0;
}

void * service(void * temp)
{
   int n, size, newsockfd, clientPort;
	uint32_t clientIP;
	char * fname;
	char command[20];
   char buffer[256];
	
	//Thread struct operations
	Peer* peerInfo = (Peer *) temp;
	newsockfd = peerInfo -> sockfd;
	clientIP = peerInfo -> addr -> sin_addr.s_addr;
	
	//Populate list with client files, IP and port
	pthread_mutex_lock(&peerLock);
	
	n = recv(newsockfd, &clientPort, sizeof(int), 0);
	if(n < 0) syserr("can't receive files from peer");
	int cliPort = ntohs(clientPort);
	printf("Client Port is: %d\n", cliPort);
	
	// Get filenames from peer
	for(;;)
	{	
    	memset(buffer, 0, BUFFSIZE);
		curr = (PeerFile *)malloc(sizeof(PeerFile));
		fname = malloc(sizeof(char)*BUFFSIZE);
		curr -> portno = clientPort;
		curr -> clientIP = clientIP;
		
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't receive files from peer");
		if(strcmp(buffer, "EndOfList") == 0) break;
		
		sscanf(buffer, "%s", fname);
		printf("Filename received is: %s\n", fname);
		strcpy(curr -> filename, fname);
		curr -> next = NULL;
		
		if(tail == NULL)
		{
		  tail = curr;
		  head = curr;
		  listLen++;	
		}
		else
		{
		  tail -> next = curr;
		  tail = curr;
		  listLen++;
		}			
	}
	pthread_mutex_unlock(&peerLock);
	
	for(;;)
	{
	    memset(buffer, 0, BUFFSIZE); 
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		//printf("amount of data recieved: %d\n", n);
		if(n < 0) syserr("can't receive command from client");
		sscanf(buffer, "%s", command);
		//printf("message from client is: %s\n", buffer);
		
		if(strcmp(command, "list") == 0)
		{
			pthread_mutex_lock(&peerLock);
            size = htonl(listLen);      
			n = send(newsockfd, &size, sizeof(int), 0);
		    if(n < 0) syserr("couldn't send listLen to client");
		    curr = head;
			while(curr)
			{
				memset(buffer, 0, BUFFSIZE);
				strcpy(buffer, curr -> filename);
  				//printf("The filename is: %s\n", buffer);
				n = send(newsockfd, &buffer, BUFFSIZE, 0);
		    	if(n < 0) syserr("couldn't send filename to client");
				int cIP = htonl(curr -> clientIP); 
				n = send(newsockfd, &cIP, sizeof(uint32_t), 0);
		    	if(n < 0) syserr("couldn't send clientIP to client");
				int cP = htonl(curr -> portno); 
				n = send(newsockfd, &cP, sizeof(int), 0);
		    	if(n < 0) syserr("couldn't send clientPort to client");
		    	
		    	curr = curr -> next;
			}
			pthread_mutex_unlock(&peerLock);
			//sendall(tempfd, newsockfd, buffer);
			//close(tempfd);			
		}
		
		if(strcmp(command, "exit") == 0)
		{
			int cport;
			n = recv(newsockfd, &cport, sizeof(int), 0);
			if(n < 0) syserr("can't receive files from peer");
			
        	char peerAddr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientIP, peerAddr, INET_ADDRSTRLEN);
			printf("Connection to client %s shutting down\n", peerAddr);
			PeerFile * dltptr = NULL;
			
			pthread_mutex_lock(&peerLock);
			curr = head;
			if(curr -> clientIP == clientIP && curr -> portno == cport)
			{
				do
				{
					head = curr -> next;
					free(curr);
					curr = head;
					listLen--;
					if(head == NULL) break; // case: list emptied out
				} while(curr -> clientIP == clientIP); 
			}
			else
			{
				while(curr -> next -> clientIP != clientIP && curr -> portno != cport) 
				{
					curr = curr -> next;
				}
				do
				{
					dltptr = curr -> next;
					curr = curr -> next -> next;
					listLen--;
					free(dltptr);
				if(curr -> next == NULL) break; // case: list emptied out
				} while(curr -> next -> clientIP != clientIP);
			}
			pthread_mutex_unlock(&peerLock);
						
			int i = 1;
			i = htonl(i);
			n = send(newsockfd, &i, sizeof(int), 0);
		    if(n < 0) syserr("didn't send exit signal to client");
				break; 
		}
	}
	return 0;
}

void sendall(int tempfd, int newsockfd, char* buffer)
{
	while (1)
	{
		memset(buffer, 0, BUFFSIZE);
		int bytes_read = read(tempfd, buffer, BUFFSIZE);
		buffer[bytes_read] = '\0';
		if (bytes_read == 0) 
			break;

		if (bytes_read < 0) syserr("error reading file");
		
		int total = 0;
		int n;
		int bytesleft = bytes_read;
		while(total < bytes_read)
		{
			n = send(newsockfd, buffer+total, bytesleft, 0);
			if (n == -1) 
			{ 
			   syserr("error sending file"); 
			   break;
			}
			//printf("The amount of bytes sent is: %d\n", n);
			total += n;
			bytesleft -= n;
		}
	}
}

void recvall(int tempfd, int newsockfd, int size, char* buffer)
{
	int totalWritten = 0;
	int useSize = 0;
	while(1)
	{
		if(size - totalWritten < BUFFSIZE) 
		{
			useSize = size - totalWritten;
		}
		else
		{
			useSize = BUFFSIZE;
		}
			memset(buffer, 0, BUFFSIZE);
			int total = 0;
			int bytesleft = useSize; //bytes left to recieve
			int n;
			while(total < useSize)
			{
				n = recv(newsockfd, buffer+total, bytesleft, 0);
				if (n == -1) 
				{ 
					syserr("error receiving file"); 
					break;
				}
				total += n;
				bytesleft -= n;
			}

		
			int bytes_written = write(tempfd, buffer, useSize);
			totalWritten += bytes_written;
			if (bytes_written == 0 || totalWritten == size) //Done writing into the file
				break;

			if (bytes_written < 0) syserr("error writing file");
		
    }	
}
