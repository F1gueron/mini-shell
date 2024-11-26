#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 10

// Color definitions
#define RESET      "\033[0m"
#define RED        "\033[31m"
#define GREEN      "\033[32m"
#define YELLOW     "\033[33m"
#define BLUE       "\033[34m"
#define MAGENTA    "\033[35m"
#define CYAN       "\033[36m"
#define WHITE      "\033[37m"


void cd(char *path) {
    if (path == NULL || strcmp(path, "~") == 0) {  // HOME
        path = getenv("HOME");
    }

    if (chdir(path) != 0) {  // cd path
        perror("cd");        // Error
    }
}

void ls(char **args) {
    pid_t pid = fork();  
    if (pid == 0) {     // Correct child -> execute ls
        execvp("ls", args);
        perror("execvp");  // Print error if `ls` fails
        exit(EXIT_FAILURE);
    } else if (pid > 0) {   // Waiting for ls
        wait(NULL);
    } else {
        perror("fork");  // Print error if fork fails
    }
}

void cat(char **args) {
    pid_t pid = fork();  
    if (pid == 0) {     // Correct child -> execute cat
        execvp("cat", args);
        perror("execvp");  // Print error if `cat` fails
        exit(EXIT_FAILURE);
    } else if (pid > 0) {   // Waiting for cat
        wait(NULL);
    } else {
        perror("fork");  // Print error if fork fails
    }
}

void display_prompt() {
    char cwd[1024];  
    char *user = getenv("USER");  

    if (user == NULL) {
        user = "Anonymous";  
    }
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // Display the prompt with directory in it
        printf(BLUE "%s" GREEN "@" BLUE "msh" GREEN ":" BLUE "%s" GREEN "$> ",user, cwd);
    } else {
        perror("getcwd"); //cwd failed
        exit(EXIT_FAILURE);
    }
}

void process_command(char *input) {
    char *args[MAX_ARGS];
    char *token = strtok(input, " \n");
    int i = 0;

    // Split the input into arguments
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;

    if (args[0] == NULL) {
        return; 
    }

    // Command match
    if (strcmp(args[0], "cd") == 0) {
        cd(args[1]);  // Implement this
    } else if (strcmp(args[0], "ls") == 0) {
        ls(args);     // Call /bin/ls          
    } else if (strcmp(args[0], "cat") == 0) {
        cat(args);    // Call /bin/cat
    } else {
        printf("Unrecognized command: %s\n", args[0]);  // Not implemented yet
    }
}

int main() {
    char input[MAX_INPUT];

    while (1) {
        display_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;  // Exit if EOF is received
        }

        // Process the command
        process_command(input);
    }

    return 0;
}