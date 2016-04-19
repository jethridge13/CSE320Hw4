#include "csapp.h"

void usage();

int main(int argc, char** argv){
	int opt;
	bool verbose = false;
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

    /* TODO Remove this */
    if(verbose){
    	printf("\x1B[1;34mNAME: %s\n", name);
    	if(createUser){
    		printf("CREATE NEW USER");
    	}
    	printf("SERVER_IP: %s\n", serverIP);
    	printf("SERVER_PORT: %s\n", serverPort);
    }
}

void usage(){
	printf(
		"\x1B[0m ./client [-hcv] NAME SERVER_IP SERVER_PORT\n"
		"-h 			Displays this help menu and returns EXIT_SUCCESS.\n"
		"-c 			Requests to server to create a new user.\n"
		"-v 			Verbose print all incoming and outgoing protocl verbs & content.\n"
		"NAME 			This is the username to display when chatting.\n"
		"SERVER_IP 		The ip address of the server to connect to.\n"
		"SERVER_PORT 	The port to connect to.\n");
}