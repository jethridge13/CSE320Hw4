#define _GNU_SOURCE

#include "csapp.h"
#include "sfwrite.c"

#define MAX_LINE 1024

int main(int argc, char** argv){
	int chatfd = atoi(argv[1]);
	char* fromName = argv[2];
	char* toName = argv[3];
    pthread_mutex_t stdoutMutex = PTHREAD_MUTEX_INITIALIZER;

	sfwrite(&stdoutMutex, stdout, "Now chatting with %s\n", toName);

    /*MULTIPLEX*/
    int selectRet = 0;
    int stdinReady = 0;
    int listenReady = 0;
    fd_set fdRead;

    char* token;
    char* fromPtr;
    char* msgPtr;

    char output[MAX_LINE];
    char input[MAX_LINE];

	while(true){
        FD_ZERO(&fdRead);
        FD_SET(fileno(stdin), &fdRead);
        FD_SET(chatfd, &fdRead);

        selectRet = select(FD_SETSIZE, &fdRead, NULL, NULL, NULL);

        if(selectRet < 0){
            /* Something went wrong! */
        }

        stdinReady = FD_ISSET(fileno(stdin), &fdRead);
        listenReady = FD_ISSET(chatfd, &fdRead);

        /* Handle connection */
        if(listenReady) {
        	memset(output, 0, sizeof(output));
            read(chatfd, output, MAX_LINE);

            if(strstr(output, "MSG ") == output) {
                strtok(output, " \r\n");
                token = strtok(NULL, " \r\n");
                token = strtok(NULL, " \r\n");
                fromPtr = token;
                token = strtok(NULL, "\r\n");
                msgPtr = token;
                if(!strcmp(fromPtr, toName)) {
	                sfwrite(&stdoutMutex, stdout, "< ", 2);
	                sfwrite(&stdoutMutex, stdout, msgPtr, strlen(msgPtr));
	                sfwrite(&stdoutMutex, stdout, "\n", 2);
	            }
	            else {
                    sfwrite(&stdoutMutex, stdout, "> ", 2);
                    sfwrite(&stdoutMutex, stdout, msgPtr, strlen(msgPtr));
                    sfwrite(&stdoutMutex, stdout, "\n", 2);
	            }
	        }
        }

        /* Handle STDIN */
        if(stdinReady) {
            if(fgets(input, MAX_LINE - 6, stdin) != NULL) {
                if(!strcmp(input, "\n")) {
                }
                if(!strcmp(input, "/close\n")) {
                    close(chatfd);
                    return EXIT_SUCCESS;
                }
                else {
                    write(chatfd, "MSG ", 4);
                    write(chatfd, toName, strlen(toName));
                    write(chatfd, " ", 1);
                    write(chatfd, fromName, strlen(fromName));
                    write(chatfd, " ", 1);
                    write(chatfd, input, strlen(input));
                    write(chatfd, " \r\n\r\n", 5);
                }
            }
        }

        selectRet = 0;
        stdinReady = 0;
        listenReady = 0;
    }
}