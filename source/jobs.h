#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stdbool.h>
#include "tasks.h"

#define MAX_JOBS 32

typedef enum {
    JOB_RODANDO,
    JOB_CONCLUIDO,
    JOB_PERDIDO
} EstadoJob;

typedef struct {
    int        id;
    pid_t      pid;
    EstadoJob  estado;
    int        status;
    Tarefa    *tarefa;
    bool       ocupado;
} Job;

bool  ha_espaco_job(void);
int   registrar_job(pid_t pid, Tarefa *tarefa);
Job  *buscar_job(int id);
void  colher_jobs(void);
void  listar_jobs(void);
bool  esperar_job(Job *job);
void  drenar_jobs(void);

#endif