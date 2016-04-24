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
#define BYE "BYE \r\n\r\n\0"
#define BYE_NO_PRO "BYE "
#define TIME "TIME"
#define EMIT "EMIT "
#define LISTU "LISTU"
#define UTSIL "UTSIL "
#define MSG "MSG"
#define UOFF "UOFF "

void help();
void users();
int sd();
void *login(void *connfd);
void *communicate();
void sigComm(int sig);
void usage();
void testPrint(char* string);
void testPrintInt(int i);

/* TODO Replace bzero with memset */
/* TODO Fix communication thread for when all users disconnect */

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
	struct user *prev;
};

struct user *HEAD, *userCursor, *userCursorPrev;

char* motd = NULL;

int main(int argc, char **argv) {
	int opt, port, argumentsPassed = 0;
	int connfd;
	int clientlen;
	char* portNumber = NULL;

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
	HEAD->prev = 0;
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
				printf("%sWOLFIE received\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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
						printf("%sIAM received\n%s", VERBOSE_TEXT, NORMAL_TEXT);
					}

					char name[strlen(buf)];
					memcpy(name, &buf[4], strlen(buf) - 2);
					char hi[] = "HI ";

					char* userName = strtok(name, ENDVERB);
					userName = strtok(userName, " ");

					char hiSend[strlen(name) + strlen(hi) + 4 + 1];
					strcpy(hiSend, hi);
					strcat(hiSend, name);
					strcat(hiSend, ENDVERB);
					strcat(hiSend, "\0");

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
							userCursorPrev = userCursor;
							userCursor = userCursor->next;
							userCursor->prev = userCursorPrev;
						} 
						/* Initialize */
						strncpy(userCursor->name, userName, 80);
						userCursor->connfd = connfd;
						userCursor->timeJoined = time(0);
						userCursor->next = 0;

						write(connfd, hiSend, strlen(hiSend));
						write(connfd, motd, strlen(motd));
						//testPrint(hiSend);
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
				char error[] = "ERR 00 'Expected IAM.' \r\n\r\n";
				write(connfd, error, strlen(error));
			}
		} else {
			char error[] = "ERR 00 'Expected WOLFIE.' \r\n\r\n";
			write(connfd, error, strlen(error));
		}
	} else {
		char error[] = "ERR 00 'Verb not found.' \r\n\r\n";
		write(connfd, error, strlen(error));
	}

	if(!connected){
		close(connfd);
		pthread_exit(EXIT_SUCCESS);
		return NULL;
	}

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
	int selectRet = 0;
	while(true){
		userCursor = HEAD;
		/*
		FD_ZERO(&commfd);
		userCursor = HEAD;
		FD_SET(userCursor->connfd, &commfd);
		while(userCursor->next != 0){
			userCursor = userCursor->next;
			FD_SET(userCursor->connfd, &commfd);
		}
		*/
		int i = 0;
		for(;i < usersConnected; i++){
			FD_SET(userCursor->connfd, &commfd);
			userCursor = userCursor->next;
		}
		userCursor = HEAD;
		/*
		i = 0;
		for(;i < maxfd + 1; i++){
			testPrintInt(i);
			char test1[] = "\t";
			char t[] = "TRUE";
			char f[] = "FALSE";
			char nl[] = "\n";
			testPrint(test1);
			if(FD_ISSET(i, &commfd)){
				testPrint(t);
			} else {
				testPrint(f);
			}
			testPrint(nl);
		}
		*/
		selectRet = select(maxfd + 1, &commfd, NULL, NULL, NULL);
		/* If select returns anything less than 0, that means
		It was interrupted by the signal sent when a login thread
		has a new fd to listen to. */
		if(selectRet > 0){
			int i = 0;
			bool clientReady = false;
			char buf[MAX_LINE];
			/* Go through all possible fd */
			for(; i < maxfd + 1; i++){
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
						cmd = strstr(buf, TIME);
						if(cmd != NULL){
							/* TIME verb found */
							if(verbose){
								char timeReceived[] = "TIME received\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, timeReceived, strlen(timeReceived));
							}
							userCursor = HEAD;
							bool clientFound = false;
							while(!clientFound){
								if(userCursor->connfd != i){
									userCursor = userCursor->next;
								} else {
									clientFound = true;
								}
							}
							time_t timeToSend = time(0) - userCursor->timeJoined;
							/* Note: This will not work if the user
							is connected for more than 317 years. 
							Maybe add in another buffer 317 years from now.
							I need to amuse myself somehow. */
							char timeBuffer[10];
							sprintf(timeBuffer, "%d", (int) timeToSend);

							char bufferToSend[strlen(EMIT) + strlen(timeBuffer) + strlen(ENDVERB) + 1];
							strcpy(bufferToSend, EMIT);
							strcat(bufferToSend, timeBuffer);
							strcat(bufferToSend, ENDVERB);
							strcat(bufferToSend, "\0");

							write(i, bufferToSend, strlen(bufferToSend));
							if(verbose){
								char timeSent[] = "EMIT sent\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, timeSent, strlen(timeSent));
							}
						}
						cmd = strstr(buf, LISTU);
						if(cmd != NULL){
							/* LISTU verb found */
							if(verbose){
								char listuReceived[] = "LISTU received\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, listuReceived, strlen(listuReceived));
							}
							char userBuffer[MAX_LINE];
							memset(userBuffer, 0, MAX_LINE);
							userCursor = HEAD;
							strcpy(userBuffer, UTSIL);
							strcat(userBuffer, userCursor->name);
							strcat(userBuffer, "\r\n");
							strcat(userBuffer, "\0");
							while(userCursor->next != 0){
								userCursor = userCursor->next;
								strcat(userBuffer, userCursor->name);
								strcat(userBuffer, "\r\n");
							}
							strcat(userBuffer, ENDVERB);

							write(i, userBuffer, strlen(userBuffer));
							if(verbose){
								char sent[] = "UTSIL sent\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, sent, strlen(sent));
							}
						}
						cmd = strstr(buf, BYE_NO_PRO);
						if(cmd != NULL){
							/* BYE verb found */
							if(verbose){
								char received[] = "BYE received\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, received, strlen(received));
							}
							userCursor = HEAD;
							bool clientFound = false;
							while(!clientFound){
								if(userCursor-> connfd != i){
									userCursor = userCursor->next;
								} else {
									clientFound = true;
								}
							}
							write(i, BYE, strlen(BYE));
							if(verbose){
								char sent[] = "BYE sent\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, sent, strlen(sent));
							}

							/* store the name for UOFF */
							char userName[80];
							strcpy(userName, userCursor->name);

							/* Close everything */
							//FD_CLR(userCursor->connfd, &commfd);
							close(userCursor->connfd);
							if(usersConnected > 1){
								if(userCursor->prev != 0){
									userCursorPrev = userCursor->prev;
									userCursorPrev->next = userCursor->next;
								} else {
									HEAD = userCursor->next;
									HEAD->prev = 0;
								}
								free(userCursor);
								usersConnected--;
							} else {
								close(userCursor->connfd);
								usersConnected--;
							}
							/* Send UOFF to all remaining users */
							if(usersConnected > 0){
								char uoff[strlen(UOFF) + strlen(userName) + strlen(ENDVERB)];
								strcpy(uoff, UOFF);
								strcat(uoff, userName);
								strcat(uoff, ENDVERB);
								userCursor = HEAD;
								write(userCursor->connfd, uoff, strlen(uoff));
								if(verbose){
									printf("%sUOFF sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
								}
								while(userCursor->next != 0){
									userCursor = userCursor->next;
									write(userCursor->connfd, uoff, strlen(uoff));
									if(verbose){
										printf("%sUOFF sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
									}
								}
							}
						}
						cmd = strstr(buf, MSG);
						if(cmd != NULL){
							/* MSG verb found */

						}
					}
				}
			}
		}
		if(!usersConnected){
			FD_ZERO(&commfd);
			if(verbose){
				printf("%sCOMMUNICATION THREAD ENDED\n", VERBOSE_TEXT);
			}
			commRun = false;
			pthread_exit(EXIT_SUCCESS);
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
	userCursor = HEAD;
	if(usersConnected > 0){
		write(userCursor->connfd, BYE, strlen(BYE));
		if(verbose){
			printf("%sBYE sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
		}
		while(userCursor->next != 0){
			userCursor = userCursor->next;
			write(userCursor->connfd, BYE, strlen(BYE));
			if(verbose){
				printf("%sBYE sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
			}
		}
	}
	userCursor = HEAD;
	if(usersConnected > 0){
		while(userCursor->prev !=0){
			while(userCursor->next != 0){
				userCursor = userCursor->next;
			}
			close(userCursor->connfd);
			userCursor = userCursor->prev;
			free(userCursor->next);
		}
	}
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
	file = fopen("test.txt", "a");
	fprintf(file, "%s", string);
	fclose(file);
}

void testPrintInt(int i){
	char buffer[5];
	sprintf(buffer, "%d", i);
	testPrint(buffer);
}