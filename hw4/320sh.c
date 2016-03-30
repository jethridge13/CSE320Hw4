#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024

#define PWD_BUFFER_SIZE 100

int 
main (int argc, char ** argv, char **envp) {

  int finished = 0;
  char *prompt = "320sh> ";
  char cmd[MAX_INPUT];

  char* cmdDNE = ": command not found\n";


  while (!finished) {
    char *cursor;
    char last_char;
    int rv;
    int count;
    char* cmds[100]; //Placeholder check w josh


    // Print the prompt
    rv = write(1, prompt, strlen(prompt));
    if (!rv) { 
      finished = 1;
      break;
    }
    
    // read and parse the input
    for(rv = 1, count = 0, 
	  cursor = cmd, last_char = 1;
	  rv 
	  && (++count < (MAX_INPUT-1))
	  && (last_char != '\n');
	cursor++) { 

      rv = read(0, cursor, 1);
      last_char = *cursor;
      if(last_char == 3) {
        write(1, "^c", 2);
      } else {
        // This line prints out the line that was read in. Don't need it. 
	       //write(1, &last_char, 1);
      }
    } 
    *cursor = '\0';

    //PARSING STARTS HERE | STORES TOKENS INTO ARRAY OF POINTERS
    for(int i = 0; i < 100; i++) {
      void* tokenptr = strtok(cmd, " \t\n");
      if(tokenptr == NULL)
        break;
      cmds[i] = tokenptr;
    }

    char* cmdOne = cmds[0];

    if (!rv) { 
      finished = 1;
      break;
    }


    // Execute the command, handling built-in commands separately 
    //Built-in commands
    /* TODO Implement Built-in commands */
    char* exitCompare = "exit";
    char* cdCompare = "cd";
    char* pwdCompare = "pwd";
    char* echoCompare = "echo";
    char* setCompare = "help";
    if(cmdOne == NULL){
      /*  Input is just a newline.
          Do nothing. 
          It works because else statements. */
    }else if(!strcmp(cmdOne, exitCompare)){
      /* exit */
      exit(0);
    } else if(!strcmp(cmdOne, cdCompare)){
      /* cd */

    } else if(!strcmp(cmdOne, pwdCompare)){
      /* pwd */
      char pwdBuffer[PWD_BUFFER_SIZE];
      memset(pwdBuffer, 0, PWD_BUFFER_SIZE);
      getcwd(pwdBuffer, PWD_BUFFER_SIZE);
      write(1, pwdBuffer, PWD_BUFFER_SIZE);
      write(1, "\n", 1);
    } else if(!strcmp(cmdOne, echoCompare)){
      /* echo */

    } else if(!strcmp(cmdOne, setCompare)){
      /* set */

    } else {
      /* TODO Implement control flow to test if given command exists */
      /* Non-built in commands */

      /* Command does not exist */
      write(1, cmd, strnlen(cmd, MAX_INPUT));
      write(1, cmdDNE, strnlen(cmdDNE, MAX_INPUT));
    }

    // Just echo the command line for now
    //write(1, cmd, strnlen(cmd, MAX_INPUT));

  }

  return 0;
}
