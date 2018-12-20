//
//  tiny_shell_fifo_write.c
//
//  Created by Shao-Wei Liang on 2018-09-18.
//  Copyright © 2018 Shao-Wei Liang. All rights reserved.
//

#define _GNU_SOURCE

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
int my_system(char *line);
void my_system_pipe_read(char **command);

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
    
    // 'exit' received, quit tiny_shell
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
        // Reallocate the tokens size if limit is reached
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

// Main Shell
int main(int argc, char *argv[]) {
    char *currentLine;
    
    while(1) {
        currentLine = getCurrentLine();
        // Compute start time
        double timeStart = getTimes();
        if (currentLine != NULL && strlen(currentLine) > 0) {
            my_system(currentLine);
            
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
int my_system(char *line) {
    my_system_pipe_read(tokenizeCommandLine(line));

    return 0;
}

/*
 Implementation of PIPE using fork()
 - command : the command line received
 - fd: file descriptor
 - processStatus: the status to hold the execution status
 - myFifo: holds the name of the fifo name given from the command line
 */
void my_system_pipe_read(char **command) {
    int fd;
    int processStatus;
    char *myFifo = command[commandLength - 1]; //reads the last argument in command line as fifo
    
    // Create a child process using fork()
    pid_t pid = fork();
    
    // Child Process - does the first command and WRITES
    if (pid == 0) {
        fd = open(myFifo, O_RDONLY);
        if (fd == -1) {
            perror("Open File");
        }
        close(0);
        dup(fd);
        command[commandLength - 1] = NULL;
        if (execvp(command[0], command) == -1){
            // Operation has failed, print error message.
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
