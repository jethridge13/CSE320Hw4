#define _GNU_SOURCE

#include "csapp.h"
#include "sfwrite.c"

#define NORMAL_TEXT "\x1B[0m"
#define ERROR_TEXT "\x1B[1;31m"
#define VERBOSE_TEXT "\x1B[1;34m"
#define MAX_LINE 1024

void usage();

bool verbose = false;
bool chatOpen = false;
pid_t childID;
pthread_mutex_t stdoutMutex = PTHREAD_MUTEX_INITIALIZER;
int sv[2]; //SOCKETPAIR FD
int chatfd;

void testPrint(char* string){
    FILE *file;
    file = fopen("test.txt", "a");
    fprintf(file, "%s", string);
    fclose(file);
}

void chatClose(int sig) {
    chatOpen = false;
    chatfd = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
}

int main(int argc, char** argv){
	int opt, port = 0;
	bool createUser = false;
    bool createAudit = false;
	char* name = NULL;
	char* serverIP = NULL;
	char* serverPort = NULL;
    char* auditFile = NULL;
    char auditBuffer[200];
    FILE* audit = NULL;
    signal(SIGCHLD, chatClose);
    chatfd = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

	while((opt = getopt(argc, argv, "a:hcv")) != -1) {
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
            case 'a':
                createAudit = true;
                auditFile = optarg;
                break;
        }
    }

	if(optind < argc && (argc - optind) == 3) {
        name = argv[optind++];
        serverIP = argv[optind++];
        serverPort = argv[optind++];
    } else {
        if((argc - optind) <= 0) {
            sfwrite(&stdoutMutex, stderr, "\x1B[1;31mMissing NAME and SERVER_IP and SERVER_PORT.\n");
        } else if((argc - optind) == 1) {
            sfwrite(&stdoutMutex, stderr, "\x1B[1;31mMissing SERVER_IP and SERVER_PORT.\n");
        } else if ((argc - optind) == 2) {
        	sfwrite(&stdoutMutex, stderr, "\x1B[1;31mMissing SERVER_PORT\n");
        }else {
            sfwrite(&stdoutMutex, stderr, "\x1B[1;31mToo many arguments provided.\n");
        }
        usage();
        exit(EXIT_FAILURE);
    }

    port = atoi(serverPort);
    if(!port){
        sfwrite(&stdoutMutex, stderr, "%sInvalid PORT_NUMBER\n", ERROR_TEXT);
        usage();
    }

    /* TODO Remove this */
    if(verbose){
    	sfwrite(&stdoutMutex, stdout, "\x1B[1;34mNAME: %s\n", name);
    	if(createUser){
    		sfwrite(&stdoutMutex, stdout, "CREATE NEW USER");
    	}
    	sfwrite(&stdoutMutex, stdout, "SERVER_IP: %s\n", serverIP);
    	sfwrite(&stdoutMutex, stdout, "SERVER_PORT: %s\n", serverPort);
    }

    if(createAudit == true) {
        audit = fopen(auditFile, "a+");
    }
    else {
        audit = fopen("audit.log", "a+");
    }

    char timeStr[50];
    time_t curtime;
    struct tm * timeStruct;

    char input[MAX_LINE]; //CLIENT IO COMMANDS

    int sockfd;
    struct sockaddr_in serveraddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0){
        sfwrite(&stdoutMutex, stderr, "%sError creating socket.\n", ERROR_TEXT);
        return EXIT_FAILURE;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((unsigned short) port);
    inet_pton(AF_INET,serverPort, &(serveraddr.sin_addr));
 
    if(connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
        sfwrite(&stdoutMutex, stderr, "%sCould not connect to server.\n", ERROR_TEXT);
        return EXIT_FAILURE;
    }

    char output[MAX_LINE];
    char loginError[] = "Error logging in! Exiting.\n";

    /*LOG IN*/
    write(sockfd, "WOLFIE \r\n\r\n", 11);
    if(verbose){
        char wolfieSent[] = "WOLFIE sent\n";
        sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
        sfwrite(&stdoutMutex, stdout, wolfieSent);
    }
    memset(output, 0, sizeof(output));
    read(sockfd, output, MAX_LINE);
    if(strstr(output, "EIFLOW ") == output && verbose) {
        char eiflowReceived[] = "EIFLOW received\n";
        sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
        sfwrite(&stdoutMutex, stdout, eiflowReceived);
    }
    if(strstr(output, "EIFLOW ") != output) {
        sfwrite(&stdoutMutex, stdout, loginError);

        curtime = time(NULL);
        timeStruct = localtime(&curtime);
        strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
        sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, fail, %s\n", timeStr, name, serverIP, serverPort, strtok(output, "\r\n"));
        write(fileno(audit), auditBuffer, strlen(auditBuffer));

        fclose(audit);
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
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, iamnewSent);
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "HINEW ") == output && verbose) {
            char hinewReceived[] = "HINEW received\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, hinewReceived);
        }
        if(strstr(output, "HINEW ") != output) {
            char accExists[] = "Account with username already exists or already logged in!\n";
            sfwrite(&stdoutMutex, stdout, accExists);
            if(verbose)
                sfwrite(&stdoutMutex, stdout, output);

            curtime = time(NULL);
            timeStruct = localtime(&curtime);
            strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
            sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, fail, %s\n", timeStr, name, serverIP, serverPort, strtok(output, "\r\n"));
            write(fileno(audit), auditBuffer, strlen(auditBuffer));

            fclose(audit);
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
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, newpassSent);
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "SSAPWEN ") == output && verbose) {
            char ssapwenReceived[] = "SSAPWEN received\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, ssapwenReceived);
            char hiReceived[] = "HI received\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, hiReceived);
        }
        if(strstr(output, "SSAPWEN ") != output) {
            char badPass[] = "Improper password!\n";
            sfwrite(&stdoutMutex, stdout, badPass);
            if(verbose)
                sfwrite(&stdoutMutex, stdout, output + 12);

            curtime = time(NULL);
            timeStruct = localtime(&curtime);
            strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
            sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, fail, %s\n", timeStr, name, serverIP, serverPort, strtok(output, "\r\n"));
            write(fileno(audit), auditBuffer, strlen(auditBuffer));

            fclose(audit);
            return EXIT_FAILURE;
        }
        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        char loginSuccess[] = "\nLogged in! Message of the Day:\n";
        sfwrite(&stdoutMutex, stdout, loginSuccess);
        char* motdPtr = strstr(output, "MOTD ");
        while(motdPtr == NULL){
            memset(output, 0, sizeof(output));
            read(sockfd, output, MAX_LINE);
            motdPtr = strstr(output, "MOTD ");
        }
        motdPtr += 5;
        strtok(motdPtr, "\r\n");
        sfwrite(&stdoutMutex, stdout, motdPtr);
        sfwrite(&stdoutMutex, stdout, "\n");

        curtime = time(NULL);
        timeStruct = localtime(&curtime);
        strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
        sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, success, %s\n", timeStr, name, serverIP, serverPort, motdPtr);
        write(fileno(audit), auditBuffer, strlen(auditBuffer));
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
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, iamSent);
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "AUTH ") == output && verbose) {
            char authReceived[] = "AUTH received\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, authReceived);
        }
        if(strstr(output, "AUTH ") != output) {
            char noUser[] = "Account with username doesn't exist or already logged in!\n";
            sfwrite(&stdoutMutex, stdout, noUser);
            if(verbose)
                sfwrite(&stdoutMutex, stdout, output);

            curtime = time(NULL);
            timeStruct = localtime(&curtime);
            strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
            sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, fail, %s\n", timeStr, name, serverIP, serverPort, strtok(output, "\r\n"));
            write(fileno(audit), auditBuffer, strlen(auditBuffer));

            fclose(audit);
            return EXIT_FAILURE;
        }

        char enterPass[] = "Enter password: ";
        sfwrite(&stdoutMutex, stdout, enterPass);

        char pass[MAX_LINE - 20];
        fgets(pass, MAX_LINE - 26, stdin);
        memset(output, 0, sizeof(output));
        strcat(output, "PASS ");
        strcat(output, pass);
        strcat(output, " \r\n\r\n\0");
        write(sockfd, output, strlen(output));
        if(verbose){
            char newpassSent[] = "PASS sent\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, newpassSent);
        }

        memset(output, 0, sizeof(output));
        read(sockfd, output, MAX_LINE);
        if(strstr(output, "SSAP ") == output && verbose) {
            char ssapReceived[] = "SSAP received\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, ssapReceived);
            char hiReceived[] = "HI received\n";
            sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
            sfwrite(&stdoutMutex, stdout, hiReceived);
            sfwrite(&stdoutMutex, stdout, output);
        }
        if(strstr(output, "SSAP ") != output) {
            char badPass[] = "Failed to log in!\n";
            sfwrite(&stdoutMutex, stdout, badPass);
            if(verbose)
                sfwrite(&stdoutMutex, stdout, output + 9);

            curtime = time(NULL);
            timeStruct = localtime(&curtime);
            strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
            sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, fail, %s\n", timeStr, name, serverIP, serverPort, strtok(output, "\r\n"));
            write(fileno(audit), auditBuffer, strlen(auditBuffer));

            fclose(audit);
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
        sfwrite(&stdoutMutex, stdout, motdPtr);
        sfwrite(&stdoutMutex, stdout, "\n");

        curtime = time(NULL);
        timeStruct = localtime(&curtime);
        strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
        sprintf(auditBuffer, "%s, %s, LOGIN, %s:%s, success, %s\n", timeStr, name, serverIP, serverPort, motdPtr);
        write(fileno(audit), auditBuffer, strlen(auditBuffer));
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

    if(chatfd < 0){
        sfwrite(&stdoutMutex, stderr, "%sError creating socket.\n", ERROR_TEXT);
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
                    sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                    sfwrite(&stdoutMutex, stdout, emitReceived);
                }
                strtok(output, " \r\n");
                timeInt = atoi(strtok(NULL, " \r\n"));
                hours = timeInt / 3600;
                timeInt = timeInt % 3600;
                minutes = timeInt / 60;
                seconds = timeInt % 60;
                sfwrite(&stdoutMutex, stdout, "%s%i%s%i%s%i%s", "Connected for ", hours, " hour(s), ", minutes, " minute(s), and ", seconds, " second(s)\n");
            }
            else if(strstr(output, "BYE \r\n\r\n") != NULL) {
                if(verbose){
                    char byeReceived[] = "BYE received\n";
                    sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                    sfwrite(&stdoutMutex, stdout, byeReceived);
                }

                curtime = time(NULL);
                timeStruct = localtime(&curtime);
                strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                sprintf(auditBuffer, "%s, %s, LOGOUT, intentional\n", timeStr, name);
                write(fileno(audit), auditBuffer, strlen(auditBuffer));

                fclose(audit);
                close(sockfd);
                kill(childID, SIGKILL);
                return EXIT_SUCCESS;
            }
            else if(strstr(output, "UTSIL ") == output) {
                if(verbose){
                    char utsilReceived[] = "UTSIL received\n";
                    sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                    sfwrite(&stdoutMutex, stdout, utsilReceived);
                }
                strtok(output, " \r\n");
                token = strtok(NULL, " \r\n");
                while(token != NULL) {
                    sfwrite(&stdoutMutex, stdout, "%s\n", token);
                    token = strtok(NULL, " \r\n");
                }
            }
            else if(strstr(output, "UOFF ") == output) {
            }
            else if(strstr(output, "MSG ") == output) {
                if(verbose){
                    char msgReceived[] = "MSG received\n";
                    sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                    sfwrite(&stdoutMutex, stdout, msgReceived);
                }

                char outputCpy[strlen(output)];
                strcpy(outputCpy, output);
                strtok(output, " \r\n");
                token = strtok(NULL, " \r\n");
                toPtr = token;
                token = strtok(NULL, " \r\n");
                fromPtr = token;

                curtime = time(NULL);
                timeStruct = localtime(&curtime);
                strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                if(!strcmp(fromPtr, name)) {
                    sprintf(auditBuffer, "%s, %s, MSG, to, %s\n", timeStr, name, fromPtr);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else {
                    sprintf(auditBuffer, "%s, %s, MSG, from, %s\n", timeStr, toPtr, fromPtr);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }

                if(chatOpen == false) {
                    childID = fork();
                    if(childID == 0){
                        close(sv[0]);

                        char fdStr[12];
                        snprintf(fdStr, 12, "%i", sv[1]);
                        char auditfdStr[12];
                        snprintf(auditfdStr, 12, "%i", fileno(audit));

                        int status = 1;

                        if(!strcmp(fromPtr, name)) {
                            char* xterm[] = {"xterm", "-geometry", "45x35+0", "-T", toPtr, "-e", "./chat", fdStr, auditfdStr, fromPtr, toPtr, NULL};
                            status = execv("/usr/bin/xterm", xterm);
                        }
                        else {
                            char* xterm[] = {"xterm", "-geometry", "45x35+500", "-T", fromPtr, "-e", "./chat", fdStr, auditfdStr, toPtr, fromPtr, NULL};
                            status = execv("/usr/bin/xterm", xterm);
                        }

                        if(status){
                            char* statusLine = "Couldn't create chat window!\n";
                            sfwrite(&stdoutMutex, stderr, statusLine, strlen(statusLine));
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
                sfwrite(&stdoutMutex, stdout, output);
            }
        }

        /*Handle Chat*/
        if(chatReady) {
            memset(output, 0, sizeof(output));
            read(sv[0], output, MAX_LINE);
            if(strstr(output, "MSG ") == output) {
                if(verbose){
                    char msgReceived[] = "MSG received\n";
                    sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                    sfwrite(&stdoutMutex, stdout, msgReceived);
                }
            write(sockfd, output, strlen(output));
            }
        }

        /* Handle STDIN */
        if(stdinReady) {
            if(fgets(input, MAX_LINE - 6, stdin) != NULL) {
                if(*input == '\n')
                    continue;
                else if(!strcmp(input, "/audit\n")) {
                    rewind(audit);
                    while(fgets(auditBuffer, 200, audit) != NULL)
                        sfwrite(&stdoutMutex, stdout, auditBuffer);
                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, /audit, success, client\n", timeStr, name);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else if(!strcmp(input, "/time\n")) {
                    if(verbose){
                        char timeSent[] = "TIME sent\n";
                        sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                        sfwrite(&stdoutMutex, stdout, timeSent);
                    }
                    write(sockfd, "TIME \r\n\r\n\0", 10);

                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, /time, success, client\n", timeStr, name);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else if(!strcmp(input, "/help\n")) {
                    sfwrite(&stdoutMutex, stdout,
                        "CLIENT COMMANDS:\n"
                        "/time          Displays how long the client has been connected to server.\n"
                        "/help          Lists all client commands. The thing you just typed in.\n"
                        "/logout        Disconnects client from server.\n"
                        "/listu         Lists all clients connected to server.\n"
                        "/chat <TO> <MESSAGE>         Starts chat.\n");

                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, /help, success, client\n", timeStr, name);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else if(!strcmp(input, "/logout\n")) {
                    if(verbose){
                        char byeSent[] = "BYE sent\n";
                        sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                        sfwrite(&stdoutMutex, stdout, byeSent);
                    }
                    write(sockfd, "BYE \r\n\r\n\0", 9);

                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, /logout, success, client\n", timeStr, name);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else if(!strcmp(input, "/listu\n")) {
                    if(verbose){
                        char listuSent[] = "LISTU sent\n";
                        sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                        sfwrite(&stdoutMutex, stdout, listuSent);
                    }
                    write(sockfd, "LISTU \r\n\r\n\0", 11);

                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, /listu, success, client\n", timeStr, name);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else if(strstr(input, "/chat") == input) {
                    strtok(input, " \r\n");
                    token = strtok(NULL, " \r\n");
                    if(token != NULL)
                        toPtr = token;
                    else {
                        sfwrite(&stdoutMutex, stdout, "Invalid chat format. Enter /help for a list of commands.\n");
                        fflush(stdout);

                        curtime = time(NULL);
                        timeStruct = localtime(&curtime);
                        strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                        sprintf(auditBuffer, "%s, %s, CMD, /chat, fail, client\n", timeStr, name);
                        write(fileno(audit), auditBuffer, strlen(auditBuffer));
                        continue;
    
                    }
                    token = strtok(NULL, "\r\n");
                    if(token != NULL)
                        msgPtr = token;
                    else {
                        sfwrite(&stdoutMutex, stdout, "Invalid chat format. Enter /help for a list of commands.\n");

                        curtime = time(NULL);
                        timeStruct = localtime(&curtime);
                        strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                        sprintf(auditBuffer, "%s, %s, CMD, /chat, fail, client\n", timeStr, name);
                        write(fileno(audit), auditBuffer, strlen(auditBuffer));
                        fflush(stdout);
                        continue;
                    }
                    if(verbose){
                        char listuSent[] = "MSG sent\n";
                        sfwrite(&stdoutMutex, stdout, VERBOSE_TEXT);
                        sfwrite(&stdoutMutex, stdout, listuSent);
                    }
                    write(sockfd, "MSG ", 4);
                    write(sockfd, toPtr, strlen(toPtr));
                    write(sockfd, " ", 1);
                    write(sockfd, name, strlen(name));
                    write(sockfd, " ", 1);
                    write(sockfd, msgPtr, strlen(msgPtr));
                    write(sockfd, " \r\n\r\n\0", 6);

                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, /chat, success, client\n", timeStr, name);
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
                else {
                    sfwrite(&stdoutMutex, stdout, "%s\n", "Invalid command. Enter /help for a list of commands.");

                    curtime = time(NULL);
                    timeStruct = localtime(&curtime);
                    strftime(timeStr, 50, "%m/%d/%y-%I:%M%P", timeStruct);
                    sprintf(auditBuffer, "%s, %s, CMD, %s, fail, client\n", timeStr, name, strtok(input, "\n"));
                    write(fileno(audit), auditBuffer, strlen(auditBuffer));
                }
            }
        }

        selectRet = 0;
        stdinReady = 0;
        listenReady = 0;
        chatReady = 0;
    }
}

void usage(){
	sfwrite(&stdoutMutex, stdout,
		"\n\x1B[0m ./client [-hcv] [-a FILE] NAME SERVER_IP SERVER_PORT\n"
        "-a FILE        Path to the audit log file.\n"
		"-h 			Displays this help menu and returns EXIT_SUCCESS.\n"
		"-c 			Requests to server to create a new user.\n"
		"-v 			Verbose print all incoming and outgoing protocl verbs & content.\n"
		"NAME 			This is the username to display when chatting.\n"
		"SERVER_IP 		The ip address of the server to connect to.\n"
		"SERVER_PORT 	The port to connect to.\n\n\n");
}