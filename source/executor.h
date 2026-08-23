#ifndef EXECUTOR_H
#define EXECUTOR_H
#include <sys/types.h>
#include "tasks.h"

pid_t create_process(Tarefa *tarefa);
pid_t wait_process(pid_t pid);

#endif