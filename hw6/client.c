#define _GNU_SOURCE

#include "csapp.h"

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024

void usage();

bool verbose = false;
bool chatOpen = false;
pid_t childID;


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

    char output[MAX_LINE];
    char loginError[] = "Error logging in! Exiting.\n";

    /*LOG IN*/
    write(sockfd, "WOLFIE \r\n\r\n", 11);
    if(verbose){
        char wolfieSent[] = "WOLFIE sent\n";
        write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
        write(1, wolfieSent, strlen(wolfieSent));
    }
    memset(output, 0, sizeof(output));
    read(sockfd, output, MAX_LINE);
    if(strstr(output, "EIFLOW ") == output && verbose) {
        char eiflowReceived[] = "EIFLOW received\n";
        write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
        write(1, eiflowReceived, strlen(eiflowReceived));
    }
    if(strstr(output, "EIFLOW ") != output) {
        write(1, loginError, strlen(loginError));
        return EXIT_FAILURE;
    }
    /*CREATE NEW USER*/
    if(createUser) {
        memset(output, 0, sizeof(output));
        strcat(output, "IAMNEW ");
        strcat(output, name);
        strcat(output, " \r\n\r\n\0");
        write(sockfd, output, strlen(output));
        if(verbose){
            char iamnewSent[] = "IAMNEW sent\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, iamnewSent, strlen(iamnewSent));
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "HINEW ") == output && verbose) {
            char hinewReceived[] = "HINEW received\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, hinewReceived, strlen(hinewReceived));
        }
        if(strstr(output, "HINEW ") != output) {
            char accExists[] = "Account with username already exists or already logged in!\n";
            write(1, accExists, strlen(accExists));
            if(verbose)
                write(1, output, strlen(output));
            return EXIT_FAILURE;
        }

        char createPass[] = "Create a password (Must be min 5 char, 1 uppercase, 1 symbol, 1 number):\n";
        write(1, createPass, strlen(createPass));

        char pass[MAX_LINE - 20];
        fgets(pass, MAX_LINE - 26, stdin);
        memset(output, 0, sizeof(output));
        strcat(output, "NEWPASS ");
        strcat(output, pass);
        strcat(output, " \r\n\r\n");
        write(sockfd, output, strlen(output));
        if(verbose){
            char newpassSent[] = "NEWPASS sent\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, newpassSent, strlen(newpassSent));
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "SSAPWEN ") == output && verbose) {
            char ssapwenReceived[] = "SSAPWEN received\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, ssapwenReceived, strlen(ssapwenReceived));
            char hiReceived[] = "HI received\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, hiReceived, strlen(hiReceived));
        }
        if(strstr(output, "SSAPWEN ") != output) {
            char badPass[] = "Improper password!\n";
            write(1, badPass, strlen(badPass));
            if(verbose)
                write(1, output + 12, strlen(output - 12));
            return EXIT_FAILURE;
        }
        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        char loginSuccess[] = "\nLogged in! Message of the Day:\n";
        write(1, loginSuccess, strlen(loginSuccess));
        char* motdPtr = strstr(output, "MOTD ");
        while(motdPtr == NULL){
            memset(output, 0, sizeof(output));
            read(sockfd, output, MAX_LINE);
            motdPtr = strstr(output, "MOTD ");
        }
        motdPtr += 5;
        strtok(motdPtr, "\r\n");
        write(1, motdPtr, strlen(motdPtr));
        write(1, "\n", 1);
    }

    /*ELSE LOG IN*/
    else {
        memset(output, 0, sizeof(output));
        strcat(output, "IAM ");
        strcat(output, name);
        strcat(output, " \r\n\r\n");
        write(sockfd, output, strlen(output));
        if(verbose){
            char iamSent[] = "IAM sent\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, iamSent, strlen(iamSent));
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "AUTH ") == output && verbose) {
            char authReceived[] = "AUTH received\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, authReceived, strlen(authReceived));
        }
        if(strstr(output, "AUTH ") != output) {
            char noUser[] = "Account with username doesn't exist or already logged in!\n";
            write(1, noUser, strlen(noUser));
            if(verbose)
                write(1, output, strlen(output));
            return EXIT_FAILURE;
        }

        char enterPass[] = "Enter password: ";
        write(1, enterPass, strlen(enterPass));

        char pass[MAX_LINE - 20];
        fgets(pass, MAX_LINE - 26, stdin);
        memset(output, 0, sizeof(output));
        strcat(output, "PASS ");
        strcat(output, pass);
        strcat(output, " \r\n\r\n\0");
        write(sockfd, output, strlen(output));
        if(verbose){
            char newpassSent[] = "PASS sent\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, newpassSent, strlen(newpassSent));
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "SSAP ") == output && verbose) {
            char ssapReceived[] = "SSAP received\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, ssapReceived, strlen(ssapReceived));
            char hiReceived[] = "HI received\n";
            write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
            write(1, hiReceived, strlen(hiReceived));
            write(1, output, strlen(output));
        }
        if(strstr(output, "SSAP ") != output) {
            char badPass[] = "Failed to log in!\n";
            write(1, badPass, strlen(badPass));
            if(verbose)
                write(1, output + 9, strlen(output - 9));
            return EXIT_FAILURE;
        }
        char* motdPtr = strstr(output, "MOTD ");
        while(motdPtr == NULL){
            memset(output, 0, sizeof(output));
            read(sockfd, output, MAX_LINE);
            motdPtr = strstr(output, "MOTD ");
        }
        motdPtr += 5;
        strtok(motdPtr, "\r\n");
        write(1, motdPtr, strlen(motdPtr));
        write(1, "\n", 1);
    }

    /*MULTIPLEX*/
    int selectRet = 0;
    int stdinReady = 0;
    int listenReady = 0;
    int chatReady = 0;
    fd_set fdRead;

    int timeInt, hours, minutes, seconds = 0;
    char* token;
    char* toPtr;
    char* fromPtr;
    char* msgPtr;

    int sv[2]; //SOCKETPAIR FD
    int chatfd = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    if(chatfd < 0){
        printf("%sError creating socket.\n", ERROR_TEXT);
        return EXIT_FAILURE;
    }

    while(true){
        FD_ZERO(&fdRead);
        FD_SET(fileno(stdin), &fdRead);
        FD_SET(sockfd, &fdRead);
        FD_SET(sv[0], &fdRead);

        selectRet = select(FD_SETSIZE, &fdRead, NULL, NULL, NULL);

        if(selectRet < 0){
            /* Something went wrong! */
        }

        stdinReady = FD_ISSET(fileno(stdin), &fdRead);
        listenReady = FD_ISSET(sockfd, &fdRead);
        chatReady = FD_ISSET(sv[0], &fdRead);

        /* Handle connection */
        if(listenReady) {
            memset(output, 0, sizeof(output));
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
                close(sockfd);
                kill(childID, SIGKILL);
                return EXIT_SUCCESS;
            }
            else if(strstr(output, "UTSIL ") == output) {
                if(verbose){
                    char utsilReceived[] = "UTSIL received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, utsilReceived, strlen(utsilReceived));
                }
                strtok(output, " \r\n");
                token = strtok(NULL, " \r\n");
                while(token != NULL) {
                    printf("%s\n", token);
                    token = strtok(NULL, " \r\n");
                }
            }
            else if(strstr(output, "MSG ") == output) {
                if(verbose){
                    char msgReceived[] = "MSG received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, msgReceived, strlen(msgReceived));
                }

                char outputCpy[strlen(output)];
                strcpy(outputCpy, output);
                strtok(output, " \r\n");
                token = strtok(NULL, " \r\n");
                toPtr = token;
                token = strtok(NULL, " \r\n");
                fromPtr = token;

                if(chatOpen == false) {
                    childID = fork();
                    if(childID == 0){
                        close(sv[0]);

                        char fdStr[12];
                        snprintf(fdStr, 12, "%i", sv[1]);
                        printf("%s", fdStr);

                        int status = 1;

                        if(!strcmp(fromPtr, name)) {
                            char* xterm[] = {"xterm", "-geometry", "45x35+0", "-T", toPtr, "-e", "./chat", fdStr, fromPtr, toPtr, NULL};
                            status = execv("/usr/bin/xterm", xterm);
                        }
                        else {
                            char* xterm[] = {"xterm", "-geometry", "45x35+500", "-T", fromPtr, "-e", "./chat", fdStr, toPtr, fromPtr, NULL};
                            status = execv("/usr/bin/xterm", xterm);
                        }

                        if(status){
                            char* statusLine = "Couldn't create chat window!\n";
                            write(1, statusLine, strlen(statusLine));
                            kill(getpid(), SIGKILL);
                        }
                        chatOpen = true;
                    }
                    else {
                        close(sv[1]);
                        chatOpen = true;
                    }
                }
                write(sv[0], outputCpy, strlen(outputCpy));
            }
            else {
                write(1, output, strlen(output));
            }
        }

        /*Handle Chat*/
        if(chatReady) {
            memset(output, 0, sizeof(output));
            read(sv[0], output, MAX_LINE);
            if(strstr(output, "MSG ") == output) {
                if(verbose){
                    char msgReceived[] = "MSG received\n";
                    write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                    write(1, msgReceived, strlen(msgReceived));
                }
            write(sockfd, output, strlen(output));
            }
        }

        /* Handle STDIN */
        if(stdinReady) {
            if(fgets(input, MAX_LINE - 6, stdin) != NULL) {
                if(*input == '\n')
                    continue;
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
                        "/listu         Lists all clients connected to server.\n"
                        "/chat <TO> <MESSAGE>         Starts chat.\n");
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
                else if(strstr(input, "/chat") == input) {
                    strtok(input, " \r\n");
                    token = strtok(NULL, " \r\n");
                    if(token != NULL)
                        toPtr = token;
                    else {
                        printf("Invalid chat format. Enter /help for a list of commands.\n");
                        fflush(stdout);
                        continue;
    
                    }
                    token = strtok(NULL, "\r\n");
                    if(token != NULL)
                        msgPtr = token;
                    else {
                        printf("Invalid chat format. Enter /help for a list of commands.\n");
                        fflush(stdout);
                        continue;
                    }
                    if(verbose){
                        char listuSent[] = "MSG sent\n";
                        write(1, VERBOSE_TEXT, strlen(VERBOSE_TEXT));
                        write(1, listuSent, strlen(listuSent));
                    }
                    write(sockfd, "MSG ", 4);
                    write(sockfd, toPtr, strlen(toPtr));
                    write(sockfd, " ", 1);
                    write(sockfd, name, strlen(name));
                    write(sockfd, " ", 1);
                    write(sockfd, msgPtr, strlen(msgPtr));
                    write(sockfd, " \r\n\r\n\0", 6);
                }
                else
                    printf("%s\n", "Invalid command. Enter /help for a list of commands.");
            }
            else {
                strcat(input, " \r\n\r\n\0");
                write(sockfd, input, strlen(input)+1);
            }
        }

        selectRet = 0;
        stdinReady = 0;
        listenReady = 0;
        chatReady = 0;
    }
}

void usage(){
	printf(
		"\n\x1B[0m ./client [-hcv] NAME SERVER_IP SERVER_PORT\n"
        "-a FILE        Path to the audit log file.\n"
		"-h 			Displays this help menu and returns EXIT_SUCCESS.\n"
		"-c 			Requests to server to create a new user.\n"
		"-v 			Verbose print all incoming and outgoing protocl verbs & content.\n"
		"NAME 			This is the username to display when chatting.\n"
		"SERVER_IP 		The ip address of the server to connect to.\n"
		"SERVER_PORT 	The port to connect to.\n\n\n");
}