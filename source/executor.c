#include "tasks.h"
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

pid_t wait_process(pid_t pid) {
    return waitpid(pid, NULL, 0);
}
