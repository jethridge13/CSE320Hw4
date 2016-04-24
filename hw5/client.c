#define _GNU_SOURCE

#include "csapp.h"

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024

void usage();

bool verbose = false;


void testPrint(char* string){
    FILE *file;
    file = fopen("test.txt", "a");
    fprintf(file, "%s", string);
    fclose(file);
}

int main(int argc, char** argv){
	int opt, port = 0;
	bool createUser = false;
	char* name = NULL;
	char* serverIP = NULL;
	char* serverPort = NULL;
	while((opt = getopt(argc, argv, "hcv")) != -1) {
        switch(opt) {
            case 'h':
                /* The help menu was selected */
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
            	/* Verbose output selected */
            	verbose = true;
            	break;
            case 'c':
            	createUser = true;
            	break;
        }
    }
	if(optind < argc && (argc - optind) == 3) {
        name = argv[optind++];
        serverIP = argv[optind++];
        serverPort = argv[optind++];
    } else {
        if((argc - optind) <= 0) {
            fprintf(stderr, "\x1B[1;31mMissing NAME and SERVER_IP and SERVER_PORT.\n");
        } else if((argc - optind) == 1) {
            fprintf(stderr, "\x1B[1;31mMissing SERVER_IP and SERVER_PORT.\n");
        } else if ((argc - optind) == 2) {
        	fprintf(stderr, "\x1B[1;31mMissing SERVER_PORT\n");
        }else {
            fprintf(stderr, "\x1B[1;31mToo many arguments provided.\n");
        }
        usage();
        exit(EXIT_FAILURE);
    }

    port = atoi(serverPort);
    if(!port){
        fprintf(stderr, "%sInvalid PORT_NUMBER\n", ERROR_TEXT);
        usage();
    }

    /* TODO Remove this */
    if(verbose){
    	printf("\x1B[1;34mNAME: %s\n", name);
    	if(createUser){
    		printf("CREATE NEW USER");
    	}
    	printf("SERVER_IP: %s\n", serverIP);
    	printf("SERVER_PORT: %s\n", serverPort);
    }

    char input[MAX_LINE]; //CLIENT IO COMMANDS

    int sockfd;
    struct sockaddr_in serveraddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0){
        printf("%sError creating socket.\n", ERROR_TEXT);
        return EXIT_FAILURE;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((unsigned short) port);
    inet_pton(AF_INET,serverPort, &(serveraddr.sin_addr));
 
    if(connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
        printf("%sCould not connect to server.\n", ERROR_TEXT);
        return EXIT_FAILURE;
    }

    int selectRet = 0;
    int stdinReady = 0;
    int listenReady = 0;
    char output[MAX_LINE];
    fd_set fdRead;

    int timeInt, hours, minutes, seconds = 0;
    char* usertoken;

    while(true){
        FD_ZERO(&fdRead);
        FD_SET(fileno(stdin), &fdRead);
        FD_SET(sockfd, &fdRead);

        selectRet = select(sockfd + 1, &fdRead, NULL, NULL, NULL);

        if(selectRet < 0){
            /* Something went wrong! */
        }

        stdinReady = FD_ISSET(fileno(stdin), &fdRead);
        listenReady = FD_ISSET(sockfd, &fdRead);

        /* Handle connection */
        if(listenReady) {
            memset(&output, 0, sizeof(output));
            read(sockfd, output, MAX_LINE);
            if(strstr(output, "EMIT ") == output) {
                if(verbose){
                    char emitReceived[] = "EMIT received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, emitReceived, strlen(emitReceived));
                }
                strtok(output, " \r\n");
                timeInt = atoi(strtok(NULL, " \r\n"));
                hours = timeInt / 3600;
                timeInt = timeInt % 3600;
                minutes = timeInt / 60;
                seconds = timeInt % 60;
                printf("%s%i%s%i%s%i%s", "Connected for ", hours, " hour(s), ", minutes, " minute(s), and ", seconds, " second(s)\n");
            }
            else if(strstr(output, "BYE \r\n\r\n") != NULL) {
                if(verbose){
                    char byeReceived[] = "BYE received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, byeReceived, strlen(byeReceived));
                }
                return EXIT_SUCCESS;
            }
            else if(strstr(output, "UTSIL ") == output) {
                if(verbose){
                    char utsilReceived[] = "UTSIL received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, utsilReceived, strlen(utsilReceived));
                }
                strtok(output, " \r\n");
                usertoken = strtok(NULL, " \r\n");
                while(usertoken != NULL) {
                    printf("%s\n", usertoken);
                    usertoken = strtok(NULL, " \r\n");
                }
            }
            else {
                if(strstr(output, "EIFLOW ") == output && verbose) {
                    char eiflowReceived[] = "EIFLOW received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, eiflowReceived, strlen(eiflowReceived));
                }
                if(strstr(output, "HI ") == output && verbose) {
                    char hiReceived[] = "HI received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, hiReceived, strlen(hiReceived));
                }
                write(1, output, sizeof(output));
            }
        }

        /* Handle STDIN */
        if(stdinReady) {
            if(fgets(input, MAX_LINE - 6, stdin) != NULL) {
                if(*input == '/') {
                    if(!strcmp(input, "/time\n")) {
                        if(verbose){
                            char timeSent[] = "TIME sent\n";
                            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                            write(1, timeSent, strlen(timeSent));
                        }
                        write(sockfd, "TIME \r\n\r\n\0", 10);
                    }
                    else if(!strcmp(input, "/help\n")) {
                        printf(
                            "CLIENT COMMANDS:\n"
                            "/time          Displays how long the client has been connected to server.\n"
                            "/help          Lists all client commands. The thing you just typed in.\n"
                            "/logout        Disconnects client from server.\n"
                            "/listu         Lists all clients connected to server.\n");
                    }
                    else if(!strcmp(input, "/logout\n")) {
                        if(verbose){
                            char byeSent[] = "BYE sent\n";
                            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                            write(1, byeSent, strlen(byeSent));
                        }
                        write(sockfd, "BYE \r\n\r\n\0", 9);
                    }
                    else if(!strcmp(input, "/listu\n")) {
                        if(verbose){
                            char listuSent[] = "LISTU sent\n";
                            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                            write(1, listuSent, strlen(listuSent));
                        }
                        write(sockfd, "LISTU \r\n\r\n\0", 11);
                    }
                    else
                        printf("%s\n", "Invalid command. Enter /help for a list of commands.");
                }
                else {
                    if(verbose){
                        if(!strcmp(input, "WOLFIE\n")) {
                            char wolfieSent[] = "WOLFIE sent\n";
                            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                            write(1, wolfieSent, strlen(wolfieSent));
                        }
                        if(strstr(input, "IAM") != NULL) {
                            char iamSent[] = "IAM sent\n";
                            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                            write(1, iamSent, strlen(iamSent));
                        }
                    }
                    strcat(input, " \r\n\r\n\0");
                    write(sockfd, input, strlen(input)+1);
                }
            }
        }

        selectRet = 0;
        stdinReady = 0;
        listenReady = 0;
    }
}

void usage(){
	printf(
		"\n\x1B[0m ./client [-hcv] NAME SERVER_IP SERVER_PORT\n"
		"-h 			Displays this help menu and returns EXIT_SUCCESS.\n"
		"-c 			Requests to server to create a new user.\n"
		"-v 			Verbose print all incoming and outgoing protocl verbs & content.\n"
		"NAME 			This is the username to display when chatting.\n"
		"SERVER_IP 		The ip address of the server to connect to.\n"
		"SERVER_PORT 	The port to connect to.\n\n\n");
}