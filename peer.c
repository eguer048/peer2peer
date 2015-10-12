#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#define BUFFSIZE 256

void syserr(char* msg) { perror(msg); exit(-1); }

typedef struct s
{
		char filename[20];
		char ipAddr[sizeof(struct sockaddr_in)];
		int portno;
}PeerFile;

PeerFile * head = 0;
PeerFile * curr = 0;
	  
int main(int argc, char* argv[])
{
	  //declarations and memory allocations
	  int peersockfd, peerPortno, trackerPortno, n, status;
	  struct hostent* tracker;
	  struct sockaddr_in serv_addr, peer_addr;
	  char buffer[256];
	  char command[20];
	  
  
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

	  //check that the selected server exists
	  tracker = gethostbyname(argv[1]);
	  if(!tracker) 
	  {
		 fprintf(stderr, "ERROR: No such tracker: %s\n", argv[1]);
		 return 2;
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

	  //store the information of the tracker
	  memset(&serv_addr, 0, sizeof(serv_addr));
	  serv_addr.sin_family = AF_INET;
	  serv_addr.sin_addr = *((struct in_addr*)tracker->h_addr);
	  serv_addr.sin_port = htons(trackerPortno);
	  
	  if(bind(peersockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) 
		     syserr("can't bind");
		printf("bind socket to port %d...\n", peerPortno);

	  //connect to server
	  if(connect(peersockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		 syserr("can't connect to tracker\n");
	  printf("connect...\n");
	  
	  //send all the files and directories within directory
	  DIR *dir;
	  struct dirent *ent;
	  dir = opendir("./");

	  if ((dir != NULL)) 
	  {	  	
		  	while ((ent = readdir (dir)) != NULL) 
			{
				strcpy(buffer, ent->d_name);
				printf("Peer is sending: %s\n", buffer);
			 	send(peersockfd, buffer, BUFFSIZE, 0);
		  	}
		  	closedir (dir);
			strcpy(buffer, "Done");
			send(peersockfd, buffer, 100, 0);
	  }
	  
	  for(;;)
	  {
			memset(buffer, 0, sizeof(buffer));
			//buffer = NULL;
			printf("Awaiting command or connection: ");
			fgets(buffer, sizeof(buffer), stdin);
		   int len = strlen(buffer);
	     	if (len>0 && buffer[len-1] == '\n')
	     	   buffer[len-1] = '\0';
		   printf("Buffer contains: %s\n", buffer);
		   
		   strcpy(command, strtok(buffer, " "));
		   printf("Buffer contains: %s\n", command);
		   
	      if (!strcmp(command, "list"))
         {
		      send(peersockfd, buffer, BUFFSIZE, 0);
		      recv(peersockfd, &buffer, BUFFSIZE, 0);
         }
         else if (!strcmp(command, "download"))
         {
		 
         }
         else if (!strcmp(command, "exit"))
         {
            //send exit message to server for interpretation
			   printf("Peer is exiting...\n");
			   send(peersockfd, buffer, 100, 0);
			   recv(peersockfd, &status, sizeof(int), 0);

			   //server sends back acknowledgement of the exit signal
	  		   if (status)
			   {
	   		   printf("Tracker removing peer from list...\n");
	   		   printf("Exit successful.\n");
	   		   exit(0);
			   }
			   else
	   		   printf("Failure to exit.\n");
         }
         else
         {
         
         }   			
	  } 
}
