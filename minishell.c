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
typedef struct bg_process {
    pid_t pid;          // ID del proceso
    char *command;      // Comando ejecutado
    struct bg_process *next; // Siguiente proceso en la lista
} bg_process;

bg_process *bg_list = NULL; // Inicializa lista de procesos en segundo plano

void bg_command(int job_id) {
    bg_process *current = bg_list;
    int i = 1;
    while (current) {
        if (i == job_id) { // Encontrar el proceso correspondiente
            kill(current->pid, SIGCONT); // Reanudar el proceso
            printf("[%d] %d continued\n", job_id, current->pid);
            return;
        }
        current = current->next;
        i++;
    }
    printf("bg: no such job\n");
}
//Foreground 
void fg_command(int job_id) {
    bg_process *current = bg_list, *prev = NULL;
    int i = 1;

    while (current) {
        if (i == job_id) { // Encontrar el proceso correspondiente
            // Sacar el proceso de la lista de procesos en segundo plano
            if (prev)
                prev->next = current->next;
            else
                bg_list = current->next;

            // Llevar el proceso al primer plano
            printf("Bringing [%d] %d to foreground\n", job_id, current->pid);
            kill(current->pid, SIGCONT); // Reanudar si estaba pausado
            int status;
            waitpid(current->pid, &status, 0); // Esperar al proceso
            free(current->command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
        i++;
    }
    printf("fg: no such job\n");
}
void execute_background(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Proceso hijo
        setpgid(0, 0); // Crear un nuevo grupo de procesos
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        // Proceso padre
        printf("[%d] %d\n", ++job_counter, pid); // Mostrar el job ID y PID
        bg_process *new_bg = malloc(sizeof(bg_process));
        new_bg->pid = pid;
        new_bg->command = strdup(args[0]); // Guardar el comando
        new_bg->next = bg_list;
        bg_list = new_bg;
    } else {
        perror("fork failed");
    }
}

void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Eliminar el proceso terminado de la lista
        bg_process *current = bg_list, *prev = NULL;
        while (current) {
            if (current->pid == pid) {
                if (prev)
                    prev->next = current->next;
                else
                    bg_list = current->next;
                free(current->command);
                free(current);
                break;
            }
            prev = current;
            current = current->next;
        }
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