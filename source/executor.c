#include "tasks.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

static int redirecionar(char *arquivo, int flags, int alvo) {
    int aberto = open(arquivo, flags, 0644);
    if (aberto < 0)
        return -1;
    if (dup2(aberto, alvo) < 0) {
        close(aberto);
        return -1;
    }
    if (aberto != alvo)
        close(aberto);
    return 0;
}

pid_t create_process(Tarefa *tarefa) {
    pid_t pid;
    char *args[MAX_ARGS + 2];
    args[0] = tarefa->programa;
    for (int i=1; i < tarefa->argc+1; i++) {
        args[i] = tarefa->args[i-1];
    }
    args[tarefa->argc+1] = NULL;
    
    pid = fork();
    if (pid < 0) {
        perror("fork");
    }
    
    if (pid == 0) {
        if (tarefa->entrada[0] != '\0') {
            if (redirecionar(tarefa->entrada, O_RDONLY, STDIN_FILENO) < 0) {
                fprintf(stderr, "processflow: %s: %s\n", tarefa->entrada, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        if (tarefa->saida[0] != '\0') {
            int modo = tarefa->anexo ? O_APPEND : O_TRUNC;
            int flags = O_WRONLY | O_CREAT | modo;
            
            if (redirecionar(tarefa->saida, flags, STDOUT_FILENO) < 0) {
                fprintf(stderr, "processflow: %s: %s\n", tarefa->saida, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        execv(tarefa->programa, args);
        fprintf(stderr, "processflow: %s: %s\n", tarefa->programa, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return pid;
}

pid_t wait_process(pid_t pid, int *out_status) {
    int status;
    pid_t r;

    if (pid <= 0) {
        errno = ECHILD;
        return -1;
    }

    do {
        r = waitpid(pid, &status, 0);
    } while (r < 0 && errno == EINTR);

    if (r > 0 && out_status != NULL)
        *out_status = status;

    return r;
}

void reportar_status(const Tarefa *t, int status) {
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0)
            fprintf(stderr, "%s: terminou com código %d\n", t->name, code);
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s: terminado pelo sinal %d\n", t->name, WTERMSIG(status));
    }
}

void executar_sequencial(Tarefa *tarefas[], int total) {
    for (int i=0; i < total; i++) {
        pid_t pid = create_process(tarefas[i]);
        int status;
        if (wait_process(pid, &status) > 0) {
            reportar_status(tarefas[i], status);
            continue;
        }
    }
}

void executar_paralelo(Tarefa *tarefas[], int total) {
    pid_t pids[total];
    for (int i=0; i < total; i++) {
        pid_t pid = create_process(tarefas[i]);
        pids[i] = pid;
    }
    for (int i=0; i < total; i++) {
        int status;
        if (wait_process(pids[i], &status) > 0) {
            reportar_status(tarefas[i], status);
            continue;
        }
    }
}

