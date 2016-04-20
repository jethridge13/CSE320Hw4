#define _GNU_SOURCE

#include "csapp.h"
#include <libexplain/bind.h>

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024
#define CURSOR "DFIServer> "

void help();
void users();
int sd();
void usage();

int main(int argc, char **argv) {
	int opt;
	bool verbose = false;
	int argumentsPassed = 0;
	char* portNumber = NULL;
	char* motd = NULL;
	int port;
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
    	printf("%sPORT_NUMBER: %d\n", VERBOSE_TEXT, port);
    	printf("%sMOTD: %s\n", VERBOSE_TEXT, motd);
	}

	int listenfd;
	int optval = 1;
	struct sockaddr_in serveraddr;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	if(listenfd == -1){
		printf("%sError creating socket.\n", ERROR_TEXT);
		return EXIT_FAILURE;
	}

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = port; 

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
		(const void *)&optval , sizeof(int)) < 0) { 
		return EXIT_FAILURE;
	}

	int ret = bind(listenfd, &serveraddr, sizeof(serveraddr));
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

	printf("%sCurrently listening on port %d.\n", NORMAL_TEXT, port);

	int selectRet = 0;
	int stdinReady = 0;
	int connectReady = 0;
	fd_set fdRead;
	char cmd[MAX_LINE];
	while(true){
		FD_ZERO(&fdRead);
		FD_SET(fileno(stdin), &fdRead);
		FD_SET(listenfd, &fdRead);

		selectRet = select(listenfd + 1, &fdRead, NULL, NULL, NULL);

		stdinReady = FD_ISSET(fileno(stdin), &fdRead);
		connectReady = FD_ISSET(listenfd, &fdRead);

		if(connectReady){
			printf("selectRet: %d\n", selectRet);
			printf("FD_ISSET(listenfd): %d\n", connectReady);
		}

		if(stdinReady){
			printf("%s%s", NORMAL_TEXT, CURSOR);
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