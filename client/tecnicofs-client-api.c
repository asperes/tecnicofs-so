#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#define OUTDIM 512

struct sockaddr_un client_addr;
int sockfdClient;
socklen_t clilen;

struct sockaddr_un serv_addr;
int sockfdServer;
socklen_t servlen;


int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}

int tfsCreate(char *filename, char nodeType) {
  	char instruction[MAX_INPUT_SIZE];
  	int sizeI;

  	sizeI = sprintf(instruction, "c %s %c", filename, nodeType);

	if (sendto(sockfdClient, instruction, sizeI+1, 0, (struct sockaddr *)         /* sends instruction to server */ 
			&serv_addr, servlen) < 0) {
    	perror("client: sendto error");
    	exit(EXIT_FAILURE);
  	}

	int c = recvfrom(sockfdClient, instruction, sizeof(instruction)-1, 0,        /* receives instruction from server */
            (struct sockaddr *)&serv_addr, &servlen);
	if (c < 0) {
		perror("client: recvfrom error");
		exit(EXIT_FAILURE);
	}
	instruction[c] = '\0';

	int result = atoi(instruction);
  	return result;
}

int tfsDelete(char *path) {
  	char instruction[MAX_INPUT_SIZE];
  	int sizeI;

  	sizeI = sprintf(instruction, "d %s", path);

	if (sendto(sockfdClient, instruction, sizeI+1, 0, (struct sockaddr *)         /* sends instruction to server */  
			&serv_addr, servlen) < 0) {
    	perror("client: sendto error");
    	exit(EXIT_FAILURE);
  	}

	int c = recvfrom(sockfdClient, instruction, sizeof(instruction)-1, 0,        /* receives instruction from server */
            (struct sockaddr *)&serv_addr, &servlen);
	if (c < 0) {
		perror("client: recvfrom error");
		exit(EXIT_FAILURE);
	}
	instruction[c] = '\0';

	int result = atoi(instruction);
  	return result;
}

int tfsMove(char *from, char *to) {
  char instruction[MAX_INPUT_SIZE];
  int sizeI;

  sizeI = sprintf(instruction, "m %s %s", from, to);

  if (sendto(sockfdClient, instruction, sizeI+1, 0, (struct sockaddr *)          /* sends instruction to server */
      &serv_addr, servlen) < 0) {
    perror("client: sendto error");
    exit(EXIT_FAILURE);
  }

  int c = recvfrom(sockfdClient, instruction, sizeof(instruction)-1, 0,          /* receives instruction from server */
          (struct sockaddr *)&serv_addr, &servlen);
  if (c < 0) {
    perror("client: recvfrom error");
    exit(EXIT_FAILURE);
  }
  instruction[c] = '\0';

  int result = atoi(instruction);
  return result;
}

int tfsLookup(char *path) {
  	char instruction[MAX_INPUT_SIZE];
  	int sizeI;

  	sizeI = sprintf(instruction, "l %s", path);

  	if (sendto(sockfdClient, instruction, sizeI+1, 0, (struct sockaddr *)         /* sends instruction to server */  
  		&serv_addr, servlen) < 0) {
      	perror("client: sendto error");
      	exit(EXIT_FAILURE);
    }

  	int c = recvfrom(sockfdClient, instruction, sizeof(instruction)-1, 0,        /* receives instruction from server */
    	(struct sockaddr *)&serv_addr, &servlen);
	if (c < 0) {
		perror("client: recvfrom error");
		exit(EXIT_FAILURE);
	}
	instruction[c] = '\0';

	int result = atoi(instruction);
  	return result;
}

void tfsPrint(char * path) {                                                      /* print current filesystem */
	char instruction[MAX_INPUT_SIZE];
  	int sizeI;

  	sizeI = sprintf(instruction, "p %s", path);

  	if (sendto(sockfdClient, instruction, sizeI+1, 0, (struct sockaddr *)         /* sends instruction to server */  
  		&serv_addr, servlen) < 0) {
      	perror("client: sendto error");
      	exit(EXIT_FAILURE);
    }
}

int tfsMount(char * sockPath) {

    char serverSocket[OUTDIM];
    sprintf(serverSocket, "/tmp/%s", sockPath);
  	servlen = setSockAddrUn (serverSocket, &serv_addr);                           /* stores location of server socket */

    int pid = getpid();                                                           /* gets process ID of client */
    char clientSocket[OUTDIM];
    sprintf(clientSocket, "/tmp/client-%d", pid);                                 /* stores name for client socket */

    if ((sockfdClient = socket(AF_UNIX, SOCK_DGRAM, 0) ) < 0) {                   /* creates client socket */
    	perror("client: can't open socket");
    	exit(EXIT_FAILURE);
  	}
  	unlink(clientSocket);
  	clilen = setSockAddrUn (clientSocket, &client_addr);
  	if (bind(sockfdClient, (struct sockaddr *) &client_addr, clilen) < 0) {       /* names client socket */
    	perror("client: bind error");
    	exit(EXIT_FAILURE);
  	}

  	return 0;
}

int tfsUnmount() {
  if (close(sockfdClient) < 0){
    perror("client: close error");
    exit(EXIT_FAILURE);
  }
  if (unlink(client_addr.sun_path) < 0){
    perror("client: unlink error");
    exit(EXIT_FAILURE);
  }
  return 0;
}
