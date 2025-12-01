#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <pthread.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <strings.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

#define INDIM 30
#define OUTDIM 512


int numberThreads = 0;

pthread_rwlock_t rwlock;

struct sockaddr_un serv_addr;                                                     /* socket address */
int sockfd;                                                                       /* socket file descriptor */
socklen_t servlen;                                                                /* socket size */


void * applyCommands(){

    while (1) {
        struct sockaddr_un client_addr;
        char in_buffer[INDIM], out_buffer[OUTDIM];
        int c;
        char token, type;
        char name[MAX_INPUT_SIZE], newPath[MAX_INPUT_SIZE];

        servlen=sizeof(struct sockaddr_un);
        c = recvfrom(sockfd, in_buffer, sizeof(in_buffer)-1, 0,                   /* receives instruction from client */
            (struct sockaddr *)&client_addr, &servlen);
        if (c <= 0) continue;
        //Preventivo, caso o cliente nao tenha terminado a mensagem em '\0',
        in_buffer[c]='\0';

        int numTokens = sscanf(in_buffer, "%c %s %c", &token, name, &type);

        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            continue;
        }

        /* Parse newPath for move command */
        if (token == 'm') {
            numTokens = sscanf(in_buffer, "%c %s %s", &token, name, newPath);
            if (numTokens != 3) {
                fprintf(stderr, "Error: move command requires source and destination\n");
                continue;
            }
        }

        int result;
        switch (token) {
            case 'c':
                switch (type) {
                    case 'f':
                        printf("Create file: %s\n", name);
						pthread_rwlock_wrlock(&rwlock);                           /* activates lock (for writing) */
                        result = create(name, T_FILE);
						pthread_rwlock_unlock(&rwlock);                           /* deactivates lock */
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
						pthread_rwlock_wrlock(&rwlock);                           /* activates lock (for writing) */
                        result = create(name, T_DIRECTORY);
                        pthread_rwlock_unlock(&rwlock);                           /* deactivates lock */
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        return NULL;
                }
                break;
            case 'l':
				pthread_rwlock_rdlock(&rwlock);                                   /* activates lock (for writing) */
                result = lookup(name);
                pthread_rwlock_unlock(&rwlock);                                   /* deactivates lock */
                if (result >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':
            	printf("Delete: %s\n", name);
            	pthread_rwlock_wrlock(&rwlock);                                   /* activates lock (for writing) */
                result = delete(name);
                pthread_rwlock_unlock(&rwlock);                                   /* deactivates lock */
                break;
            case 'm':
                printf("Move: %s to %s\n", name, newPath);
                pthread_rwlock_wrlock(&rwlock);                                   /* activates lock (for writing) */
                result = move(name, newPath);
                pthread_rwlock_unlock(&rwlock);                                   /* deactivates lock */
                break;
            case 'p': ;
                FILE *output_file = fopen(name, "w");                             /* opens output file */
                if (output_file == NULL) {
                    fprintf(stderr, "Error: could not open output file %s\n", name);
                    result = FAIL;
                    break;
                }
                pthread_rwlock_rdlock(&rwlock);                                   /* activates lock (for reading) */
                print_tecnicofs_tree(output_file);                                /* prints TecnicoFS */
                pthread_rwlock_unlock(&rwlock);                                   /* deactivates lock */
                fclose(output_file);                                              /* closes output file */
                result = SUCCESS;
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                return NULL;
            }
        }

        c = sprintf(out_buffer, "%d", result);                                    /* cast int to be sent to client as char */
        sendto(sockfd, out_buffer, c+1, 0, (struct sockaddr *)&client_addr, servlen);
    }
}


/* manages various threads functionalities */
int threadsManager(){
    int i;
    pthread_t tid[numberThreads];                                                 /* declares threads pool */

    for (i=0; i<numberThreads; i++)                                               /* starts threads */
        pthread_create (&tid[i], NULL, applyCommands, NULL);
    for (i=0; i<numberThreads; i++)                                               /* waits for threads to complete tasks */
        pthread_join (tid[i], NULL);
    return 0;
}


int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  char buffer[OUTDIM];

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  sprintf(buffer, "/tmp/%s", path);
  unlink(buffer);
  strcpy(addr->sun_path, buffer);

  return SUN_LEN(addr);
}


int main(int argc, char* argv[]) {
    if (pthread_rwlock_init(&rwlock, NULL) != 0){                                 /* initializes rwlock */
        perror("rwlock: failed to initialize lock");
        exit(EXIT_FAILURE);
    }
    struct timeval begin, end;                                                    /* declares timers */
    if (argc == 3){                                                               /* verifies number of arguments */
        /* init filesystem */
        init_fs();

        if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {                      /* creates server socket */
            perror("server: can't open socket");
            exit(EXIT_FAILURE);
        }

        servlen = setSockAddrUn (argv[2], &serv_addr);
        if (bind(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0) {         /* names server socket */
            perror("server: bind error");
            exit(EXIT_FAILURE);
        }

        numberThreads = atoi(argv[1]);                                            /* retrieves number of threads */
        if (numberThreads <= 0){                                                  /* verifies if thread number is positive */
            perror("threads: less than 1");
            exit(EXIT_FAILURE);
        }
        gettimeofday(&begin, 0);                                                  /* gets time1 */
        threadsManager();                                                         /* starts processing input */
        gettimeofday(&end, 0);                                                    /* gets time2 */

        long seconds = end.tv_sec - begin.tv_sec;                                 /* computes execution time */
        long microseconds = end.tv_usec - begin.tv_usec;
        double elapsed = seconds + microseconds*1e-6;
        printf("TecnicoFS completed in %.4f seconds.\n", elapsed);

        /* release allocated memory */
        destroy_fs();
        if (pthread_rwlock_destroy(&rwlock) != 0){                                /* destroys rwlock */
            perror("rwlock: failed to destroy lock");
            exit(EXIT_FAILURE);
        }
    }
    else{                                                                         /* incorrect number of arguments */
        printf("Requires two arguments.\n");
        exit(EXIT_FAILURE);
	}
    close(sockfd);
    unlink(serv_addr.sun_path);
    exit(EXIT_SUCCESS);
}
