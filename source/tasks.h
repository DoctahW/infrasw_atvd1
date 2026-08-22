#ifndef TASK_H
#define TASK_H

#define MAX_TASKS     32
#define MAX_N_LEN  64
#define MAX_P_LEN  256
#define MAX_ARGS      16
#define MAX_ARG_LEN   64

typedef struct {
    char name[MAX_N_LEN];
    char programa[MAX_P_LEN];
    char args[MAX_ARGS][MAX_ARG_LEN];
    int  argc;
} Tarefa;

typedef enum {
    TASK_OK = 0,
    TASK_E_TABELA_CHEIA,
    TASK_E_NOME_DUPLICADO,
    TASK_E_CAMPO_LONGO,
    TASK_E_ARGS_DEMAIS
} StatusTarefa;

StatusTarefa cadastrar_tarefa(char *nome, char *programa,
                           char *args[], int argc);
Tarefa *buscar_tarefa(char *nome);

#endif
