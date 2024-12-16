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
    fprintf(stderr, comando, ": No se encuentra el mandato\n" , comando->argv[0], strerror(errno));
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
            if (fd_in != 0) { //No es el primer comando
                dup2(fd_in, 0); // Redirigir la entrada estándar al pipe de lectura
                close(fd_in);
            }else{
                if (line->redirect_input != NULL){
                    int fd_in = open(line->redirect_input, O_RDONLY);
                    if (fd_in == -1) {
                        perror(line->redirect_input, "Error.", strerror(errno));
                        continue;
                    }
                    dup2(fd_in, STDIN_FILENO); // Redirigir la entrada estándar desde el archivo
                    close(fd_in);
                    }
            }
            if (i < line->ncommands - 1) { //No es el último comando
                dup2(pipefd[1], 1);  // Redirigir la salida estándar al pipe de escritura
                close(pipefd[1]);
            } else {
                if (line->redirect_output != NULL) {
                    fd_out = open(line->redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Redirigir la salida estándar al archivo
                    if (fd_out == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                if (line->redirect_error != NULL) {
                    int fd_err = open(line->redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Redirigir la salida de error al archivo
                    if (fd_err == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_err, STDERR_FILENO);
                    close(fd_err);
                }
            }
            close(pipefd[0]); // Se cierra pipe de lectura
            execute_command(&line->commands[i], fd_in, fd_out);
            
        } else { // Proceso padre
            close(pipefd[1]); // Se cierra pipe de escritura
            fd_in = pipefd[0]; // Se guarda el pipe de lectura

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
        printf("msh> ")
        //printf(BLUE "%s" GREEN "@" BLUE "msh" GREEN ":" BLUE "%s" GREEN "$> ", user, cwd);
    } else {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
}
// Manejador para limpiar procesos terminados en background
void manejador_sigchld(int sig) {
    (void)sig; // Evitar advertencias sobre argumentos no usados
    pid_t pid;
    int estado;

    // Limpiar todos los procesos terminados
    while ((pid = waitpid(-1, &estado, WNOHANG)) > 0) {
        printf("Proceso en background (PID: %d) terminado.\n", pid);
        eliminar_proceso(&lista_bg, pid);
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
        
        // Ejecutar el primer comando en foreground o background
        if (line->background) {
            // Ejecutar en background
            execute_background(line->commands[0].argv[0], line->commands[0].argv);
        } else {
            // Ejecutar en foreground
            ejecutar_en_foreground(line->commands[0].argv[0], line->commands[0].argv);
        }

        execute_piped_commands(line);
    }

    return 0;
}
typedef struct process {
    pid_t pid;          // ID del proceso
    char *command;      // Comando ejecutado
    struct bg_process *next; // Siguiente proceso en la lista
} process;

process *fg_list = NULL; // Inicializa lista de procesos en primer plano

process *bg_list = NULL; // Inicializa lista de procesos en segundo plano

void addFgProcess(process *lista, pid_t pid, const char *comando){
    process *nuevo=malloc(1*sizeof(process));
    if (!nuevo) {
        perror("Error al asignar memoria");
        return;
    }
    nuevo->pid = pid;
    nuevo->command = strdup(comando);
    nuevo->next = lista;
    *lista = *nuevo;
}
void eliminar_process(process **lista, pid_t pid) {
    process *actual = *lista, *anterior = NULL;

    while (actual != NULL) {
        if (actual->pid == pid) {
            if (anterior == NULL) {
                *lista = actual->next;
            } else {
                anterior->next = actual->next;
            }
            free(actual->command);
            free(actual);
            return;
        }
        anterior = actual;
        actual = actual->next;
    }
}

void eliminar_process(process **lista, pid_t pid) {
    process *actual = *lista, *anterior = NULL;

    while (actual != NULL) {
        if (actual->pid == pid) {
            if (anterior == NULL) {
                *lista = actual->next;
            } else {
                anterior->next = actual->next;
            }
            free(actual->command);
            free(actual);
            return;
        }
        anterior = actual;
        actual = actual->next;
    }
}
void execute_background(const char *command, char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Error al crear el proceso");
        return;
    } else if (pid == 0) {
        // Proceso hijo
        if (execvp(args[0], args) == -1) {
            perror("Error al ejecutar el comando");
            exit(EXIT_FAILURE);
        }
    } else {
        // Proceso padre: Agregar el proceso a la lista
        printf("Ejecutando en background: %s (PID: %d)\n", command, pid);
        addBgProcess(&bg_list, pid, command);
    }
}
// Ejecutar un comando en foreground
void ejecutar_en_foreground(const char *command, char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Error al crear el proceso");
        return;
    } else if (pid == 0) {
        // Proceso hijo
        if (execvp(args[0], args) == -1) {
            perror("Error al ejecutar el comando");
            exit(EXIT_FAILURE);
        }
    } else {
        // Proceso padre: Agregar el proceso a la lista de foreground
        agregar_proceso(&fg_list, pid, command);

        // Esperar al proceso en foreground
        int estado;
        waitpid(pid, &estado, WUNTRACED);

        // Analizar el estado del proceso
        if (WIFEXITED(estado)) {
            printf("Proceso en foreground (PID: %d) terminado con código %d\n", pid, WEXITSTATUS(estado));
        } else if (WIFSIGNALED(estado)) {
            printf("Proceso en foreground (PID: %d) terminado por señal %d\n", pid, WTERMSIG(estado));
        } else if (WIFSTOPPED(estado)) {
            printf("Proceso en foreground (PID: %d) detenido por señal %d\n", pid, WSTOPSIG(estado));
        }

        // Limpiar la lista de foreground
        eliminar_proceso(&fg_list, pid);
    }
}
