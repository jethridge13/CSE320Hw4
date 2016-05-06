#define _GNU_SOURCE

#include "sfwrite.c"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <semaphore.h>

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024
#define CURSOR "DFIServer> "
#define ENDVERB "\r\n\r\n"
#define WOLFIE "WOLFIE"
#define IAM "IAM "
#define IAMNEW "IAMNEW "
#define NEWPASS "NEWPASS "
#define PASS "PASS "
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
void accounts();
int sd();
bool validPassword(char* password);
void *login(void *connfd);
void *communicate();
void sigComm(int sig);
void sigShutDown();
void usage();
void testPrint(char* string);
void testPrintInt(int i);

/* TODO Make the program catch kill9 thingy */

bool verbose = false;

int usersConnected = 0;
int accountsInFile = 0;

bool acctFileAvailable = false;
FILE *accountfd = NULL;

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

struct account {
	char name[80];
	char pwd[65];
	unsigned char salt[5];
	struct account *next;
	struct account *prev;
};

struct user *HEAD, *userCursor, *userCursorPrev;

struct account *AHEAD, *AuserCursor, *AuserCursorPrev;

char* motd = NULL;
char* accountFile = NULL;

sem_t lists;

pthread_mutex_t stdoutMutex = PTHREAD_MUTEX_INITIALIZER;

void P(sem_t *s){
	sem_wait(s);
}

void V(sem_t *s){
	sem_post(s);
}

int main(int argc, char **argv) {

	sem_init(&lists, 0, 1);

	signal(SIGINT, sigShutDown);
	signal(SIGKILL, sigShutDown);

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
    int addArgs = argc - optind;
	if(optind < argc && (addArgs == 3 || addArgs == 2)) {
		portNumber = argv[optind++];
		motd = argv[optind++];
		accountFile = argv[optind++];
    } else {
        if(addArgs <= 0) {
            sfwrite(&stdoutMutex, stderr, "%sMissing PORT_NUMBER and MOTD.\n", ERROR_TEXT);
        } else if(addArgs == 1) {
            sfwrite(&stdoutMutex, stderr, "%sMissing MOTD.\n", ERROR_TEXT);
        } else if (addArgs > 3){
            sfwrite(&stdoutMutex, stderr, "%sToo many arguments provided.\n", ERROR_TEXT);
        }
        usage();
        exit(EXIT_FAILURE);
    }

    port = atoi(portNumber);
    if(!port){
    	sfwrite(&stdoutMutex, stderr, "%sInvalid PORT_NUMBER\n", ERROR_TEXT);
    	usage();
    	return EXIT_FAILURE;
    }

    /* Initializing account list */
	AHEAD = malloc(sizeof(struct account));
	AHEAD->next = 0;
	AHEAD->prev = 0;
	AuserCursor = AHEAD;

    if(accountFile != NULL){
    	struct stat dfe;
    	int fe = stat(accountFile, &dfe);
    	bool fileExists = !fe;
    	if(fileExists){
    		acctFileAvailable = true;
			accountfd = fopen(accountFile, "r");
			char* readBuffer = NULL;
			size_t length = 0;
			int whileRead = getdelim(&readBuffer, &length, 10, accountfd);
			while(whileRead > 0) {
				char* accountName = strtok(readBuffer, "\t");
				char* passWord = strtok(NULL, "\n");
				if(accountsInFile){
					AuserCursor->next = malloc(sizeof(struct account));
					AuserCursorPrev = AuserCursor;
					AuserCursor = AuserCursor->next;
					AuserCursor->prev = AuserCursorPrev;
				} 
										
				strncpy(AuserCursor->name, accountName, 80);
				AuserCursor->next = 0;
				strncpy(AuserCursor->pwd, passWord, 80);

				accountsInFile++;

				readBuffer = NULL;
				length = 0;

				whileRead = getdelim(&readBuffer, &length, 10, accountfd);
			}
			fclose(accountfd);
    	} else {
    		accountFile = NULL;
    		sfwrite(&stdoutMutex, stdout, "%sACCOUNTS_FILE does not exists. Using no file instead.\n", ERROR_TEXT);
    	}
	}
    if(verbose){
    	sfwrite(&stdoutMutex, stdout, "%sPORT_NUMBER: %d\n%s", VERBOSE_TEXT, port, NORMAL_TEXT);
    	sfwrite(&stdoutMutex, stdout, "%sMOTD: %s\n%s", VERBOSE_TEXT, motd, NORMAL_TEXT);
    	sfwrite(&stdoutMutex, stdout, "%sACCOUNTS_FILE: %s\n%s", VERBOSE_TEXT, accountFile, NORMAL_TEXT);
	}

	int listenfd;
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char *hostaddrp;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	if(listenfd < 0){
		sfwrite(&stdoutMutex, stdout, "%sError creating socket.\n", ERROR_TEXT);
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
		sfwrite(&stdoutMutex, stdout, "%sError binding. Returned: %d\n", ERROR_TEXT, ret);
		return EXIT_FAILURE;
	}

	if(listen(listenfd, LISTENQ) < 0){
		sfwrite(&stdoutMutex, stdout, "%sError creating socket listener.\n", ERROR_TEXT);
		//sfwrite(&stdoutMutex, stdout, "%s\n", explain_bind(listenfd, serveraddr, sizeof(serveraddr)));
		return EXIT_FAILURE;
	}
	//sfwrite(&stdoutMutex, stdout, "%d\n", listenfd);

	clientlen = sizeof(clientaddr);

	sfwrite(&stdoutMutex, stdout, "%sCurrently listening on port %d.\n", NORMAL_TEXT, port);

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
				sfwrite(&stdoutMutex, stdout, "%sSpawning login thread.\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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
				sfwrite(&stdoutMutex, stdout, "%sError on accept.\n", ERROR_TEXT);
			}
			
			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
			if(hostp == NULL){

			}
			hostaddrp = inet_ntoa(clientaddr.sin_addr);
			if(hostaddrp == NULL){

			}
			sfwrite(&stdoutMutex, stdout, "%sServer established connection with %s (%s)\n",
				NORMAL_TEXT, hostp->h_name, hostaddrp);

			pthread_create(&tid, NULL, login, &connfd);
			pthread_setname_np(tid, "LOGIN");
		}

		/* Handle STDIN */
		if(stdinReady){
			//sfwrite(&stdoutMutex, stdout, "%s%s", NORMAL_TEXT, CURSOR);
			fgets(cmd, MAX_LINE, stdin);
			strtok(cmd, "\n");
			if(!strcmp(cmd, "/help")){
				help();
			} else if(!strcmp(cmd, "/shutdown")){
				return sd();
			} else if(!strcmp(cmd, "/users")){
				users();
			} else if(!strcmp(cmd, "/accts")){
				accounts();
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
				sfwrite(&stdoutMutex, stdout, "%sWOLFIE received\n%s", VERBOSE_TEXT, NORMAL_TEXT);
			}
			char connect[] = "EIFLOW \r\n\r\n";
			write(connfd, connect, strlen(connect));
			if(verbose){
				sfwrite(&stdoutMutex, stdout, "%sEIFLOW sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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
						sfwrite(&stdoutMutex, stdout, "%sIAM received\n%s", VERBOSE_TEXT, NORMAL_TEXT);
					}

					char name[strlen(buf)];
					memcpy(name, &buf[4], strlen(buf) - 2);
					char hi[] = "HI ";

					char* userName = strtok(name, ENDVERB);
					userName = strtok(userName, " ");

					char hiSend[strlen(name) + strlen(hi) + 6];
					strcpy(hiSend, hi);
					strcat(hiSend, name);
					strcat(hiSend, ENDVERB);
					//strcat(hiSend, "\0");

					bool acctExists = false;

					/* Make user accountname is unique */
					/* Make sure the lists are locked */
					P(&lists);
					AuserCursor = AHEAD;
					if(accountsInFile > 1){
						while(AuserCursor->next != 0){
							if(!strcmp(userName, AuserCursor->name)){
								acctExists = true;
								break;
							}
						AuserCursor = AuserCursor->next;
						}
					} else if(accountsInFile == 1){
						if(!strcmp(userName, AuserCursor->name)){
							acctExists = true;
						}
					}

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
					V(&lists);
					/* Unlock the lists */

					bool passwordMatch = false;

					if(uniqueName && acctExists){
						char auth[strlen(buf)];
						memcpy(auth, &buf[4], strlen(buf) - 2);
						char hi[] = "AUTH ";

						char authSend[strlen(auth) + strlen(hi) + 6];
						strcpy(authSend, hi);
						strcat(authSend, name);
						strcat(authSend, ENDVERB);

						write(connfd, authSend, strlen(authSend));

						if(verbose){
							sfwrite(&stdoutMutex, stdout, "%sAUTH sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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
							cmd = strstr(buf, PASS);
							if(cmd != NULL){
								char pwd[strlen(buf)];
								memcpy(pwd, &buf[4], strlen(buf) - 2);
								char hi[] = "SSAP ";

								char* password = strtok(pwd, ENDVERB);
								password = strtok(password, " ");

								char passSend[strlen(pwd) + strlen(hi) + 6];
								strcpy(passSend, hi);
								strcat(passSend, pwd);
								strcat(passSend, ENDVERB);

								unsigned char salt[5];
								strcpy((char*)salt, (char*)AuserCursor->salt);

								char passwordHash[strlen(password) + 5];
								strcpy(passwordHash, password);
								//strcat(passwordHash, (char*) salt);

								unsigned char hash[SHA256_DIGEST_LENGTH];
								SHA256_CTX sha256;
								SHA256_Init(&sha256);
								SHA256_Update(&sha256, password, strlen(password));
								SHA256_Final(hash, &sha256);

								char outputBuffer[65];
								int i = 0;
								for(; i < SHA256_DIGEST_LENGTH; i++){
									sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
								}
								outputBuffer[64] = 0;

								if(!strcmp(outputBuffer, AuserCursor->pwd)){
									passwordMatch = true;
									write(connfd, passSend, strlen(passSend));
								}
							}
						}
					}
					
					/* If name user gave is unique 
						and acct exists */
					if(uniqueName && acctExists && passwordMatch){
						/* Lock the lists */
						P(&lists);
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
						V(&lists);
						/* Unlock the lists */

						char message[MAX_LINE];
						char motdverb[] = "MOTD ";

						strcpy(message, motdverb);
						strcat(message, motd);
						strcat(message, ENDVERB);

						write(connfd, hiSend, strlen(hiSend));
						write(connfd, message, strlen(message));
                        write(connfd, "\n\n", 2);
						usersConnected++;
						connected = true;

						if(verbose){
							sfwrite(&stdoutMutex, stdout, "%sHI sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
						}
					} else {
						if(!uniqueName){
							char error[] = "ERR 00 'USER NAME TAKEN.' \r\n\r\n";
							write(connfd, error, strlen(error));
							write(connfd, BYE, strlen(BYE));
							close(connfd);
						} else if(!acctExists){
							char error[] = "ERR 01 'USER NOT AVAILABLE.' \r\n\r\n";
							write(connfd, error, strlen(error));
							write(connfd, BYE, strlen(BYE));
							close(connfd);
						} else {
							char error[] = "ERR 02 'BAD PASSWORD.' \r\n\r\n";
							write(connfd, error, strlen(error));
							write(connfd, BYE, strlen(BYE));
							close(connfd);
						}
					}
				} else {
					cmd = strstr(buf, IAMNEW);
					if(cmd != NULL){
						if(verbose){
							sfwrite(&stdoutMutex, stdout, "%sIAMNEW received\n%s", VERBOSE_TEXT, NORMAL_TEXT);
						}
						char name[strlen(buf)];
						memcpy(name, &buf[7], strlen(buf) - 2);
						char hi[] = "HINEW ";

						char* userName = strtok(name, ENDVERB);
						userName = strtok(userName, " ");

						char hiSend[strlen(name) + strlen(hi) + 6];
						strcpy(hiSend, hi);
						strcat(hiSend, name);
						strcat(hiSend, ENDVERB);
						/* hiSend now contains HINEW <name> <ENDVERB> */

						bool notDuplicate = true;

						/* Lock the lists */
						P(&lists);
						if(accountsInFile > 0){
							notDuplicate = true;
							AuserCursor = AHEAD;
							if(!strcmp(userName, AuserCursor->name)){
								notDuplicate = false;
							}
							while(AuserCursor->next != 0){
								AuserCursor = AuserCursor->next;
								if(!strcmp(userName, AuserCursor->name)){
									notDuplicate = false;
								}
							}
						} 
						V(&lists);

						if(notDuplicate) {
							write(connfd, hiSend, strlen(hiSend));
							if(verbose){
								sfwrite(&stdoutMutex, stdout, "%sHINEW sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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
								cmd = strstr(buf, NEWPASS);
								if(cmd != NULL){
									if(verbose){
										sfwrite(&stdoutMutex, stdout, "%sNEWPASS received\n%s", VERBOSE_TEXT, NORMAL_TEXT);
									}
									char pass[strlen(buf)];
									memcpy(pass, &buf[7], strlen(buf) - 2);
									char hi[] = "NEWPASS";

									char* passWord = strtok(pass, ENDVERB);
									passWord = strtok(passWord, " ");

									char passSend[strlen(pass) + strlen(hi) + 4];
									strcpy(passSend, hi);
									strcat(passSend, pass);
									strcat(passSend, ENDVERB);

									/* LET'S ADD SOME SALT BEFORE WE HASH */
									unsigned char salt[5];
									RAND_bytes(salt, 5);

									char passwordHash[strlen(passWord) + 5];
									strcpy(passwordHash, passWord);
									//strcat(passwordHash, (char*)salt);
									/*
									testPrint((char*)salt);
									testPrint(passwordHash);
									*/
									/* HASHING */
									unsigned char hash[SHA256_DIGEST_LENGTH];
									SHA256_CTX sha256;
									SHA256_Init(&sha256);
									SHA256_Update(&sha256, passWord, strlen(passWord));
									SHA256_Final(hash, &sha256);

									char outputBuffer[65];
									int i = 0;
									for(; i < SHA256_DIGEST_LENGTH; i++){
										sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
									}
									outputBuffer[64] = 0;

									/* Test if password meets criteria */
									bool valid = validPassword(passWord);

									if(valid){
										char validP[] = "SSAPWEN \r\n\r\n";
										write(connfd, validP, strlen(validP));
										if(verbose){
											sfwrite(&stdoutMutex, stdout, "%sSSAPWEN sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
										}

										char hi[] = "HI ";
										char hiSend[strlen(name) + strlen(hi) + strlen(ENDVERB) + 1];
										strcpy(hiSend, hi);
										strcat(hiSend, name);
										strcat(hiSend, ENDVERB);
										strcat(hiSend, "\0");

										/* Lock the lists */
										P(&lists);
										/* Initialize user */
										if(usersConnected){
											userCursor->next = malloc(sizeof(struct user));
											userCursorPrev = userCursor;
											userCursor = userCursor->next;
											userCursor->prev = userCursorPrev;
										} 
										
										strncpy(userCursor->name, userName, 80);
										userCursor->connfd = connfd;
										userCursor->timeJoined = time(0);
										userCursor->next = 0;

										/* Initialize account */
										if(accountsInFile){
											AuserCursor->next = malloc(sizeof(struct user));
											AuserCursorPrev = AuserCursor;
											AuserCursor = AuserCursor->next;
											AuserCursor->prev = AuserCursorPrev;
										} 
										
										strncpy(AuserCursor->name, userName, 80);
										AuserCursor->next = 0;
										strncpy(AuserCursor->pwd, outputBuffer, 65);
										strncpy((char*)(AuserCursor->salt), (char*)salt, 5);
										V(&lists);
										/* Unlock the lists */

										write(connfd, hiSend, strlen(hiSend));
										if(verbose){
											sfwrite(&stdoutMutex, stdout, "%sHI sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
										}

										char message[MAX_LINE];
										char motdverb[] = "MOTD ";

										strcpy(message, motdverb);
										strcat(message, motd);
										strcat(message, ENDVERB);

										write(connfd, message, strlen(message));
										write(connfd, "\n\n", 2);
										usersConnected++;
										accountsInFile++;
										connected = true;
									} else {
										char error[] = "ERR 02 'BAD PASSWORD' \r\n\r\n";
										write(connfd, error, strlen(error));
									}
								} else {
									char error[] = "ERR 100 'Expected NEWPASS' \r\n\r\n";
									write(connfd, error, strlen(error));
								}
							}
						} else {
							char error[] = "ERR 00 'USER NAME TAKEN' \r\n\r\n";
							write(connfd, error, strlen(error));
						}
					}
				}
			} else {
				char error[] = "ERR 100 'Expected IAM or IAMNEW' \r\n\r\n";
				write(connfd, error, strlen(error));
			}
		} else {
			char error[] = "ERR 100 'Expected WOLFIE.' \r\n\r\n";
			write(connfd, error, strlen(error));
		}
	} else {
		char error[] = "ERR 100 'Verb not found.' \r\n\r\n";
		write(connfd, error, strlen(error));
	}

	if(!connected){
		write(connfd, BYE, strlen(BYE));
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
		sfwrite(&stdoutMutex, stdout, "%sCOMMUNICATION THREAD STARTED\n", VERBOSE_TEXT);
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
					/* Lock the lists */
					//P(&lists);
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
					//V(&lists);
					/* Unlock the lists */
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
							/* Lock the lists */
							P(&lists);
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
									sfwrite(&stdoutMutex, stdout, "%sUOFF sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
								}
								while(userCursor->next != 0){
									userCursor = userCursor->next;
									write(userCursor->connfd, uoff, strlen(uoff));
									if(verbose){
										sfwrite(&stdoutMutex, stdout, "%sUOFF sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
									}
								}
							}
							V(&lists);
						}
						cmd = strstr(buf, MSG);
						if(cmd != NULL){
							/* Can just recycle the MSG <TO> <FROM> <MESSAGE>
							received from the client and forward it to both. */

							/* MSG verb found */
							if(verbose){
								char received[] = "MSG received\n";
								write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
								write(1, received, strlen(received));
							}

							char bufCpy[MAX_LINE];
							strcpy(bufCpy, buf);

							char* token = strtok(buf, " ");
							/* token holds MSG */
							token = strtok(NULL, " ");
							/* token holds <TO> */
							char to[80];
							strcpy(to, token);
							token = strtok(NULL, " ");
							/* token holds <FROM> */
							char from[80];
							strcpy(from, token);

							bool toFound = false;
							bool fromFound = false;

							userCursor = HEAD;
							if(!strcmp(userCursor->name, to)){
								userCursorPrev = userCursor;
								toFound = true;
							}
							while (!toFound && userCursor->next != 0){
								userCursor = userCursor->next;
								if(!strcmp(userCursor->name, to)){
									userCursorPrev = userCursor;
									toFound = true;
								}
							}
							if(!toFound){
								char error[] = "ERR 100 '<TO> User not found.' \r\n\r\n";
								write(i, error, strlen(error));
							} else {
								userCursor = HEAD;
								if(!strcmp(userCursor->name, from)){
									fromFound = true;
								}
								while (!fromFound && userCursor->next != 0){
									userCursor = userCursor->next;
									if(!strcmp(userCursor->name, from)){
										fromFound = true;
									}
								}
								fromFound = true;
								if(!fromFound){
									char error[] = "ERR 100 '<FROM> User not found.' \r\n\r\n";
									write(i, error, strlen(error));
								}
							}
							if(toFound && fromFound){
								write(userCursorPrev->connfd, bufCpy, strlen(bufCpy));
								write(i, bufCpy, strlen(bufCpy));

								if(verbose){
									char sent[] = "MSG sent\n";
									write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
									write(1, sent, strlen(sent));
								}
							}
						}
					}
				}
			}
		}
		if(!usersConnected){
			FD_ZERO(&commfd);
			if(verbose){
				sfwrite(&stdoutMutex, stdout, "%sCOMMUNICATION THREAD ENDED\n", VERBOSE_TEXT);
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

void sigShutDown(){
	exit(sd());
}

bool validPassword(char* password){
	bool valid = true;
	bool cap = false;
	bool symbol = false;
	bool number = false;
	char *caps = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *symbols = "!@#$^&*()_+-=~`<>,.?;'{}[]|";
	char *numbers = "0123456789";
	if(strlen(password) < 5){
		valid = false;
		return false;
	}
	while(*password){
		if(strchr(caps, *password)){
			cap = true;
		}
		if(strchr(symbols, *password)){
			symbol = true;
		}
		if(strchr(numbers, *password)){
			number = true;
		}
		password++;
	}

	valid = cap && symbol && number;
	return valid;
}

void users(){
	if(!usersConnected){
		sfwrite(&stdoutMutex, stdout, "%sNo users connected.\n", NORMAL_TEXT);
	} else {
		userCursor = HEAD;
		int i = 0;
		sfwrite(&stdoutMutex, stdout, "%s%d: %s 	FD: %d\n", 
			NORMAL_TEXT, ++i, userCursor->name, userCursor->connfd);

		while(userCursor->next != 0){
			userCursor = userCursor->next;

			sfwrite(&stdoutMutex, stdout, "%s%d: %s	FD: %d\n", 
				NORMAL_TEXT, ++i, userCursor->name, userCursor->connfd);
		}
	}
}

void accounts(){
	if(!accountsInFile){
		write(1, NORMAL_TEXT, strlen(NORMAL_TEXT));
		char string[] = "No accounts available.\n";
		write(1, string, strlen(string));
	} else {
		AuserCursor = AHEAD;
		int i = 0;
		sfwrite(&stdoutMutex, stdout, "%s%d: %s\n", NORMAL_TEXT, ++i, AuserCursor->name);

		while(AuserCursor->next != 0){
			AuserCursor = AuserCursor->next;
			sfwrite(&stdoutMutex, stdout, "%s%d: %s\n", NORMAL_TEXT, ++i, AuserCursor->name);
		}
	}
}

int sd(){
	P(&lists);
	sfwrite(&stdoutMutex, stdout, "%sSHUTTING DOWN\n", NORMAL_TEXT);
	userCursor = HEAD;
	if(usersConnected > 0){
		write(userCursor->connfd, BYE, strlen(BYE));
		if(verbose){
			sfwrite(&stdoutMutex, stdout, "%sBYE sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
		}
		while(userCursor->next != 0){
			userCursor = userCursor->next;
			write(userCursor->connfd, BYE, strlen(BYE));
			if(verbose){
				sfwrite(&stdoutMutex, stdout, "%sBYE sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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
	if(acctFileAvailable){
		accountfd = fopen(accountFile, "w");
		AuserCursor = AHEAD;
		if(accountsInFile > 0){
			write(fileno(accountfd), AuserCursor->name, strlen(AuserCursor->name));
			write(fileno(accountfd), "\t", 1);
			write(fileno(accountfd), AuserCursor->pwd, strlen(AuserCursor->pwd));
			//write(fileno(accountfd), "\t", 1);
			//write(fileno(accountfd), AuserCursor->salt, strlen((char*)AuserCursor->salt));
			write(fileno(accountfd), "\n", 1);
			while(AuserCursor->next != 0){
				AuserCursor = AuserCursor->next;

				write(fileno(accountfd), AuserCursor->name, strlen(AuserCursor->name));
				write(fileno(accountfd), "\t", 1);
				write(fileno(accountfd), AuserCursor->pwd, strlen(AuserCursor->pwd));
				//write(fileno(accountfd), "\t", 1);
				//write(fileno(accountfd), AuserCursor->salt, strlen((char*)AuserCursor->salt));
				write(fileno(accountfd), "\n", 1);
			}
		}
		fclose(accountfd);
	}
	AuserCursor = AHEAD;
	if(usersConnected > 0){
		while(AuserCursor->prev != 0){
			while(AuserCursor->next != 0){
				AuserCursor = AuserCursor->next;
			}
			AuserCursor = AuserCursor->prev;
			free(AuserCursor->next);
		}
	}
	free(AHEAD);
	if(commRun){
		pthread_cancel(commT);
	}
	V(&lists);
	return EXIT_SUCCESS;
}

void help(){
	sfwrite(&stdoutMutex, stdout, 
		"%sUsage:\n"
		"/accts 	Displays all accounts loaded.\n"
		"/help		Displays this help menu.\n"
		"/shutdown	Disconnects all users and shuts down the server.\n"
		"/users		Displays all currently logged-on users.\n", NORMAL_TEXT);
}

void usage(){
	sfwrite(&stdoutMutex, stdout, 
		"%s./server [-hv] [-t THREAD_COUNT] PORT_NUMBER MOTD [ACCOUNTS_FILE]\n"
		"-h 				Displays help menu & returns EXIT_SUCCESS.\n"
		"-t THREAD_COUNT	The number of threads used for the login queue.\n"
		"-v 				Verbose print all incoming and outgoing protocol verbs & content.\n"
		"PORT_NUMBER 		Port number to listn on.\n"
		"MOTD 				Message to display to the client when they connect.\n"
		"ACCOUNTS_FILE 		File containing username and password data to be loaded upon execution.\n", NORMAL_TEXT);
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