#ifndef TASK_H
#define TASK_H

#define MAX_TASKS     64
#define MAX_N_LEN  64
#define MAX_P_LEN  256
#define MAX_ARGS      16
#define MAX_ARG_LEN   256

typedef struct {
    char name[MAX_N_LEN];
    char programa[MAX_P_LEN];
    char args[MAX_ARGS][MAX_ARG_LEN];
    int  argc;
    char entrada[MAX_P_LEN];
    char saida[MAX_P_LEN];
    int anexo;
} Tarefa;

typedef enum {
    TASK_OK = 0,
    TASK_E_TABELA_CHEIA,
    TASK_E_NOME_DUPLICADO,
    TASK_E_CAMPO_LONGO,
    TASK_E_ARGS_DEMAIS,
    TASK_E_NAO_ENCONTRADA
} StatusTarefa;

StatusTarefa cadastrar_tarefa(char *nome, char *programa,
                           char *args[], int argc);
StatusTarefa definir_entrada(char *nome, char *arquivo);
StatusTarefa definir_saida(char *nome, char *arquivo, int anexo);
Tarefa *buscar_tarefa(char *nome);

#endif
