#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024

#define PWD_BUFFER_SIZE 100

#ifdef d
  #define debugRun(cmd) fprintf(stderr, "RUNNING: %s\n", cmd)
  #define debugEnd(cmd, return) fprintf(stderr, "ENDED: %s (ret=%d)\n", cmd, return)
#else
  #define debugRun(cmd)
  #define debugEnd(cmd, return)
#endif

int 
main (int argc, char ** argv, char **envp) {

  int finished = 0;
  char *prompt = "320sh> ";
  char cmd[MAX_INPUT];

  char* cmdDNE = ": command not found\n";

  char previousWD[PWD_BUFFER_SIZE];
  memset(previousWD, 0, PWD_BUFFER_SIZE);
  getcwd(previousWD, PWD_BUFFER_SIZE);


  while (!finished) {
    char *cursor;
    char last_char;
    int rv;
    int count;
    char* cmds[100]; //Placeholder check w josh
    /* TODO Delete this once this implementation strategy is changed */
    memset(cmds, 0, 100);

    // Print the working directory
    char cwd[PWD_BUFFER_SIZE];
    memset(cwd, 0, PWD_BUFFER_SIZE);
    getcwd(cwd, PWD_BUFFER_SIZE);
    write(1, "[", 1);
    write(1, cwd, PWD_BUFFER_SIZE);
    write(1, "] ", 2);

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
      void* tokenptr;
      if(i == 0)
        tokenptr = strtok(cmd, " \t\n");
      else
        tokenptr = strtok(NULL, " \t\n");
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
    char* exitCompare = "exit";
    char* cdCompare = "cd";
    char* pwdCompare = "pwd";
    char* echoCompare = "echo";
    char* setCompare = "set";
    char* helpCompare = "help";
    if(cmdOne == NULL){
      /*  Input is just a newline.
          Do nothing. 
          It works because else statements. */
    }else if(!strcmp(cmdOne, exitCompare)){
      /* exit */
      exit(0);
    } else if(!strcmp(cmdOne, cdCompare)){
      /* cd */
      debugRun("CD");

      int returnCode = 0;

      /* Temp will hold the current directory
        which becomes the previous directory upon change */
      char temp[PWD_BUFFER_SIZE];
      memset(temp, 0, PWD_BUFFER_SIZE);
      getcwd(temp, PWD_BUFFER_SIZE);

      if(cmds[1] == NULL){
        returnCode = chdir(getenv("HOME"));
        strcpy(previousWD, temp);
      } else if (!strcmp(cmds[1], "-")){
        returnCode = chdir(previousWD);
        strcpy(previousWD, temp);
      } else {
        returnCode = chdir(cmds[1]);
        strcpy(previousWD, temp);
      }
      if(returnCode){
        write(1, "Something went wrong.\n", 22);
      }
      debugEnd("CD", returnCode);
    } else if(!strcmp(cmdOne, pwdCompare)){
      /* pwd */
      debugRun("cd");

      char pwdBuffer[PWD_BUFFER_SIZE];
      memset(pwdBuffer, 0, PWD_BUFFER_SIZE);
      getcwd(pwdBuffer, PWD_BUFFER_SIZE);
      write(1, pwdBuffer, PWD_BUFFER_SIZE);
      write(1, "\n", 1);

      debugEnd("pwd", 0);
    } else if(!strcmp(cmdOne, echoCompare)){
      /* echo */
      debugRun("echo");

      char* variable = getenv(cmds[1]);
      if(variable != NULL){
        int i = 0;
        while(strcmp(&variable[i],"\0")){
          i++;
        }
        write(1, variable, i);
      } else {
          int i = 1;
          while(cmds[i] != NULL){
            char* string = cmds[i];
            int j = 0;
            while(strcmp(&string[j], "")){
              j++;
            }
            write(1, string, j);
            write(1, " ", 1);
            i++;
          }
        }
      write(1, "\n", 1);

      debugEnd("echo", 0);
    } else if(!strcmp(cmdOne, setCompare)){
      /* set */
      debugRun("set");

      int returnCode = setenv(cmds[1], cmds[3], 1);
      if(returnCode){
        write(1, "Something went wrong.\n", 22);
      }

      debugEnd("set", returnCode);
    } else if (!strcmp(cmdOne, helpCompare)){
      /* help */
      debugRun("help");

      char* cdString = "cd\t\tChange the current working directory.\n";
      char* echoString = "echo\t\tPrint strings and expand environment variables.\n";
      char* exitString = "exit\t\tExits the shell.\n";
      char* helpString = "help\t\tDisplay this help menu.\n";
      char* pwdString = "pwd\t\tPrint the current working directory.\n";
      char* setString = "set\t\tSet the value of environment variables.\n";
      write(1, cdString, strlen(cdString));
      write(1, echoString, strlen(echoString));
      write(1, exitString, strlen(exitString));
      write(1, helpString, strlen(helpString));
      write(1, pwdString, strlen(pwdString));
      write(1, setString, strlen(setString));

      debugEnd("help", 0);
    } else {
      /* Non-built in commands */
      int customPath = 0;
      char* buffer;
      int pathFound = 0;
      int bufferLength = 0;
      if(!strncmp(cmdOne, "/", 1)){
        char bufferSize[strlen(cmdOne)];
        strcpy(bufferSize, cmdOne);
        struct stat pathBuffer;
        if(stat (bufferSize, &pathBuffer) == 0){
          buffer = bufferSize;
          customPath = 1;
          bufferLength = strlen(bufferSize);
        }
      } else if(!strncmp(cmdOne, "./", 2)) {
        char cwd[PWD_BUFFER_SIZE];
        memset(cwd, 0, PWD_BUFFER_SIZE);
        getcwd(cwd, PWD_BUFFER_SIZE);
        char bufferSize[strlen(cmdOne) + PWD_BUFFER_SIZE + 1];
        strcpy(bufferSize, cwd);
        strcat(bufferSize, "/");
        strcat(bufferSize, cmdOne);
        struct stat pathBuffer;
        if(stat (bufferSize, &pathBuffer) == 0){
          customPath = 1;
          buffer = bufferSize;
          bufferLength = strlen(bufferSize);
        }
      } else {
        char* paths[100];
        char* path = getenv("PATH");
        int size = strlen(path);
        char pathHolder[size];
        strcpy(pathHolder, path);

        int pathsCount = 0;
        int i;
        for(i = 0; i < 100; i++) {
          void* tokenptr;
          if(i == 0)
            tokenptr = strtok(pathHolder, ":");
          else
            tokenptr = strtok(NULL, ":");
          if(tokenptr == NULL)
            break;
          paths[i] = tokenptr;
          pathsCount++;
        }
        char* loopBuffer;
        for(i = 0; i < pathsCount; i++){
          // buffer contains the full filepath to check
          loopBuffer = malloc(strlen(paths[i]) + strlen(cmdOne) + 1);
          strcpy(loopBuffer, paths[i]);
          strcat(loopBuffer, "/");
          strcat(loopBuffer, cmdOne);
          struct stat pathBuffer;
          if(stat (loopBuffer, &pathBuffer) == 0){
            pathFound = 1;
            char bufferSize[sizeof(loopBuffer)];
            strcpy(bufferSize, loopBuffer);
            buffer = bufferSize;
            bufferLength = strlen(bufferSize);
            free(loopBuffer);
            break;
          }
          free(loopBuffer);
        }
      }
      if(pathFound || customPath){
        int childStatus;
        char bufferSize[bufferLength];
        strcpy(&bufferSize[0], buffer);

        debugRun(bufferSize);

        pid_t childID = fork();
        if(childID == 0){
          /*
          write(1, bufferSize, strlen(bufferSize));
          write(1, "\n", 1);
          */
          int status = execv(bufferSize, cmds);
          if(status){
            /* error handling that I used for debugging 
            int errnum;
            errnum = errno;
            fprintf(stderr, "ERROR: %s\n", strerror(errnum));
            */
            char* statusLine = "Something with the execution of: '";
            char* statusLine2 = "' failed.\n";
            write(1, statusLine, strlen(statusLine));
            write(1, bufferSize, strlen(bufferSize));
            write(1, statusLine2, strlen(statusLine2));
            /* Something went wrong with execution. Kill child. */
            kill(getpid(), SIGKILL);
          }
        } else {
          //free(buffer);
          waitpid(childID, &childStatus, 0);

          debugEnd(bufferSize, childStatus);
        }
      } else {
        /* Command does not exist */
        write(1, cmd, strnlen(cmd, MAX_INPUT));
        write(1, cmdDNE, strnlen(cmdDNE, MAX_INPUT));
      }
    }

    // Just echo the command line for now
    //write(1, cmd, strnlen(cmd, MAX_INPUT));

  }

  return 0;
}
