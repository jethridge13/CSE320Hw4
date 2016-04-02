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
  //char pipingCmd[MAX_INPUT];
  memset(cmd, 0, MAX_INPUT);

  struct cmdHist { //COMMAND HISTORY LIST
    char cmdPrint[MAX_INPUT];
    struct cmdHist *next;
    struct cmdHist *prev;
  };
  typedef struct cmdHist cmdHist;

  cmdHist* cmdHistPtr = calloc(50, sizeof(cmdHist)*50);
  cmdHist* cmdHistHead = cmdHistPtr;
  cmdHist* cmdHistTail = cmdHistPtr;

  int cmdHistSize = 0;
  int cmdIdx = 0;

  char* cmdDNE = ": command not found\n";

  char previousWD[PWD_BUFFER_SIZE];
  memset(previousWD, 0, PWD_BUFFER_SIZE);
  getcwd(previousWD, PWD_BUFFER_SIZE);

  setenv("?", "0", 1);

  while (!finished) {
    char *cursor;
    char last_char;
    int rv;
    int count;
    char* cmds[100]; //Placeholder check w josh
    char* pipingCmds[100];
    int pipingCount = 0;
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

    cmdHist* currHist = NULL; //FOR UP AND DOWN ARROWS
    write(1, "\e[s", 3); //SAVE CURSOR POSITION
    
    // read and parse the input
    for(rv = 1, count = 0, 
	  cursor = cmd, last_char = 1;
	  rv 
	  && (++count < (MAX_INPUT-1))
	  && (last_char != '\n');
	cursor++) { 

      rv = read(0, cursor, 1);

      /* Debug stuff
      int test = strcmp(cursor, "\b");
      char testString[4];
      sprintf(testString, "%d", test);
      write(1, testString, strlen(testString));
      */
      if(strcmp(cursor, "\e") == 0) {
        rv = read(0, cursor, 1);
        if(strcmp(cursor, "[") == 0) {
          rv = read(0, cursor, 1);
          if(strcmp(cursor, "A") == 0) { //UP ARROW
            if(currHist == NULL && cmdHistTail != NULL) {
              *cursor = 0;
              cursor--;
              write(1, "\e[u", 3);
              write(1, "\e[K", 3);
              currHist = cmdHistTail;
              strcpy(cmd, (*currHist).cmdPrint);
              write(1, cmd, strcspn(cmd, "\n"));
              continue;
            }
            else if(currHist != NULL && (*currHist).prev != NULL) {
              *cursor = 0;
              cursor--;
              write(1, "\e[u", 3);
              write(1, "\e[K", 3);
              currHist = (*currHist).prev;
              strcpy(cmd, (*currHist).cmdPrint);
              write(1, cmd, strcspn(cmd, "\n"));
              continue;
            }
            else {
              *cursor = 0;
              cursor--;
              continue;
            }
          }

          else if(strcmp(cursor, "B") == 0) { //DOWN ARROW
            if((*currHist).next != NULL) {
              *cursor = 0;
              cursor--;
              write(1, "\e[u", 3);
              write(1, "\e[K", 3);
              currHist = (*currHist).next;
              strcpy(cmd, (*currHist).cmdPrint);
              write(1, cmd, strcspn(cmd, "\n"));
              continue;
            }
            else {
              *cursor = 0;
              cursor--;
              continue;
            }
          }
          
          else if(strcmp(cursor, "C") == 0) { //LEFT ARROW

          }

          else if(strcmp(cursor, "D") == 0) { //RIGHT ARROW
          }
        }
      }

      if(strcmp(cursor, "\b") == 119){
        /* Backspace detected */
        int i = 0;
        int endCase = strlen(cwd) + 3 + strlen(prompt) + strlen(cmd) + 1;
        char clear[endCase];
        for(;i < endCase; i++){
          clear[i] = ' ';
        }

        char *cr = "\r";
        char *cursorMove = "\033[1D";
        write(1, cr, strlen(cr));
        write(1, clear, strlen(clear));
        write(1, cr, strlen(cr));
        write(1, "[", 1);
        write(1, cwd, PWD_BUFFER_SIZE);
        write(1, "] ", 2);
        write(1, prompt, strlen(prompt));
        int cmdLen = strlen(cmd);
        cmdLen += -1;
        cmd[cmdLen] = '\0';
        cmd[cmdLen - 1] = '\0';
        char cmdPrint[MAX_INPUT];
        strncpy(cmdPrint, cmd, cmdLen);
        write(1, cmdPrint, cmdLen);
        write(1, cursorMove, strlen(cursorMove));
        *cursor = 0;
        cursor--;
        *cursor = 0;
        cursor--;
      }
      else {
        write(1, cursor, 1);
      }
      
      last_char = *cursor;
      if(last_char == 3) {
        write(1, "^c", 2);
      } else {
        // This line prints out the line that was read in. Don't need it. 
	       //write(1, &last_char, 1);
      }
    } 
    *cursor = '\0';

    /*COMMAND LIST HISTORY*/
    if(strcmp(cmd, "\n")) {
      if(cmdHistSize == 0) {
        strcpy((*cmdHistHead).cmdPrint, cmd); //INITIALIZE COMMAND HISTORY LIST
        cmdHistTail = cmdHistHead;
        cmdHistSize++;
        cmdIdx++;
      }
      else {
        if(cmdHistSize < 50) {
          strcpy(cmdHistPtr[cmdIdx].cmdPrint, cmd);
          (*cmdHistTail).next = &(cmdHistPtr[cmdIdx]);
          cmdHistPtr[cmdIdx].prev = cmdHistTail;
          cmdHistTail = &cmdHistPtr[cmdIdx];
          cmdHistSize++;
          cmdIdx++;
        }
        else {
          if(cmdIdx >= 50)
            cmdIdx = 0;
          cmdHistHead = (*cmdHistHead).next;
          (*cmdHistHead).prev = NULL; 
          strcpy(cmdHistPtr[cmdIdx].cmdPrint, cmd);
          (*cmdHistTail).next = &(cmdHistPtr[cmdIdx]);
          cmdHistPtr[cmdIdx].prev = cmdHistTail;
          cmdHistTail = &cmdHistPtr[cmdIdx];
          (*cmdHistTail).next = NULL;
          cmdIdx++;
        }
      }
    }

    //PARSING STARTS HERE | STORES TOKENS INTO ARRAY OF POINTERS
    for(int i = 0; i < 100; i++){
      void* tokenptr;
      if(i == 0)
        tokenptr = strtok(cmd, "<>|");
      else
        tokenptr = strtok(NULL, "<>|");
      if(tokenptr == NULL)
        break;
      pipingCmds[i] = tokenptr;
      pipingCount++;
    }
    strcpy(cmd, pipingCmds[0]);
    for(int i = 0; i < pipingCount; i++){
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
          char* status1 = "cd: ";
          char* status2 = ": No such file or directory\n";
          write(1, status1, strlen(status1));
          write(1, cmds[1], strlen(cmds[1]));
          write(1, status2, strlen(status2));
        }

        char returnCodeString[4];
        sprintf(returnCodeString, "%d", returnCode);
        setenv("?", returnCodeString, 1);
        debugEnd("CD", returnCode);
      } else if(!strcmp(cmdOne, pwdCompare)){
        /* pwd */
        debugRun("cd");

        char pwdBuffer[PWD_BUFFER_SIZE];
        memset(pwdBuffer, 0, PWD_BUFFER_SIZE);
        getcwd(pwdBuffer, PWD_BUFFER_SIZE);
        write(1, pwdBuffer, PWD_BUFFER_SIZE);
        write(1, "\n", 1);

        char returnCodeString[4];
        sprintf(returnCodeString, "%d", 0);
        setenv("?", returnCodeString, 1);
        debugEnd("pwd", 0);
      } else if(!strcmp(cmdOne, echoCompare)){
        /* echo */
        debugRun("echo");

        char* variable = NULL;
        if(cmds[1][0] == '$'){
          char* variableName = cmds[1];
          variable = getenv(&variableName[1]);
        }
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

        char returnCodeString[4];
        sprintf(returnCodeString, "%d", 0);
        setenv("?", returnCodeString, 1);
        debugEnd("echo", 0);
      } else if(!strcmp(cmdOne, setCompare)){
        /* set */
        debugRun("set");

        int returnCode = setenv(cmds[1], cmds[3], 1);
        if(returnCode){
          write(1, "Something went wrong.\n", 22);
        }

        char returnCodeString[4];
        sprintf(returnCodeString, "%d", returnCode);
        setenv("?", returnCodeString, 1);
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

        char returnCodeString[4];
        sprintf(returnCodeString, "%d", 0);
        setenv("?", returnCodeString, 1);
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
          int loopBufferSize = strlen(path) + strlen(cmdOne);
          char* loopBuffer = malloc(loopBufferSize);
          for(i = 0; i < pathsCount; i++){
            // buffer contains the full filepath to check
            memset(loopBuffer, 0, loopBufferSize);
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
              break;
            } 
          }
          free(loopBuffer);
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

            char returnCodeString[4];
            sprintf(returnCodeString, "%d", childStatus);
            setenv("?", returnCodeString, 1);
            debugEnd(bufferSize, childStatus);
          }
        } else {
          /* Command does not exist */
          write(1, cmd, strnlen(cmd, MAX_INPUT));
          write(1, cmdDNE, strnlen(cmdDNE, MAX_INPUT));
        }
      }
      if(i != pipingCount-1){
        strcpy(cmd, pipingCmds[i+1]);
      } 
    }
  }
  free(cmdHistPtr);
  return 0;
}