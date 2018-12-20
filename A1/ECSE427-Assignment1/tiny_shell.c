//
//  tiny_shell.c
//
//  Created by Shao-Wei, Liang (260606721) on 2018-09-18.
//  Copyright © 2018 Shao-Wei Liang. All rights reserved.
//

#define _GNU_SOURCE
#define STACK_SIZE (128 * 128)

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

double getTimes(void);
char** tokenizeCommandLine(char *input);
int my_system(char **line);
void my_system_f(char **command);
void my_system_v(char **command);
void my_system_c(char **command);
void my_system_pipe_write(char **command);
static int childFunc(void* arg);

int commandLength; // a variable used to store the length of the command (to be used to extract the location of fifo within the command for piping)

/** Get the current line from input (whether its from file or keyboard)
 *  Returns the line received
 */
char *getCurrentLine(void) {
    char *line = NULL;
    size_t bufferSize = 0;
    
    // Before reaching end of file
    if (stdin != NULL) {
        getline(&line, &bufferSize, stdin);
        
        // replace the end character \n with \0
        char *pos;
        if ((pos=strchr(line, '\n')) != NULL)
            *pos = '\0';
    }
    
    // 'exit' command received, quit tiny_shell
    if(strcasecmp(line, "exit") == 0) {
        exit(0);
    }
    return line;
}

/** Parse the current line into an array of "arguments"
    so that we can identify pass the appropriate command later
    Returns an array with arguments/flags receiveed.
    - token : single argument after tokenizing the input
    - tokens : array that contains all the token of a given input
 */
char** tokenizeCommandLine(char *input) {
    int position = 0;
    int bufferSize = 8;
    char *token;
    char **tokens = malloc(bufferSize * sizeof(char*));
    
    token = strtok(input, " ");
    while (token != NULL) {
        tokens[position] = token;
        position++;
        
        //  Reallocate the array size if the size is reached
        if (position >= bufferSize) {
            bufferSize *= 2;
            tokens = realloc(tokens, bufferSize * sizeof(char*));
        }
        token = strtok(NULL, " ");
    }
    tokens[position] = NULL;
    commandLength = position;
    return tokens;
}

/** Main Shell
 */
int main(int argc, char *argv[]) {
    char *currentLine;
    
    while(1) {
        // Get the line by called the `getCurrentLine()` method save the result into pointer to be passed
        currentLine = getCurrentLine();
        
        // Compute start time
        double timeStart = getTimes();
        if (currentLine != NULL && strlen(currentLine) > 0) {
            printf(">>>>>>>>>>>>>>>>>>>> Input: %s <<<<<<<<<<<<<<<<<<<<\n", currentLine);
            // Tokenize the currentLine into an array of arguments and pass the results to `my_system(**char)`
            my_system(tokenizeCommandLine(currentLine));
            // Compute end time and determine the time elapsed
            double timeEnd = getTimes();
            printf("Time elapased: %f ms\n", timeEnd-timeStart);
        } else {
            // End of file reached - exit the program here
            exit(0);
        }
    }
    return 0;
}

/*
 Implementation of `my_system`
 - line : the command line received
 */
int my_system(char **line) {
    #ifdef FORK
        my_system_f(line);
    #elif VFORK
        my_system_v(line);
    #elif CLONE
        my_system_c(line);
    #elif PIPE
        my_system_pipe_write(line);
    #else
        my_system_f(line);
#endif
    return 0;
}

/** my_system implementation using `fork()`
 */
void my_system_f(char **command) {
    int processStatus;
    
    // Create a child process using fork()
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child Process
        
        if (execvp(command[0], command) == -1){
            // Operation has failed, print error message.
            puts(strerror(errno));
            _exit(errno);
        }
        _exit(0);
    } else if (pid > 0) {
        // Parent process
        waitpid(pid, &processStatus, 0);
    } else {
        // Child process creation is unsuccessful
        perror("Fork process unsuccessful");
    }
    free(command);
}

/** my_system implementation using `vfork()`
 */
void my_system_v(char **command) {
    int processStatus;
    
    // Create a child process using vfork()
    pid_t pid = vfork();
    
    if (pid == 0) {
        // Child Process
        if (execvp(command[0], command) == -1){
            // Operation has failed, print error message.
            puts(strerror(errno));
            _exit(errno);
        }
        _exit(0);
    } else if (pid > 0) {
        // Parent process
        waitpid(pid, &processStatus, 0);
    } else {
        // Child process creation is unsuccessful
        perror("vfork process unsuccessful");
    }
    free(command);
}

/** my_system implementation using `clone()`
 */
void my_system_c(char **command) {
    
    int processStatus;
    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        perror("Stack");
        exit(1);
    }
    char *stackTop = stack + STACK_SIZE; //points to top

    pid_t pid = clone(childFunc, stackTop, CLONE_VFORK | CLONE_FS | SIGCHLD, command);

    if (pid == -1) {
        perror("Clone");
        free(stack);
        exit(1);
    }
    waitpid(pid, &processStatus, 0);
    free(command);
    free(stack);
}

/** The child function that is called when using `clone`
 */
static int childFunc(void* arg) {
    char** arguments = (char **)arg;
    if (strcasecmp(arguments[0], "cd") == 0) {
        int cd = chdir(arguments[1]); // If the first argument of line contains `cd`, we change directory by calling `chdir`
        if (cd == -1){
            perror("Unknown Directory");
        }
    } else {
        if (execvp(arguments[0], arguments) == -1){ // Else, execute the command
            puts(strerror(errno));
            _exit(errno);
        }
        _exit(0);
    }
    free(arguments);
    return 0;
}

/** my_system implementation for PIPE using `fork`
    - fd : file descriptor number
    - myFifo : the fifo file created using `mkfifo`
 */
void my_system_pipe_write(char **command) {
    int fd;
    int processStatus;
    char* myFifo = command[commandLength - 1]; // fifo name is captured as the last argument in the commandLine.
    
    // Create a child process using fork()
    pid_t pid = fork();
    
    if (pid == 0) {        
        fd = open(myFifo, O_WRONLY); // open the fifo which then `open` returns 1 (stdout)
        if (fd == -1) {
            perror("Open File (write)");
        }
        close(1); // close the output of the process to make sure its available
        dup(fd); // fd now points to output as its the first available file descriptor
        command[commandLength - 1] = NULL; // once we have fifo name, we can remove it from the argument and pass the remaining line into exec()
        if (execvp(command[0], command) == -1){
            puts(strerror(errno));
            _exit(errno);
        }
    } else if (pid > 0){
        // Parent process 
        waitpid(pid, &processStatus, 0);
    } else {
        // Child process creation is unsuccessful
        perror("Fork process unsuccessful");
    }
    free(command);
}

// Compute current time - to be used for measuring time spent
double getTimes(void) {
    struct timespec ts; // this is defined in `time.h`
    
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
        perror("Clock-gettime"); //Tag the error
    }
    
    double x = ts.tv_sec * 1000;
    x += ts.tv_nsec/1000000;
    return x;
}
