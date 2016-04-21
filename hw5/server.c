#define _GNU_SOURCE

#include "csapp.h"

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024
#define CURSOR "DFIServer> "
#define ENDVERB "\r\n\r\n"
#define WOLFIE "WOLFIE "
#define IAM "IAM "

void help();
void users();
int sd();
void *login(void *connfd);
void usage();

bool verbose = false;

int main(int argc, char **argv) {
	int opt, /*childrenLogin = 0, */ port, argumentsPassed = 0;
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

    /* TODO Remove this */
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
/* TODO Save users and fds */
void *login(void *vargp){

	int connfd = *((int*)vargp);
	//pthread_t id = pthread_self();

	char buf[MAX_LINE];
	bzero(buf, MAX_LINE);

	read(connfd, buf, MAX_LINE);

	char* cmd = strstr(buf, ENDVERB);
	int charLeft = MAX_LINE;
	bool verbFound = false;

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

					/* TODO This does not yet extract the name,
					but sends back HI */
					char name[strlen(buf)];
					memcpy(name, &buf[3], strlen(buf) - 2);
					char hi[] = "HI ";

					char hiSend[strlen(name) + strlen(hi)];
					strcpy(hiSend, hi);
					strcat(hiSend, name);

					write(connfd, hiSend, strlen(hiSend));

					if(verbose){
						printf("%sHI sent\n%s", VERBOSE_TEXT, NORMAL_TEXT);
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

	close(connfd);
	pthread_exit(EXIT_SUCCESS);
	return NULL;
}

void users(){

}

int sd(){
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