/* test_tasks.c — suite de testes para parser.c e tasks.c
 *
 * Cada teste roda num processo filho (fork), por dois motivos:
 *  1. a tabela estatica volta limpa em cada teste;
 *  2. se um teste estourar a memoria, a suite sobrevive e reporta CRASH.
 *
 * Compilar:  gcc -std=gnu11 -Wall -Wextra -g test_tasks.c parser.c tasks.c -o test_tasks
 * Rodar:     ./test_tasks
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "parser.h"
#include "tasks.h"

static int falhas = 0;
static int total_testes = 0;

/* ---- infraestrutura ---- */

#define CHECK(cond, msg, ...)                                            \
    do {                                                                 \
        if (!(cond)) {                                                   \
            printf("FALHOU\n      -> " msg "\n", ##__VA_ARGS__);         \
            fflush(stdout);                                              \
            _exit(1);                                                    \
        }                                                                \
    } while (0)

static void roda(const char *nome, void (*teste)(void)) {
    total_testes++;
    printf("[%02d] %-52s ", total_testes, nome);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) {
        teste();
        _exit(0);
    }
    int status;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status)) {
        printf("CRASH (sinal %d)\n", WTERMSIG(status));
        falhas++;
    } else if (WEXITSTATUS(status) != 0) {
        falhas++;   /* o filho ja imprimiu o motivo */
    } else {
        printf("ok\n");
    }
}

/* ================= PARSER ================= */

static void t_parser_conta(void) {
    char linha[] = "task listar /bin/ls -l\n";
    char *p[64];
    int n = dividir_palavras(linha, p, 64);
    CHECK(n == 4, "esperava retorno 4 (palavras encontradas), veio %d", n);
}

static void t_parser_tokens(void) {
    char linha[] = "task listar /bin/ls -l\n";
    char *p[64];
    dividir_palavras(linha, p, 64);
    CHECK(p[0] && strcmp(p[0], "task") == 0, "p[0] deveria ser 'task'");
    CHECK(p[1] && strcmp(p[1], "listar") == 0, "p[1] deveria ser 'listar'");
    CHECK(p[2] && strcmp(p[2], "/bin/ls") == 0, "p[2] deveria ser '/bin/ls'");
    CHECK(p[3] && strcmp(p[3], "-l") == 0, "p[3] deveria ser '-l'");
    CHECK(p[4] == NULL, "p[4] deveria ser NULL");
}

static void t_parser_espacos(void) {
    char linha[] = "   task    listar\t\t/bin/ls   \n";
    char *p[64];
    int n = dividir_palavras(linha, p, 64);
    CHECK(p[0] && strcmp(p[0], "task") == 0, "espacos multiplos viraram token vazio");
    CHECK(p[3] == NULL, "deveria ter exatamente 3 palavras");
    CHECK(n == 3, "esperava retorno 3, veio %d", n);
}

static void t_parser_linha_vazia(void) {
    char linha[] = "\n";
    char *p[64];
    int n = dividir_palavras(linha, p, 64);
    CHECK(p[0] == NULL, "linha vazia deveria devolver p[0] == NULL");
    CHECK(n == 0, "esperava retorno 0, veio %d", n);
}

static void t_parser_estouro(void) {
    /* 10 palavras, vetor de 4: cabem 3 + NULL, sem escrever fora */
    char linha[] = "a b c d e f g h i j";
    char *p[5];
    p[4] = (char *) 0xDEAD;               /* canario logo depois do vetor util */
    int n = dividir_palavras(linha, p, 4);
    CHECK(p[4] == (char *) 0xDEAD, "ESCREVEU FORA do limite de quant");
    CHECK(p[3] == NULL, "faltou o NULL na ultima posicao util");
    CHECK(n == 3, "esperava retorno 3, veio %d", n);
}

static void t_parser_quant_zero(void) {
    char linha[] = "a b c";
    char *p[2];
    char *canario = (char *) 0xBEEF;
    p[0] = canario;
    (void) dividir_palavras(linha, p, 0);
    CHECK(p[0] == canario, "com quant=0 gravou NULL num vetor sem espaco");
}

/* ================= TASKS ================= */

static void t_cadastro_simples(void) {
    char *args[] = { "-l" };
    StatusTarefa s = cadastrar_tarefa("listar", "/bin/ls", args, 1);
    CHECK(s == TASK_OK, "cadastro devolveu %d, esperava TASK_OK", s);

    Tarefa *t = buscar_tarefa("listar");
    CHECK(t != NULL, "buscar_tarefa nao achou a tarefa recem cadastrada");
    CHECK(strcmp(t->name, "listar") == 0, "nome guardado: '%s'", t->name);
    CHECK(strcmp(t->programa, "/bin/ls") == 0, "programa guardado: '%s'", t->programa);
    CHECK(t->argc == 1, "argc guardado: %d", t->argc);
    CHECK(strcmp(t->args[0], "-l") == 0, "args[0] guardado: '%s'", t->args[0]);
}

static void t_busca_inexistente(void) {
    Tarefa *t = buscar_tarefa("nao_existe");
    CHECK(t == NULL, "buscar_tarefa devolveu algo para nome inexistente");
}

/* O TESTE DE ACEITACAO DA FASE 2:
 * cadastra a partir do buffer de leitura, reescreve o buffer,
 * e confere se a tarefa sobreviveu. */
static void t_sobrevive_reescrita_do_buffer(void) {
    char buffer[300];
    char *p[64];

    strcpy(buffer, "task listar /bin/ls -l\n");
    dividir_palavras(buffer, p, 64);
    StatusTarefa s = cadastrar_tarefa(p[1], p[2], &p[3], 1);
    CHECK(s == TASK_OK, "cadastro inicial falhou (%d)", s);

    /* proxima volta do laco: fgets reescreve o mesmo buffer */
    memset(buffer, 0, sizeof buffer);
    strcpy(buffer, "outra linha qualquer bem diferente\n");
    dividir_palavras(buffer, p, 64);

    Tarefa *t = buscar_tarefa("listar");
    CHECK(t != NULL, "a tarefa sumiu depois que o buffer foi reescrito");
    CHECK(strcmp(t->name, "listar") == 0, "nome virou '%s' apos reescrita", t->name);
    CHECK(strcmp(t->programa, "/bin/ls") == 0, "programa virou '%s'", t->programa);
    CHECK(strcmp(t->args[0], "-l") == 0, "args[0] virou '%s'", t->args[0]);
}

static void t_duplicado(void) {
    char *args[] = { "-l" };
    cadastrar_tarefa("listar", "/bin/ls", args, 1);
    StatusTarefa s = cadastrar_tarefa("listar", "/bin/pwd", args, 0);
    CHECK(s == TASK_E_NOME_DUPLICADO, "esperava TASK_E_NOME_DUPLICADO, veio %d", s);
}

static void t_tabela_cheia(void) {
    char nome[32];
    for (int i = 0; i < MAX_TASKS; i++) {
        sprintf(nome, "t%d", i);
        StatusTarefa s = cadastrar_tarefa(nome, "/bin/true", NULL, 0);
        CHECK(s == TASK_OK, "cadastro %d falhou antes de encher (%d)", i, s);
    }
    StatusTarefa s = cadastrar_tarefa("estouro", "/bin/true", NULL, 0);
    CHECK(s == TASK_E_TABELA_CHEIA, "esperava TASK_E_TABELA_CHEIA, veio %d", s);
}

static void t_args_demais(void) {
    char *args[MAX_ARGS + 5];
    for (int i = 0; i < MAX_ARGS + 5; i++) args[i] = "x";
    StatusTarefa s = cadastrar_tarefa("muitos", "/bin/true", args, MAX_ARGS + 5);
    CHECK(s == TASK_E_ARGS_DEMAIS, "esperava TASK_E_ARGS_DEMAIS, veio %d", s);
}

static void t_args_no_limite(void) {
    char *args[MAX_ARGS];
    for (int i = 0; i < MAX_ARGS; i++) args[i] = "x";
    StatusTarefa s = cadastrar_tarefa("limite", "/bin/true", args, MAX_ARGS);
    CHECK(s == TASK_OK, "MAX_ARGS argumentos deveriam caber, veio %d", s);
    Tarefa *t = buscar_tarefa("limite");
    CHECK(t != NULL && t->argc == MAX_ARGS, "argc guardado errado");
}

static void t_argc_negativo(void) {
    StatusTarefa s = cadastrar_tarefa("negativo", "/bin/true", NULL, -1);
    CHECK(s != TASK_OK, "aceitou argc = -1 e devolveu TASK_OK");
}

static void t_nome_longo(void) {
    char nome[MAX_N_LEN + 50];
    memset(nome, 'a', sizeof nome - 1);
    nome[sizeof nome - 1] = '\0';
    StatusTarefa s = cadastrar_tarefa(nome, "/bin/true", NULL, 0);
    CHECK(s == TASK_E_CAMPO_LONGO, "esperava TASK_E_CAMPO_LONGO, veio %d", s);
}

static void t_arg_longo(void) {
    char arg[MAX_ARG_LEN + 50];
    memset(arg, 'b', sizeof arg - 1);
    arg[sizeof arg - 1] = '\0';
    char *args[] = { "ok", arg };
    StatusTarefa s = cadastrar_tarefa("comarglongo", "/bin/true", args, 2);
    CHECK(s == TASK_E_CAMPO_LONGO, "esperava TASK_E_CAMPO_LONGO, veio %d", s);
}

/* depois de um cadastro que falhou no meio, a tabela nao pode ficar suja */
static void t_falha_nao_suja_tabela(void) {
    char arg[MAX_ARG_LEN + 50];
    memset(arg, 'b', sizeof arg - 1);
    arg[sizeof arg - 1] = '\0';
    char *ruins[] = { arg };
    cadastrar_tarefa("meiocadastro", "/bin/true", ruins, 1);

    Tarefa *t = buscar_tarefa("meiocadastro");
    CHECK(t == NULL, "tarefa que falhou no meio ficou visivel na tabela");

    char *bons[] = { "-l" };
    StatusTarefa s = cadastrar_tarefa("boa", "/bin/ls", bons, 1);
    CHECK(s == TASK_OK, "cadastro seguinte falhou (%d)", s);
    Tarefa *b = buscar_tarefa("boa");
    CHECK(b != NULL && strcmp(b->programa, "/bin/ls") == 0, "slot reaproveitado ficou corrompido");
}

static void t_nome_nulo(void) {
    StatusTarefa s = cadastrar_tarefa(NULL, "/bin/true", NULL, 0);
    CHECK(s != TASK_OK, "aceitou nome NULL");
}

int main(void) {
    printf("\n=== PARSER ===\n");
    roda("conta as palavras no retorno", t_parser_conta);
    roda("tokens corretos", t_parser_tokens);
    roda("multiplos espacos e tabs", t_parser_espacos);
    roda("linha vazia", t_parser_linha_vazia);
    roda("respeita o limite quant (nao escreve fora)", t_parser_estouro);
    roda("quant = 0 nao escreve nada", t_parser_quant_zero);

    printf("\n=== TASKS ===\n");
    roda("cadastro simples + busca", t_cadastro_simples);
    roda("busca de nome inexistente devolve NULL", t_busca_inexistente);
    roda("ACEITACAO F2: sobrevive a reescrita do buffer", t_sobrevive_reescrita_do_buffer);
    roda("recusa nome duplicado", t_duplicado);
    roda("recusa quando a tabela enche", t_tabela_cheia);
    roda("recusa argc > MAX_ARGS", t_args_demais);
    roda("aceita exatamente MAX_ARGS", t_args_no_limite);
    roda("recusa argc negativo", t_argc_negativo);
    roda("recusa nome longo demais", t_nome_longo);
    roda("recusa argumento longo demais", t_arg_longo);
    roda("cadastro que falha nao suja a tabela", t_falha_nao_suja_tabela);
    roda("nao estoura com nome NULL", t_nome_nulo);

    printf("\n----------------------------------------\n");
    printf("%d testes, %d falha(s)\n\n", total_testes, falhas);
    return falhas > 0 ? 1 : 0;
}
