#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024

int 
main (int argc, char ** argv, char **envp) {

  int finished = 0;
  char *prompt = "320sh> ";
  char cmd[MAX_INPUT];


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
	write(1, &last_char, 1);
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

    //Placeholder so -Wall -Werror stops yelling at me
    char* cmdOne = cmds[0];
    //printf("%p", cmdOne);


    if (!rv) { 
      finished = 1;
      break;
    }


    // Execute the command, handling built-in commands separately 
    //Built-in commands
    char* exitCompare = "exit";
    int exitStatus;
    exitStatus = strcmp(cmdOne, exitCompare);
    if(exitStatus == 0){
      printf("SUCCESS!");
    }
    // Just echo the command line for now
    //write(1, cmd, strnlen(cmd, MAX_INPUT));

  }

  return 0;
}
