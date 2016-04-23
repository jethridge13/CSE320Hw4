#define _GNU_SOURCE

#include "csapp.h"

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024
#define CURSOR "DFIServer> "
#define ENDVERB "\r\n\r\n"
#define WOLFIE "WOLFIE"
#define IAM "IAM "
#define BYE "BYE"

void help();
void users();
int sd();
void *login(void *connfd);
void *communicate();
void sigComm(int sig);
void usage();
void testPrint(char* string);

bool verbose = false;

int usersConnected = 0;

pthread_t commT;
bool commRun = false;

fd_set commfd;
int maxfd = -1;

struct user { 
	int connfd;
	time_t timeJoined;
	char name[80];
	char* ip;
	struct user *next;
};

struct user *HEAD, *userCursor;

int main(int argc, char **argv) {
	int opt, port, argumentsPassed = 0;
	int connfd;
	int clientlen;
	char* portNumber = NULL;
	char* motd = NULL;

	while((opt = getopt(argc, argv, "hv")) != -1) {
        switch(opt) {
            case 'h':
                /* The help menu was selected */
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
            	/* Verbose output selected */
            	verbose = true;
            	argumentsPassed++;
            	break;
        }
    }
	if(optind < argc && (argc - optind) == 2) {
		portNumber = argv[optind++];
		motd = argv[optind++];
    } else {
        if((argc - optind) <= 0) {
            fprintf(stderr, "%sMissing PORT_NUMBER and MOTD.\n", ERROR_TEXT);
        } else if((argc - optind) == 1) {
            fprintf(stderr, "%sMissing MOTD.\n", ERROR_TEXT);
        } else {
            fprintf(stderr, "%sToo many arguments provided.\n", ERROR_TEXT);
        }
        usage();
        exit(EXIT_FAILURE);
    }

    port = atoi(portNumber);
    if(!port){
    	fprintf(stderr, "%sInvalid PORT_NUMBER\n", ERROR_TEXT);
    	usage();
    }

    /* TODO Remove this 
    Maybe not. I kinda like it...*/
    if(verbose){
    	printf("%sPORT_NUMBER: %d\n%s", VERBOSE_TEXT, port, NORMAL_TEXT);
    	printf("%sMOTD: %s\n%s", VERBOSE_TEXT, motd, NORMAL_TEXT);
	}

	int listenfd;
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char *hostaddrp;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	if(listenfd < 0){
		printf("%sError creating socket.\n", ERROR_TEXT);
		return EXIT_FAILURE;
	}

	int optval = 1;

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
		(const void *)&optval , sizeof(int)) < 0) { 
		return EXIT_FAILURE;
	}

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;	/* Use the internet */
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons((unsigned short) port); /* Listen on this port */

	int ret = bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if(ret < 0){
		printf("%sError binding. Returned: %d\n", ERROR_TEXT, ret);
		return EXIT_FAILURE;
	}

	if(listen(listenfd, LISTENQ) < 0){
		printf("%sError creating socket listener.\n", ERROR_TEXT);
		//printf("%s\n", explain_bind(listenfd, serveraddr, sizeof(serveraddr)));
		return EXIT_FAILURE;
	}
	//printf("%d\n", listenfd);

	clientlen = sizeof(clientaddr);

	printf("%sCurrently listening on port %d.\n", NORMAL_TEXT, port);

	/* Initializing user list */
	HEAD = malloc(sizeof(struct user));
	HEAD->next = 0;
	userCursor = HEAD;

	int selectRet = 0;
	int stdinReady = 0;
	int connectReady = 0;
	fd_set fdRead;
	char cmd[MAX_LINE];
	clientlen = sizeof(struct sockaddr_storage);
	pthread_t tid;

	while(true){
		FD_ZERO(&fdRead);
		FD_SET(fileno(stdin), &fdRead);
		FD_SET(listenfd, &fdRead);

		selectRet = select(listenfd + 1, &fdRead, NULL, NULL, NULL);

		if(selectRet < 0){
			/* Something went wrong! */
		}

		stdinReady = FD_ISSET(fileno(stdin), &fdRead);
		connectReady = FD_ISSET(listenfd, &fdRead);
		/* Handle connection */
		if(connectReady){
			if(verbose){
				printf("%sSpawning login thread.\n%s", VERBOSE_TEXT, NORMAL_TEXT);
			}
			/*
			pid_t childID = fork();
			if(childID){
				childrenLogin++;
			} else {

			}
			*/
			connfd = accept(listenfd, (SA *)&clientaddr, (socklen_t*)&clientlen);
			if(connfd < 0){
				printf("%sError on accept.\n", ERROR_TEXT);
			}
			
			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
			if(hostp == NULL){

			}
			hostaddrp = inet_ntoa(clientaddr.sin_addr);
			if(hostaddrp == NULL){

			}
			printf("%sServer established connection with %s (%s)\n",
				NORMAL_TEXT, hostp->h_name, hostaddrp);

			pthread_create(&tid, NULL, login, &connfd);
			pthread_setname_np(tid, "LOGIN");
		}

		/* Handle STDIN */
		if(stdinReady){
			//printf("%s%s", NORMAL_TEXT, CURSOR);
			fgets(cmd, MAX_LINE, stdin);
			strtok(cmd, "\n");
			if(!strcmp(cmd, "/help")){
				help();
			} else if(!strcmp(cmd, "/shutdown")){
				return sd();
			} else if(!strcmp(cmd, "/users")){
				users();
			} else {
				help();
			}
		}

		selectRet = 0;
		stdinReady = 0;
		connectReady = 0;
	}
}

/* The method to handle logging on */
void *login(void *vargp){

	int connfd = *((int*)vargp);
	//pthread_t id = pthread_self();

	char buf[MAX_LINE];
	bzero(buf, MAX_LINE);

	read(connfd, buf, MAX_LINE);

	char* cmd = strstr(buf, ENDVERB);
	int charLeft = MAX_LINE;
	bool verbFound = false;
	bool connected = false;

	/* This bracket will check for verbs for the length of
	MAX_LINE. If MAX_LINE is exceeded and no verbs found, 
	will write an error to the client and disconnect them. */
	if(cmd == NULL){
		while(charLeft > 0){
			char buf2[charLeft];
			read(connfd, buf2, charLeft);
			strcpy(buf+strlen(buf), buf2);
			/*
			FILE *file;
			file = fopen("test.txt", "wt");
			fprintf(file, "%s", buf);
			fclose(file);
			*/
			cmd = strstr(buf, ENDVERB);
			if(cmd != NULL){
				verbFound = true;
				break;
			}
			charLeft = charLeft - strlen(buf);
		}
	} else {
		verbFound = true;
	}

	/* Verb found. Should be WOLFIE, then IAM. */
	if(verbFound){
		cmd = strstr(buf, WOLFIE);
		if(cmd != NULL){
			if(verbose){
				printf("%sWOLFIE recieved\n%s", VERBOSE_TEXT, NORMAL_TEXT);
			}
			char connect[] = "EIFLOW \r\n\r\n";
			write(connfd, connect, strlen(connect));
			if(verbose){
				printf("%sEIFLOW sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
			}

			bzero(buf, MAX_LINE);
			read(connfd, buf, MAX_LINE);
			cmd = strstr(buf, ENDVERB);
			charLeft = MAX_LINE;
			verbFound = false;
			if(cmd == NULL){
				while(charLeft > 0){
					char buf2[charLeft];
					read(connfd, buf2, charLeft);
					strcpy(buf+strlen(buf), buf2);

					cmd = strstr(buf, ENDVERB);
					if(cmd != NULL){
						verbFound = true;
						break;
					}
					charLeft = charLeft - strlen(buf);
				}
			} else {
				verbFound = true;
			}
			if(verbFound){
				cmd = strstr(buf, IAM);
				if(cmd != NULL){
					if(verbose){
						printf("%sIAM recieved\n%s", VERBOSE_TEXT, NORMAL_TEXT);
					}

					char name[strlen(buf)];
					memcpy(name, &buf[4], strlen(buf) - 2);
					char hi[] = "HI ";

					char* userName = strtok(name, ENDVERB);
					userName = strtok(userName, " ");

					char hiSend[strlen(name) + strlen(hi) + 4];
					strcpy(hiSend, hi);
					strcat(hiSend, name);
					strcat(hiSend, ENDVERB);

					bool uniqueName = true;

					/* Make user username is unique */
					userCursor = HEAD;
					if(usersConnected > 1){
						while(userCursor->next != 0){
							if(!strcmp(userName, userCursor->name)){
								uniqueName = false;
								break;
							}
						userCursor = userCursor->next;
						}
					} else if(usersConnected == 1){
						if(!strcmp(userName, userCursor->name)){
							uniqueName = false;
						}
					}
					
					/* If name user gave is unique */
					if(uniqueName){
						if(usersConnected){
							userCursor->next = malloc(sizeof(struct user));
							userCursor = userCursor->next;
							/* Initialize */
						} 

						strncpy(userCursor->name, userName, 80);
						userCursor->connfd = connfd;
						userCursor->timeJoined = time(0);
						userCursor->next = 0;

						write(connfd, hiSend, strlen(hiSend));
						testPrint(hiSend);
						usersConnected++;
						connected = true;

						if(verbose){
							printf("%sHI sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
						}
					} else {
						char error[] = "ERR 00 'Name not unique.' \r\n\r\n";
						write(connfd, error, strlen(error));
						write(connfd, BYE, strlen(BYE));
						close(connfd);
					}
				}
			} else {
				char error[] = "ERR 1 'Expected IAM.' \r\n\r\n";
				write(connfd, error, strlen(error));
			}
		} else {
			char error[] = "ERR 1 'Expected WOLFIE.' \r\n\r\n";
			write(connfd, error, strlen(error));
		}
	} else {
		char error[] = "ERR 1 'Verb not found.' \r\n\r\n";
		write(connfd, error, strlen(error));
	}

	if(!connected){
		close(connfd);
		pthread_exit(EXIT_SUCCESS);
		return NULL;
	}

	/* Update the communication thread 
	fd_set to listen to this client in 
	the communication thread. */
	FD_SET(connfd, &commfd);
	if(connfd > maxfd){
		maxfd = connfd;
	}

	if(!commRun){

		pthread_create(&commT, NULL, communicate, NULL);
		pthread_setname_np(commT, "COMMUNICATE");

		commRun = true;
	} else {
		/* Communication thread already running. 
		Send signal to interrupt thread. */
		pthread_kill(commT, SIGUSR1);
	}

	pthread_exit(EXIT_SUCCESS);
	return NULL;
}

void* communicate(){
	if(verbose){
		printf("%sCOMMUNICATION THREAD STARTED\n", VERBOSE_TEXT);
	}
	signal(SIGUSR1, sigComm);
	FD_ZERO(&commfd);
	int selectRet = 0;
	while(true){
		selectRet = select(maxfd + 1, &commfd, NULL, NULL, NULL);
		/* If select returns anything less than 0, that means
		It was interrupted by the signal sent when a login thread
		has a new fd to listen to. */
		if(selectRet > 0){
			write(1, "FD\n", 3);
			int i = 0;
			bool clientReady = false;
			char buf[MAX_LINE];
			/* Go through all possible fd */
			for(; i < FD_SETSIZE; i++){
				/* Check to see if the ith fd is triggered */
				if(FD_ISSET(i, &commfd)){
					/* Find out of that fd corresponds to a user */
					userCursor = HEAD;
					if(userCursor->connfd == i){
						clientReady = true;
					} else {
						while(userCursor->next != 0){
							userCursor = userCursor->next;
							if(userCursor->connfd == i){
								clientReady = true;
							}
						}
					}
				}
				/* If user is ready, handle user stuff */
				if(clientReady){
					char* cmd = strstr(buf, ENDVERB);
					int charLeft = MAX_LINE;
					bool verbFound = false;

					bzero(buf, MAX_LINE);
					read(i, buf, MAX_LINE);
					cmd = strstr(buf, ENDVERB);
					charLeft = MAX_LINE;
					verbFound = false;
					if(cmd == NULL){
						while(charLeft > 0){
							char buf2[charLeft];
							read(i, buf2, charLeft);
							strcpy(buf+strlen(buf), buf2);

							cmd = strstr(buf, ENDVERB);
							if(cmd != NULL){
								verbFound = true;
								break;
							}
							charLeft = charLeft - strlen(buf);
						}
					} else {
						verbFound = true;
					}
					if(verbFound){
						char test[] = "VERB FOUND!\n";
						write(i, test, strlen(test));
					}
				}
			}
		} 
		userCursor = HEAD;
		FD_ZERO(&commfd);
		FD_SET(userCursor->connfd, &commfd);
		while(userCursor->next != 0){
			userCursor = userCursor->next;
			FD_SET(userCursor->connfd, &commfd);
		}
	}
	return NULL;
}

void sigComm(int sig){
	signal(SIGUSR1, sigComm);
}

void users(){
	if(!usersConnected){
		printf("%sNo users connected.\n", NORMAL_TEXT);
	} else {
		userCursor = HEAD;
		int i = 0;
		printf("%s%d: %s 	FD: %d\n", 
			NORMAL_TEXT, ++i, userCursor->name, userCursor->connfd);

		while(userCursor->next != 0){
			userCursor = userCursor->next;

			printf("%s%d: %s	FD: %d\n", 
				NORMAL_TEXT, ++i, userCursor->name, userCursor->connfd);
		}
	}
}

int sd(){
	free(HEAD);
	return EXIT_SUCCESS;
}

void help(){
	printf(
		"%sUsage:\n"
		"/help		Displays this help menu.\n"
		"/shutdown	Disconnects all users and shuts down the server.\n"
		"/users		Displays all currently logged-on users.\n", NORMAL_TEXT);
}

void usage(){
	printf(
		"%s./server [-h|-v] PORT_NUMBER MOTD\n"
		"-h 			Displays help menu & returns EXIT_SUCCESS.\n"
		"-v 			Verbose print all incoming and outgoing protocol verbs & content.\n"
		"PORT_NUMBER 	Port number to listn on.\n"
		"MOTD 			Message to display to the client when they connect.\n", NORMAL_TEXT);
}

void testPrint(char* string){
	FILE *file;
	file = fopen("test.txt", "wt");
	fprintf(file, "%s", string);
	fclose(file);
}