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

#define MAX_BG_PROCESS 1024 //Número máximo de procesos que vamos a poder ejecutar en segundo plano a la vez

# define MAX_INPUT 1024

typedef struct {
    pid_t pid;           // ID del proceso
    char *command;   // Comando ejecutado (asumiendo un límite de 256 caracteres)
    int status;     // Estado del proceso (0 muerto 1 ejecutándose)
} process;

process bg_pids[MAX_BG_PROCESS]; //Array de pids
int contador = 0; //inicializamos variable contador que contará los procesos corriendo en background


void addBgProcessArray(pid_t pid, const char *comand) {
    if (contador < MAX_BG_PROCESS) {
        // Inicializa el proceso en el índice actual
        bg_pids[contador].pid = pid;

        // Reservar memoria para el comando
        bg_pids[contador].command = malloc(strlen(comand) + 1);
        if (bg_pids[contador].command == NULL) {
            fprintf(stderr, "Error: No se pudo asignar memoria para el comando.\n");
            return;
        }
        strcpy(bg_pids[contador].command, comand);

        // Estado inicial del proceso en ejecución
        bg_pids[contador].status = 1;

        // Incrementar contador después de agregar
        contador++;
    } else {
        fprintf(stderr, "Error: no se pueden agregar más procesos en background (límite alcanzado).\n");
    }
}


void removeBgProcessStruct(pid_t pid) {
    int found = 0; // Bandera para determinar si se encontró el proceso

    for (int i = 0; i < contador; i++) {
        if (bg_pids[i].pid == pid) {
            found = 1; // Marcar como encontrado
            for (int j = i; j < contador - 1; j++) {
                // Mover todos los elementos una posición hacia atrás
                bg_pids[j] = bg_pids[j + 1];
            }
            contador--; // Reducir el contador después de eliminar
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Error: Proceso con PID %d no encontrado.\n", pid);
    }
}

void execute_background(const char *command, char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Error al crear el proceso");
        return;
    } else if (pid == 0) {
        // Proceso hijo
        signal(SIGINT, SIG_IGN);  // Ignorar SIGINT (Ctrl+C)
        signal(SIGQUIT, SIG_IGN); // Ignorar SIGQUIT (Ctrl+\)
        
        if (execvp(args[0], args) == -1) {
            perror("Error al ejecutar el comando");
            exit(EXIT_FAILURE);
        }
    } else {
        // Proceso padre
        printf("Ejecutando en background: %s (PID: %d)\n", command, pid);
        addBgProcessArray(pid, command);
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
        removeBgProcessStruct(pid); // Cambiar a removeBgProcessStruct
    }
}

void jobs() {
    printf("Procesos en background:\n");
    for (int i = 0; i < contador; i++) {
        // Verificar si el proceso sigue activo
        if (kill(bg_pids[i].pid, 0) == 0) {
            printf("[%d] Running: %d, Command=%s - En ejecución\n", i + 1, bg_pids[i].pid, bg_pids[i].command);
        }
    }

    if (contador == 0) {
        printf("No hay procesos en background.\n");
    }
}
void devolverAFg(int jobid){
    if(jobid==-1){
        if(contador>0){
            waitpid(bg_pids[contador-1].pid, NULL, 0);
            removeBgProcessStruct(bg_pids[contador-1].pid);
        }
        else{
            printf("No hay procesos en background.\n");
        }
    }
    else{
        if(contador>=jobid){
            waitpid(bg_pids[jobid-1].pid, NULL, 0);
            removeBgProcessStruct(bg_pids[jobid-1].pid);
        }
        else{
            printf("No esta el proceso en background\n");
        }
    }
}

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n"); 
    } else if (sig == SIGQUIT) {
        printf("\n"); 
    }
}

void cd(char *path) {
    char resolved_path[1024];
    if (path == NULL) { 
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
    if (execvp(comando->argv[0], comando->argv) == -1) {
        fprintf(stderr,"%s: No se encuentra el mandato\n" , comando->argv[0]);
    }
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
            return;
        }
        if (strcmp(line->commands[i].argv[0], "jobs") == 0) {
            jobs();
            close(pipefd[0]);
            close(pipefd[1]);
            return;
        }
        if (strcmp(line->commands[i].argv[0], "fg") == 0) {
            int job_id = -1;
            if (line->commands[0].argc > 1) {
                job_id = atoi(line->commands[0].argv[1]);
            }
            devolverAFg(job_id);            
            close(pipefd[0]);
            close(pipefd[1]);
            return;
        }

        if (line->commands[i].filename == NULL) {
            fprintf(stderr,"%s: No se encuentra el mandato\n", line->commands[i].argv[0]);
            break;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
        }

        if (pid == 0) { // Proceso hijo
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            if (fd_in != 0) { // No es el primer comando
                dup2(fd_in, 0); // Redirigir la entrada estándar al pipe de lectura
                close(fd_in);
            }else{
                if (line->redirect_input != NULL){
                    int fd_in = open(line->redirect_input, O_RDONLY);
                    if (fd_in == -1) {
                        fprintf(stderr, "%s Error.%s\n", line->redirect_input, strerror(errno));
                        continue;
                    }
                    dup2(fd_in, STDIN_FILENO); // Redirigir la entrada estándar al archivo
                    close(fd_in);
                    }
            }
            if (i < line->ncommands - 1) { // No es el último comando
                dup2(pipefd[1], 1);
                close(pipefd[1]);
            } else {
                if (line->redirect_output != NULL) {
                    fd_out = open(line->redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out == -1) {
                        fprintf(stderr, "%s Error.%s\n", line->redirect_output, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_out, STDOUT_FILENO); // Redirigir la salida estándar al archivo
                    close(fd_out);
                }
                if (line->redirect_error != NULL) { 
                    int fd_err = open(line->redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_err == -1) { 
                        fprintf(stderr, "%s Error.%s\n", line->redirect_error, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_err, STDERR_FILENO); // Redirigir la salida de error estándar al archivo
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
        printf("msh> ");
        //printf(BLUE "%s" GREEN "@" BLUE "msh" GREEN ":" BLUE "%s" GREEN "$> ", user, cwd);
    } else {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
}



int main() {
    char input[MAX_INPUT];
    tline *line;
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    while (1) {
        display_prompt();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // EOF
        }

        if (strcmp(input, "quit\n") == 0 || strcmp(input, "exit\n") == 0 ||
            strcmp(input, "quit()\n") == 0 || strcmp(input, "exit()\n") == 0) {
            printf("Exiting shell. Goodbye!\n");
            return 0;
        }

        line = tokenize(input); // Tokenizar la entrada usando librería del profe
        if (line->ncommands == 0) {
            fprintf(stderr,"Error: no se pudo procesar el comando.\n");
            continue;
        }
        
        // Ejecutar el primer comando en foreground o background
        if (line->background) {
            printf("Ejecutando en background: %s\n", line->commands[0].argv[0]);
            // Ejecutar en background
            execute_background(line->commands[0].argv[0], line->commands[0].argv);
        } else {
            execute_piped_commands(line);
        }
        
    }

    return 0;
}