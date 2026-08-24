#include <stdio.h>
#include <sys/wait.h>
#include "jobs.h"
#include "executor.h"

static Job tabela[MAX_JOBS];
static int proximo_id = 1;

bool ha_espaco_job(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!tabela[i].ocupado) {
            return true;
        }
    }
    return false;
}

int registrar_job(pid_t pid, Tarefa *tarefa) {
    if (pid <= 0) {
        return -1;
    }

    for (int i = 0; i < MAX_JOBS; i++) {
        if (tabela[i].ocupado) {
            continue;
        }
        tabela[i].id      = proximo_id++;
        tabela[i].pid     = pid;
        tabela[i].estado  = JOB_RODANDO;
        tabela[i].status  = 0;
        tabela[i].tarefa  = tarefa;
        tabela[i].ocupado = true;
        return tabela[i].id;
    }
    return -1;
}

Job *buscar_job(int id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (tabela[i].ocupado && tabela[i].id == id) {
            return &tabela[i];
        }
    }
    return NULL;
}

void colher_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!tabela[i].ocupado || tabela[i].estado != JOB_RODANDO) {
            continue;
        }

        int status;
        pid_t r = waitpid(tabela[i].pid, &status, WNOHANG);
        if (r == tabela[i].pid) {
            tabela[i].estado = JOB_CONCLUIDO;
            tabela[i].status = status;
        } else if (r < 0) {
            tabela[i].estado = JOB_PERDIDO;
        }
    }
}

static void descrever_estado(const Job *job, char *buf, size_t tam) {
    if (job->estado == JOB_RODANDO) {
        snprintf(buf, tam, "Rodando");
    } else if (job->estado == JOB_PERDIDO) {
        snprintf(buf, tam, "Perdido");
    } else if (WIFEXITED(job->status)) {
        snprintf(buf, tam, "Concluido(%d)", WEXITSTATUS(job->status));
    } else if (WIFSIGNALED(job->status)) {
        snprintf(buf, tam, "Concluido(sinal:%d)", WTERMSIG(job->status));
    } else {
        snprintf(buf, tam, "Concluido");   /* inalcancavel sem WUNTRACED */
    }
}

void listar_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!tabela[i].ocupado) {
            continue;
        }
        char estado[32];
        descrever_estado(&tabela[i], estado, sizeof estado);
        printf("[%d] %d %s %s\n", tabela[i].id, (int)tabela[i].pid,
               estado, tabela[i].tarefa->name);
    }
    fflush(stdout);
}

bool esperar_job(Job *job) {
    if (job->estado == JOB_CONCLUIDO) {
        return true;
    }
    if (job->estado == JOB_PERDIDO) {
        return false;
    }

    int status;
    if (wait_process(job->pid, &status) > 0) {
        job->estado = JOB_CONCLUIDO;
        job->status = status;
        return true;
    }

    job->estado = JOB_PERDIDO;
    return false;
}

void drenar_jobs() {
    bool avisou = false;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!tabela[i].ocupado || tabela[i].estado != JOB_RODANDO) {
            continue;
        }
        if (!avisou) {
            fprintf(stderr, "Aguardando jobs em execucao...\n");
            fflush(stderr);
            avisou = true;
        }
        esperar_job(&tabela[i]);
    }
}