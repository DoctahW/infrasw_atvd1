#ifndef EXECUTOR_H
#define EXECUTOR_H
#include <sys/types.h>
#include "tasks.h"

pid_t create_process(Tarefa *tarefa);
pid_t wait_process(pid_t pid, int *out_status);
void reportar_status(const Tarefa *t, int status);
void executar_sequencial(Tarefa *tarefas[], int total);
void executar_paralelo(Tarefa *tarefas[], int total);

#endif