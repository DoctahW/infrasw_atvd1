#include "tasks.h"
#include <string.h>
#include <stdbool.h>
#include <stddef.h>


static Tarefa tabela[MAX_TASKS];
static int total = 0;

static bool copiar(char *destino, size_t tamanho, char *origem) {
    if (origem == NULL || strlen(origem) >= tamanho) {
        return false;
    }
    strcpy(destino, origem);
    return true;
}

StatusTarefa cadastrar_tarefa(char *nome, char *programa,
                          char *args[], int argc) {

    if (total >= MAX_TASKS){
        return TASK_E_TABELA_CHEIA;
    }
    if (buscar_tarefa(nome) != NULL){
        return TASK_E_NOME_DUPLICADO;
    }
    if (argc < 0 || argc > MAX_ARGS){
        return TASK_E_ARGS_DEMAIS;
    }

    Tarefa *t = &tabela[total];
    if (!copiar(t->name, sizeof t->name, nome)){
        return TASK_E_CAMPO_LONGO;
    }
    if (!copiar(t->programa, sizeof t->programa, programa)){
        return TASK_E_CAMPO_LONGO;
    }

    for (int i = 0; i < argc; i++)
        if (!copiar(t->args[i], sizeof t->args[i], args[i])){
            return TASK_E_CAMPO_LONGO;
        }

    t->argc = argc;
    total++;
    return TASK_OK;
}

Tarefa *buscar_tarefa(char *nome) {
    for (int i = 0; i < total; i++) {
        if (strcmp(tabela[i].name, nome) == 0) {
            return &tabela[i];
        }
    }
    return NULL;
}
