#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <dirent.h>
#define BUFFSIZE 256

void syserr(char* msg) { perror(msg); exit(-1); }
void sendall(int tempfd, int newsockfd, char* buffer);
void recvall(int tempfd, int newsockfd, int size, char* buffer);
void servSide(int newsockfd, char* buffer);
void cltSide(char *trackIP, int portTrac, int portClient);
void peer2peer(uint32_t cIP, int cP, char * filen);

typedef struct s
{
	char * filename;
	int portno;
	uint32_t clientIP;
	struct s * next;	
} PeerFile;

PeerFile * head = NULL;
PeerFile * curr = NULL;
PeerFile * tail = NULL;
	  
int main(int argc, char* argv[])
{
	  //declarations and memory allocations
	  int peersockfd, newsockfd, trackerPortno, peerPortno, pid;
	  socklen_t addrlen;
	  struct sockaddr_in serv_addr, peer_addr;
	  char buffer[256];
	  
  
	  //check for correct number of parameters
	  if (argc == 2)
	  {
	  		trackerPortno = 5000;
	  		peerPortno = 6000;
	  }
	  else if (argc == 3)
	  {
	  		trackerPortno = atoi(argv[2]);
	  		peerPortno = 6000;
	  }
	  else if (argc == 4)
	  {
	  		trackerPortno = atoi(argv[2]);
	  		peerPortno = atoi(argv[3]);
	  }
	  else
	  {
		 fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
		 return 1;
	  }
 	
	  //create socket for client 
	  peersockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	  if(peersockfd < 0) syserr("can't open socket\n");
	  printf("create socket...\n");
	  
	  //store the information of the peer
	  memset(&peer_addr, 0, sizeof(peer_addr));
	  peer_addr.sin_family = AF_INET;
	  peer_addr.sin_addr.s_addr = INADDR_ANY;
	  peer_addr.sin_port = htons(peerPortno);
	  
	  if(bind(peersockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) 
		   syserr("can't bind");
	  printf("bind socket to port %d...\n", peerPortno);

	  listen(peersockfd, 5);
	  //conditional for another peer making a connection
	  int anotherPeer = 0;
	  
	  pid = fork();
	  if (pid < 0) syserr("Error on fork");
	  //if another peer connects, enter server mode routine
	  if (pid == 0)
	  {
			anotherPeer = 1;
			printf("Waiting on port %d...\n", peerPortno);
			addrlen = sizeof(peer_addr);
			newsockfd = accept(peersockfd, (struct sockaddr*)&peer_addr, &addrlen);
	  		if(newsockfd < 0) syserr("can't accept"); 
			servSide(newsockfd, buffer);
			close(newsockfd); 
		}
		else
		{
			//run client mode routine
			if (anotherPeer == 0)
			{
				cltSide(argv[1], trackerPortno, peerPortno);
				close(peersockfd);
			}		
		}    			
	   close(peersockfd);
	   return 0;
}

void servSide(int newsockfd, char* buffer)
{
	int n, size, tempfd;
   struct stat filestats;
	char * filename;
	filename = malloc(sizeof(char)*BUFFSIZE);
	
	//Receive file name
   memset(buffer, 0, BUFFSIZE);
	n = recv(newsockfd, buffer, BUFFSIZE, 0);
	if(n < 0) syserr("can't receive filename from peer");
	sscanf(buffer, "%s", filename);
	printf("filename peer wants to download is: %s\n", filename);
	
	//Send file size and file to peer
	stat(filename, &filestats);
	size = filestats.st_size;
	printf("Size of file to send: %d\n", size);
   size = htonl(size);      
	n = send(newsockfd, &size, sizeof(int), 0);
   if(n < 0) syserr("couldn't send size to peer");
   
	tempfd = open(filename, O_RDONLY);
	if(tempfd < 0) syserr("failed to open file");
	sendall(tempfd, newsockfd, buffer);
	close(tempfd);
	
	//Close the connection to peer	
	printf("Connection to peer shutting down\n");
	int status = 1;
	status = htonl(status);
	n = send(newsockfd, &status, sizeof(int), 0);
   if(n < 0) syserr("didn't send exit signal to client");
}

void cltSide(char *trackHost, int portTrac, int portClient)
{
	int socksfd, portno, n, size, listLen;
   char input[70];
  	char *filename;
  	char *command;
  	struct hostent* server;
  	struct sockaddr_in serv_addr;
  	char buffer[256];
  	command = malloc(sizeof(char)*sizeof(buffer));
  
  	server = gethostbyname(trackHost);
  	if(!server) 
  	{
    	fprintf(stderr, "ERROR: no such host: %s\n", trackHost);
    	exit(0);
  	}
   portno = portTrac;

   socksfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if(socksfd < 0) syserr("can't open socket");
   printf("create socket as peer client...\n");

 	//Create server
  	memset(&serv_addr, 0, sizeof(serv_addr)); 
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  	serv_addr.sin_port = htons(portno); 	

 	//Connect to tracker
  	if(connect(socksfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    	syserr("can't connect to tracker");
  	printf("connect...\n");
  	printf("Connection established. Waiting for commands...\n");
  
  	//Send the peer's port number
  	int cliP = htons(portClient);
  	n = send(socksfd, &cliP, sizeof(int), 0);
  	if(n < 0) syserr("can't send portClient to tracker");
  
  	//Send Files
  	DIR * directory;
  	struct dirent * filesInfo;
  	directory = opendir("./");
  	if(directory != NULL)
  	{
  		while((filesInfo = readdir(directory)) != NULL)
  		{
  			if(filesInfo -> d_type != DT_DIR)
  			{
	  			memset(buffer, 0, sizeof(buffer));
	  			strcpy(buffer, filesInfo -> d_name);
				n = send(socksfd, buffer, BUFFSIZE, 0);
				if(n < 0) syserr("can't send filename to tracker");
			}  		
  		}
		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "EndOfList");
		n = send(socksfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't send EndOfList to tracker");
  	}
  
  //Input Commands
  	for(;;)
  	{
  		printf("%s:%d> ", trackHost, portno);
  		fgets(buffer, sizeof(input), stdin);
  		int m = strlen(buffer);
  		if (m>0 && buffer[m-1] == '\n')
  			buffer[m-1] = '\0';
  	
  		strcpy(command, strtok(buffer, " "));
  		if(strcmp(command, "list") == 0)
  		{
  			//Send command
  			memset(buffer, 0, sizeof(buffer));
			strcpy(buffer, "list");
			n = send(socksfd, buffer, BUFFSIZE, 0);
			if(n < 0) syserr("can't send command to tracker");
			
			//Receive the number of elements in the list
			n = recv(socksfd, &size, sizeof(int), 0); 
        	if(n < 0) syserr("can't receive size of list from tracker");
        	listLen = ntohl(size);
        
        	//Populate the list
        	head = curr = tail = NULL;
        	int i;
        	for(i=1; i<=listLen; i++)
        	{
        		curr = (PeerFile *)malloc(sizeof(PeerFile));
  				filename = malloc(sizeof(char)*sizeof(buffer));
        		n = recv(socksfd, &buffer, BUFFSIZE, 0); 
    			if(n < 0) syserr("can't receive filename from tracker");
    			sscanf(buffer, "%s", filename);
    			curr -> filename = filename;
  			
    			uint32_t peerIP;
    			n = recv(socksfd, &peerIP, sizeof(uint32_t), 0); 
    			if(n < 0) syserr("can't receive IP from tracker");
    			curr -> clientIP = ntohl(peerIP);
    		
    			int peerPort;
    			n = recv(socksfd, &peerPort, sizeof(int), 0); 
    			if(n < 0) syserr("can't receive port from tracker");
    			curr -> portno = ntohl(peerPort);
    		
    			curr -> next = NULL;
        		if(tail == NULL)
        		{
        			tail = curr;
        			head = curr;
        		}
        		else
        		{
					tail -> next = curr;
					tail = curr;
        		}
        	} 
        
        	//Print out list to console
        	curr = head;
        	for(i=1; i<=listLen; i++)
        	{
        		char peerAddr[INET_ADDRSTRLEN];
        		uint32_t cIP = curr -> clientIP;
				inet_ntop(AF_INET, &cIP, peerAddr, INET_ADDRSTRLEN);
        		printf("[%d] %s %s:%d\n", i, curr -> filename, peerAddr, ntohs(curr -> portno));
        		curr = curr -> next;
        }
  		}
  		else if(strcmp(command, "exit") == 0)
  		{
  			memset(buffer, 0, sizeof(buffer));
			strcpy(buffer, "exit");
			n = send(socksfd, buffer, BUFFSIZE, 0);
			if(n < 0) syserr("can't send command to tracker");
  			n = send(socksfd, &cliP, sizeof(int), 0);
			if(n < 0) syserr("can't send portno to tracker");
			
			//Wait for ack signal from tracker
			n = recv(socksfd, &size, sizeof(int), 0);
        	size = ntohl(size);  
        	if(n < 0) syserr("can't receive exit signal from server");
        	
        	//Checking for exit
			if(size)
			{
				printf("Connection to server terminated\n");
				break;
			}
			else
			{
				printf("Server didn't exit");
			}
		} 	
  		else if(strcmp(command, "download") == 0)
  		{
  			//Get the number of the index of the file to download 		
  			filename = malloc(sizeof(char)*sizeof(buffer));
  			strcpy(filename, strtok(NULL, " "));
  			
  			//Search for file
  			int index = atoi(filename);
  			free(filename);
  			if(listLen <= index) syserr("Index too large for list");
  			curr = head;
  			int j;
  			for(j=1; j<index; j++)
  			{
  				curr = curr -> next;
  			}
  			//Handle download
  			peer2peer(curr -> clientIP, curr -> portno, curr -> filename);
  		} 
  		else
  		{
  			printf("Unknown command.\n");
  		}  	
  }
  close(socksfd);
}

void sendall(int tempfd, int newsockfd, char* buffer)
{
	while (1)
	{
		memset(buffer, 0, BUFFSIZE);
		int bytes_read = read(tempfd, buffer, BUFFSIZE);
		buffer[bytes_read] = '\0';
		
		if (bytes_read == 0) //Finished reading from file
			break;
		if (bytes_read < 0) syserr("error reading file");
		
		int total = 0;
		int n;
		int bytesleft = bytes_read;
		
		//continue sending while the total hasn't bee reached
		while(total < bytes_read)
		{
			n = send(newsockfd, buffer+total, bytesleft, 0);
			if (n == -1) 
			{ 
			   syserr("error sending file"); 
			   break;
			}
			total += n;
			bytesleft -= n;
		}
	}
}

void peer2peer(uint32_t peerIP, int peerPort, char * filen)
{
	int sockfd, portno, n, size, tempfd;
	char *filename;
	struct hostent* server;
	struct sockaddr_in serv_addr;
	char buffer[256];
	char peerAddr[INET_ADDRSTRLEN];
	
	//Convert clientIP to standard dot notation
	inet_ntop(AF_INET, &peerIP, peerAddr, INET_ADDRSTRLEN);
	
	server = gethostbyname(peerAddr);
	if(!server) {
		fprintf(stderr, "ERROR: no such host: %s\n", peerAddr);
		return;
	}
	portno = peerPort;
	filename = filen;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // check man page
	if(sockfd < 0) syserr("can't open socket");
	printf("create socket...\n");

	// set all to zero, then update the sturct with info
	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
	serv_addr.sin_port = portno; 	

	// connect with filde descriptor, server address and size of addr
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	syserr("can't connect to server");
	printf("connect...\n");
	printf("Connection established. Getting files...\n");
	
	//Get the file
	memset(buffer, 0, sizeof(buffer));
	strcpy(buffer, filename);
	n = send(sockfd, buffer, BUFFSIZE, 0);
	if(n < 0) syserr("can't send filename to peer");
	n = recv(sockfd, &size, sizeof(int), 0); 
   if(n < 0) syserr("can't receive size of file from peer");
   size = ntohl(size);        
	if(size ==0) // check if file exists
	{
		printf("File not found at peer\n");
		return;
	}
	printf("The size of the file to recieve is: %d\n", size);
	tempfd = open(filename, O_CREAT | O_WRONLY, 0666);
	if(tempfd < 0) syserr("failed to get file");
	recvall(tempfd, sockfd, size, buffer);
	printf("Download of '%s' was successful\n", filename);  
	close(tempfd);
	
	//Close connection
	n = recv(sockfd, &size, sizeof(int), 0);
   size = ntohl(size);  
   if(n < 0) syserr("can't receive exit signal from server");
    
	//Check for exit
	if(size)
	{
		printf("Connection to server terminated\n");
	}
	else
	{
		printf("Server didn't exit");
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
