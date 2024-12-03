#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "parser.h"
#include <fcntl.h>



# define MAX_INPUT 1024

#define RESET      "\033[0m"
#define RED        "\033[31m"
#define GREEN      "\033[32m"
#define YELLOW     "\033[33m"
#define BLUE       "\033[34m"

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n" YELLOW "SIGINT received.\n" RESET); // TODO: Implement handling for process in foreground
        // TODO
    } else if (sig == SIGQUIT) {
        printf("\n" YELLOW "SIGQUIT received.\n" RESET); // TODO: Implement handling for process in foreground.
        // TODO
    }
}

void cd(char *path) {
    char resolved_path[1024];

    if (path == NULL || strcmp(path, "~") == 0) { 
        path = getenv("HOME"); // Ir al directorio HOME si no se proporciona un argumento o es "~"
    } else if (path[0] == '~') { 
        // Cuando la dirección es "~/*"
        const char *home = getenv("HOME");
        strcpy(resolved_path, home);   
        strcat(resolved_path, path + 1); 
        path = resolved_path;
    }

    if (chdir(path) != 0) { 
        perror("cd"); // Error al cambiar de directorio
    }
}

void execute_command(tcommand *comando, int fd_in, int fd_out) {
    if (fd_in != 0) { // Entra cuando usamos pipes y >
        dup2(fd_in, 0);
        close(fd_in);
    }
    if (fd_out != 1) { // Entra cuando usamos pipes y <
        dup2(fd_out, 1);
        close(fd_out);
    }
    execvp(comando->argv[0], comando->argv); // Buscar en el PATH
    fprintf(stderr, RED "Error: No se pudo ejecutar '%s': %s\n" RESET, comando->argv[0], strerror(errno));
}

void execute_piped_commands(tline *line) {
    int i;
    int pipefd[2];
    int fd_in = 0;
    int fd_out = 1;

    for (i = 0; i < line->ncommands; i++) {
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        // Ningún built-in tiene que hacer fork
        // Como un hijo no puede cambiar el directorio de trabajo de un padre, se maneja el comando `cd` aquí
        if (strcmp(line->commands[i].argv[0], "cd") == 0) {
            cd(line->commands[i].argv[1]);
            close(pipefd[0]);
            close(pipefd[1]);
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
        }

        if (pid == 0) { // Proceso hijo
            if (fd_in != 0) {
                dup2(fd_in, 0); 
                close(fd_in);
            }else{
                if (line->redirect_input != NULL){
                    int fd_in = open(line->redirect_input, O_RDONLY);
                    if (fd_in == -1) {
                        perror("open");
                        continue;
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                    }
            }
            if (i < line->ncommands - 1) {
                dup2(fd_out, 1);
                close(fd_out);
            } else {
                if (line->redirect_output != NULL){
                    fd_out = open(line->redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
            }
            close(pipefd[0]); // Se cierra pipe de lectura
            close(pipefd[1]); // Se cierra pipe de escritura
            execute_command(&line->commands[i], fd_in, fd_out);
            
        } else { // Proceso padre
            close(pipefd[1]); // Se cierra pipe de escritura
            fd_in = pipefd[0]; // Se guarda el pipe de lectura
            close(pipefd[0]); // Se cierra pipe de lectura
        }
    }

    for (i = 0; i < line->ncommands; i++) {
        wait(NULL); 
    }
}

void display_prompt() {
    char cwd[1024];
    char *user = getenv("USER");

    if (user == NULL) {
        user = "Anonymous";
    }
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf(BLUE "%s" GREEN "@" BLUE "msh" GREEN ":" BLUE "%s" GREEN "$> ", user, cwd);
    } else {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
}

int main() {
    char input[MAX_INPUT];
    tline *line;
    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);

    while (1) {
        display_prompt();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // EOF
        }

        if (strcmp(input, "quit\n") == 0 || strcmp(input, "exit\n") == 0 ||
            strcmp(input, "quit()\n") == 0 || strcmp(input, "exit()\n") == 0) {
            printf(GREEN "Exiting shell. Goodbye!\n" RESET );
            return 0;
        }

        line = tokenize(input); // Tokenizar la entrada usando librería del profe
        if (line == NULL) {
            fprintf(stderr, RED "Error: no se pudo procesar el comando.\n" RESET);
            continue;
        }

        execute_piped_commands(line);
    }

    return 0;
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
        static int job_counter = 0;
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