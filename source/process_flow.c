#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "tasks.h"
#include "executor.h"

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
        int qtd = 0;
        
        for (int i = 2; i < n; i++) {
            if (buscar_tarefa(palavras[i]) == NULL) {
                fprintf(stderr, "Erro: tarefa '%s' nao existe.\n", palavras[i]);
                continue;
            }
            Tarefa *tarefa = buscar_tarefa(palavras[i]);
            tarefas[qtd++] = tarefa;
        }

        if (qtd == 0) {
            fprintf(stderr, "Erro: Nenhuma tarefa existe.\n");
            return;
        }
        executar_sequencial(tarefas, qtd);
        return;
    }
    if (strcmp(palavras[1], "parallel") == 0) {
        fprintf(stderr, "Erro: 'run parallel' ainda nao implementado (Fase 4).\n");
        return;
    }
    if (strcmp(palavras[1], "pipe") == 0) {
        fprintf(stderr, "Erro: 'run pipe' ainda nao implementado (Fase 6).\n");
        return;
    }

    if (buscar_tarefa(palavras[1]) == NULL) {
        fprintf(stderr, "Erro: tarefa '%s' nao existe.\n", palavras[1]);
        return;
    }
    pid_t pid = create_process(buscar_tarefa(palavras[1]));
    wait_process(pid);
}

static void tratar_exit(char **palavras, int n) {
    (void)palavras;
    (void)n;
    running = false;
}

typedef struct {
    char *nome;
    void (*tratador)(char **palavras, int n);
} Comando;

static const Comando comandos[] = {
    { "task", tratar_task },
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
        if (modo == 1){
            printf("processflow> ");
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
