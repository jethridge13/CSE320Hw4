#define _GNU_SOURCE

#include "csapp.h"

void usage();

int main(int argc, char** argv) {
	int opt;
	bool verbose = false;
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
            	break;
        }
    }
	if(optind < argc && (argc - optind) == 2) {
        portNumber = argv[optind++];
        motd = argv[optind++];
    } else {
        if((argc - optind) <= 0) {
            fprintf(stderr, "\x1B[1;31mMissing PORT_NUMBER and MOTD.\n");
        } else if((argc - optind) == 1) {
            fprintf(stderr, "\x1B[1;31mMissing MOTD.\n");
        } else {
            fprintf(stderr, "\x1B[1;31mToo many arguments provided.\n");
        }
        usage();
        exit(EXIT_FAILURE);
    }

    /* TODO Remove this */
    if(verbose){
    	printf("\x1B[1;34mPORT_NUMBER: %s\n", portNumber);
		printf("MOTD: %s\n", motd);
	}
}

void usage(){
	printf(
		"\x1B[0m./server [-h|-v] PORT_NUMBER MOTD\n"
		"-h 			Displays help menu & returns EXIT_SUCCESS.\n"
		"-v 			Verbose print all incoming and outgoing protocol verbs & content.\n"
		"PORT_NUMBER 	Port number to listn on.\n"
		"MOTD 			Message to display to the client when they connect.\n");
}