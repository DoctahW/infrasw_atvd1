#include "tasks.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

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
        execv(tarefa->programa, args);
        perror("execv");
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


