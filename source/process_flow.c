#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "parser.h"
#include "tasks.h"
#include "executor.h"
#include "jobs.h"

#define MAX_PALAVRAS 64

bool running = true;

static void descartar_resto(FILE *arquivo) {
    int c;
    do {
        c = fgetc(arquivo);
    } while (c != '\n' && c != EOF);
}

static bool coube(char *buffer) {
    char *quebra = strchr(buffer, '\n');
    if (quebra == NULL) {
        return false;
    }
    return true;
}

static char *mensagem_status(StatusTarefa status) {
    switch (status) {
        case TASK_OK:               return "sem erro";
        case TASK_E_TABELA_CHEIA:   return "a tabela de tarefas esta cheia";
        case TASK_E_NOME_DUPLICADO: return "ja existe uma tarefa com esse nome";
        case TASK_E_CAMPO_LONGO:    return "nome, programa ou argumento longo demais";
        case TASK_E_ARGS_DEMAIS:    return "argumentos demais para uma tarefa";
        case TASK_E_NAO_ENCONTRADA: return "Tarefa nao encontrada";
    }
    return "motivo desconhecido";
}

static void tratar_task(char **palavras, int n) {
    if (n < 3) {
        fprintf(stderr, "Erro: uso: task <nome> <programa> [argumentos...]\n");
        return;
    }

    StatusTarefa status = cadastrar_tarefa(palavras[1], palavras[2],
                                           &palavras[3], n - 3);
    if (status != TASK_OK) {
        fprintf(stderr, "Erro: tarefa '%s' nao cadastrada: %s.\n",
                palavras[1], mensagem_status(status));
    }
}

static void tratar_input(char **palavras, int n) {
    if (n != 3) {
        fprintf(stderr, "Erro: uso: input <tarefa> <arquivo>\n");
        return;
    }
    
    StatusTarefa status = definir_entrada(palavras[1], palavras[2]);
    if (status != TASK_OK) {
        fprintf(stderr, "Erro: entrada nao definida: %s.\n", mensagem_status(status));
    }
}

static void tratar_output(char **palavras, int n) {
    if (n != 3) {
        fprintf(stderr, "Erro: uso: output <tarefa> <arquivo>\n");
        return;
    }
    
    StatusTarefa status = definir_saida(palavras[1], palavras[2], 0);
    if (status != TASK_OK) {
        fprintf(stderr, "Erro: saida nao definida: %s.\n", mensagem_status(status));
    }
}

static void tratar_append(char **palavras, int n) {
    if (n != 3) {
        fprintf(stderr, "Erro: uso: append <tarefa> <arquivo>\n");
        return;
    }
    
    StatusTarefa status = definir_saida(palavras[1], palavras[2], 1);
    if (status != TASK_OK) {
        fprintf(stderr, "Erro: append nao definido: %s.\n", mensagem_status(status));
    }
}

static void tratar_workdir(char **palavras, int n) {
    if (n != 2) {
        fprintf(stderr, "Erro: uso: workdir <diretório>\n");
        return;
    }
    if (chdir(palavras[1]) != 0) {
        fprintf(stderr, "processflow: %s: %s.\n", palavras[1], strerror(errno));
        return;
    }
}

static int resolver_tarefas(char **palavras, int n, Tarefa *destino[]) {
    int qtd = 0;
    for (int i = 0; i < n; i++) {
        Tarefa *tarefa = buscar_tarefa(palavras[i]);
        if (tarefa == NULL) {
            fprintf(stderr, "Erro: tarefa '%s' nao existe.\n", palavras[i]);
            continue;
        }
        destino[qtd++] = tarefa;
    }
    return qtd;
}

static int resolver_tarefas_estrito(char **palavras, int n, Tarefa *destino[]) {
    int qtd = resolver_tarefas(palavras, n, destino);
    if (qtd == n) {
        return qtd;
    }
    return 0;
}

static void tratar_run(char **palavras, int n) {
    if (n < 2) {
        fprintf(stderr, "Erro: uso: run <tarefa> | run sequential | parallel | pipe <tarefas...>\n");
        return;
    }

    if (strcmp(palavras[1], "sequential") == 0) {
        if (n < 4) {
            fprintf(stderr, "Erro: uso: run sequential <tarefa1> <tarefa2> ...\n");
            return;
        }
        
        Tarefa *tarefas[n - 2];
        int qtd = resolver_tarefas(palavras + 2, n - 2, tarefas);

        if (qtd == 0) {
            fprintf(stderr, "Erro: Nenhuma tarefa existe.\n");
            return;
        }
        executar_sequencial(tarefas, qtd);
        return;
    }
    if (strcmp(palavras[1], "parallel") == 0) {
        if (n < 4) {
            fprintf(stderr, "Erro: uso: run parallel <tarefa1> <tarefa2> ...\n");
            return;
        }
        
        Tarefa *tarefas[n - 2];
        int qtd = resolver_tarefas(palavras + 2, n - 2, tarefas);
        if (qtd == 0) {
            fprintf(stderr, "Erro: Nenhuma tarefa existe.\n");
            return;
        }
        executar_paralelo(tarefas, qtd);
        return;
    }
    if (strcmp(palavras[1], "pipe") == 0) {
        if (n < 4) {
            fprintf(stderr, "Erro: uso: run pipe <tarefa1> <tarefa2> [tarefa3...]\n");
            return;
        }
    
        Tarefa *tarefas[n - 2];
        int qtd = resolver_tarefas_estrito(palavras + 2, n - 2, tarefas);
        if (qtd == 0) {
            fprintf(stderr, "Erro: Uma das tarefas não existe.\n");
            return;
        }
    
        executar_pipeline(tarefas, qtd);
        return;
    }

    if (buscar_tarefa(palavras[1]) == NULL) {
        fprintf(stderr, "Erro: tarefa '%s' nao existe.\n", palavras[1]);
        return;
    }
    Tarefa *tarefa = buscar_tarefa(palavras[1]);
    pid_t pid = create_process(tarefa);
    int status;
    if (wait_process(pid, &status) > 0) {
        reportar_status(tarefa, status);
    }
}

static void tratar_exit(char **palavras, int n) {
    (void)palavras;
    (void)n;
    running = false;
}

static void tratar_start(char **palavras, int n) {
    if (n != 2) {
        fprintf(stderr, "Erro: uso: start <tarefa>\n");
        return;
    }

    Tarefa *tarefa = buscar_tarefa(palavras[1]);
    if (tarefa == NULL) {
        fprintf(stderr, "Erro: tarefa '%s' nao existe.\n", palavras[1]);
        return;
    }

    if (!ha_espaco_job()) {
        fprintf(stderr, "Erro: a tabela de jobs esta cheia.\n");
        return;
    }

    pid_t pid = create_process(tarefa);
    if (pid <= 0) {
        return;
    }

    int id = registrar_job(pid, tarefa);
    if (id < 0) {
        fprintf(stderr, "Erro: nao foi possivel registrar o job.\n");
        return;
    }

    printf("[%d] %d\n", id, (int)pid);
    fflush(stdout);
}

static void tratar_jobs(char **palavras, int n) {
    (void)palavras;
    if (n != 1) {
        fprintf(stderr, "Erro: uso: jobs\n");
        return;
    }
    colher_jobs();
    listar_jobs();
}

static void tratar_wait(char **palavras, int n) {
    if (n != 2) {
        fprintf(stderr, "Erro: uso: wait <jobId>\n");
        return;
    }

    char *fim;
    errno = 0;
    long id = strtol(palavras[1], &fim, 10);
    if (errno != 0 || fim == palavras[1] || *fim != '\0' || id <= 0) {
        fprintf(stderr, "Erro: '%s' nao e um id de job valido.\n", palavras[1]);
        return;
    }

    Job *job = buscar_job((int)id);
    if (job == NULL) {
        fprintf(stderr, "Erro: job [%ld] nao existe.\n", id);
        return;
    }

    if (esperar_job(job)) {
        reportar_status(job->tarefa, job->status);
    } else {
        fprintf(stderr, "Erro: desfecho do job [%d] desconhecido.\n", job->id);
    }
}

typedef struct {
    char *nome;
    void (*tratador)(char **palavras, int n);
} Comando;

static const Comando comandos[] = {
    { "task", tratar_task },
    { "input", tratar_input },
    { "output", tratar_output },
    { "append", tratar_append },
    { "workdir", tratar_workdir },
    { "start", tratar_start },
    { "jobs", tratar_jobs },
    { "wait", tratar_wait },
    { "run",  tratar_run  },
    { "exit", tratar_exit },
};

static void despachar(char **palavras, int n) {
    if (n == 0) {
        return;
    }

    for (size_t i = 0; i < sizeof comandos / sizeof comandos[0]; i++) {
        if (strcmp(palavras[0], comandos[i].nome) == 0) {
            comandos[i].tratador(palavras, n);
            return;
        }
    }
    
    fprintf(stderr, "Erro: comando desconhecido: '%s'.\n", palavras[0]);
}

void leitura(FILE *arquivo, int modo){
    char buffer[300];
    char *palavras[MAX_PALAVRAS];

    while (running) {
        colher_jobs();
        if (modo == 1){
            fprintf(stderr, "processflow> ");
            fflush(stderr);
        }
        fflush(stdout);
        char *result = fgets(buffer, 300, arquivo);
        if (result == NULL) {
            break;
        }

        if (!coube(buffer) && !feof(arquivo)) {
            descartar_resto(arquivo);
            fprintf(stderr, "Erro: linha longa demais, ignorada.\n");
            continue;
        }

        if (modo == 0){
            printf("%s", buffer);
            fflush(stdout);
        }

        int n = dividir_palavras(buffer, palavras, MAX_PALAVRAS);
        despachar(palavras, n);
    }
    drenar_jobs();
}

int main (int argc, char *argv[]){
    if (argc == 1) {
        leitura(stdin, 1);
    }
    if (argc == 2) {
        FILE *arquivo = fopen(argv[1], "r");
        if (arquivo == NULL) {
            fprintf(stderr, "Erro: arquivo não encontrado.\n");
            return 1;
        }
        leitura(arquivo, 0);
        fclose(arquivo);
    }
    if (argc > 2) {
        fprintf(stderr, "Erro: número de argumentos inválido.\n");
        return 1;
    }
}
