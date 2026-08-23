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
#include <sys/stat.h>
#include <fcntl.h>

#include "parser.h"
#include "tasks.h"
#include "executor.h"

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

/* ================= REDIRECIONAMENTO — Fase 5 ================= */
/* Dois níveis, de propósito:
 *   (a) tasks.c  — o comando *anexa uma propriedade* a uma tarefa cadastrada;
 *   (b) executor.c — a propriedade *vira descritor* no filho, entre fork e exec.
 * Um teste verde em (a) com (b) vermelho significa que a informação está
 * guardada certa e quem consome é que ignora.
 */

static char dir_tmp[128];

static void prepara_dir(void) {
    snprintf(dir_tmp, sizeof dir_tmp, "/tmp/pf_utest_%d", (int) getpid());
    mkdir(dir_tmp, 0755);
}

static char *caminho(char *destino, size_t n, const char *nome) {
    snprintf(destino, n, "%s/%s", dir_tmp, nome);
    return destino;
}

static void escreve(const char *arq, const char *texto) {
    FILE *f = fopen(arq, "w");
    CHECK(f != NULL, "nao consegui preparar o arquivo %s", arq);
    fputs(texto, f);
    fclose(f);
}

static void le_tudo(const char *arq, char *destino, size_t n) {
    destino[0] = '\0';
    FILE *f = fopen(arq, "r");
    if (f == NULL) return;
    size_t lidos = fread(destino, 1, n - 1, f);
    destino[lidos] = '\0';
    fclose(f);
}

static int conta_linhas(const char *arq) {
    FILE *f = fopen(arq, "r");
    if (f == NULL) return -1;
    int n = 0, c, ultimo = '\n';
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') n++;
        ultimo = c;
    }
    if (ultimo != '\n') n++;      /* ultima linha sem \n ainda e uma linha */
    fclose(f);
    return n;
}

/* Monta uma Tarefa na mao: o executor recebe um ponteiro, nao um nome
 * cadastrado — da pra exercita-lo sem passar pela tabela. */
static void monta(Tarefa *t, char *programa, char *arg,
                  char *entrada, char *saida, int anexo) {
    memset(t, 0, sizeof *t);
    strcpy(t->name, "teste");
    strcpy(t->programa, programa);
    if (arg != NULL) {
        strcpy(t->args[0], arg);
        t->argc = 1;
    }
    if (entrada != NULL) strcpy(t->entrada, entrada);
    if (saida != NULL)   strcpy(t->saida, saida);
    t->anexo = anexo;
}

/* devolve o status bruto do filho; -1 se nem houve filho pra esperar */
static int roda_ate_o_fim(Tarefa *t) {
    int status = -1;
    pid_t pid = create_process(t);
    if (wait_process(pid, &status) <= 0) return -1;
    return status;
}

/* ---- (a) a propriedade fica guardada na tarefa ---- */

static void t_redir_campos_comecam_vazios(void) {
    StatusTarefa s = cadastrar_tarefa("nova", "/bin/true", NULL, 0);
    CHECK(s == TASK_OK, "cadastro falhou (%d)", s);
    Tarefa *t = buscar_tarefa("nova");
    CHECK(t != NULL, "buscar_tarefa nao achou a tarefa");
    CHECK(t->entrada[0] == '\0', "entrada nasceu suja: '%s'", t->entrada);
    CHECK(t->saida[0] == '\0', "saida nasceu suja: '%s'", t->saida);
    CHECK(t->anexo == 0, "anexo nasceu com %d, esperado 0", t->anexo);
}

static void t_input_guarda_o_arquivo(void) {
    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    StatusTarefa s = definir_entrada("ord", "nomes.txt");
    CHECK(s == TASK_OK, "definir_entrada devolveu %d, esperava TASK_OK", s);
    Tarefa *t = buscar_tarefa("ord");
    CHECK(strcmp(t->entrada, "nomes.txt") == 0, "entrada guardada: '%s'", t->entrada);
    CHECK(t->saida[0] == '\0', "input mexeu na saida: '%s'", t->saida);
}

static void t_output_guarda_e_deixa_anexo_zero(void) {
    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    StatusTarefa s = definir_saida("ord", "resultado.txt", 0);
    CHECK(s == TASK_OK, "definir_saida devolveu %d", s);
    Tarefa *t = buscar_tarefa("ord");
    CHECK(strcmp(t->saida, "resultado.txt") == 0, "saida guardada: '%s'", t->saida);
    CHECK(t->anexo == 0, "output deveria deixar anexo=0, veio %d", t->anexo);
    CHECK(t->entrada[0] == '\0', "output mexeu na entrada: '%s'", t->entrada);
}

static void t_append_liga_o_anexo(void) {
    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    definir_saida("ord", "historico.txt", 1);
    Tarefa *t = buscar_tarefa("ord");
    CHECK(strcmp(t->saida, "historico.txt") == 0, "saida guardada: '%s'", t->saida);
    CHECK(t->anexo == 1, "append deveria ligar anexo, veio %d", t->anexo);
}

/* output e append sao a mesma operacao com flag diferente: o ultimo manda */
static void t_output_depois_de_append_volta_a_truncar(void) {
    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    definir_saida("ord", "hist.txt", 1);
    definir_saida("ord", "res.txt", 0);
    Tarefa *t = buscar_tarefa("ord");
    CHECK(strcmp(t->saida, "res.txt") == 0, "ultimo output nao venceu: '%s'", t->saida);
    CHECK(t->anexo == 0, "anexo ficou preso em 1 depois de um output");
}

static void t_append_depois_de_output_liga_o_anexo(void) {
    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    definir_saida("ord", "res.txt", 0);
    definir_saida("ord", "res.txt", 1);
    Tarefa *t = buscar_tarefa("ord");
    CHECK(t->anexo == 1, "append depois de output nao ligou o anexo");
}

static void t_input_sobrescreve_o_anterior(void) {
    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    definir_entrada("ord", "primeiro.txt");
    definir_entrada("ord", "segundo.txt");
    Tarefa *t = buscar_tarefa("ord");
    CHECK(strcmp(t->entrada, "segundo.txt") == 0, "entrada ficou '%s'", t->entrada);
}

static void t_redir_em_tarefa_inexistente(void) {
    StatusTarefa a = definir_entrada("fantasma", "x.txt");
    CHECK(a == TASK_E_NAO_ENCONTRADA, "input em tarefa inexistente devolveu %d", a);
    StatusTarefa b = definir_saida("fantasma", "x.txt", 0);
    CHECK(b == TASK_E_NAO_ENCONTRADA, "output em tarefa inexistente devolveu %d", b);
    StatusTarefa c = definir_saida("fantasma", "x.txt", 1);
    CHECK(c == TASK_E_NAO_ENCONTRADA, "append em tarefa inexistente devolveu %d", c);
}

static void t_redir_nao_vaza_entre_tarefas(void) {
    cadastrar_tarefa("uma", "/bin/true", NULL, 0);
    cadastrar_tarefa("outra", "/bin/true", NULL, 0);
    definir_entrada("uma", "entrada_da_uma.txt");
    definir_saida("uma", "saida_da_uma.txt", 1);

    Tarefa *o = buscar_tarefa("outra");
    CHECK(o->entrada[0] == '\0', "'outra' herdou a entrada: '%s'", o->entrada);
    CHECK(o->saida[0] == '\0', "'outra' herdou a saida: '%s'", o->saida);
    CHECK(o->anexo == 0, "'outra' herdou anexo=%d", o->anexo);
}

static void t_caminho_longo_nao_corrompe(void) {
    char longo[MAX_P_LEN + 50];
    memset(longo, 'c', sizeof longo - 1);
    longo[sizeof longo - 1] = '\0';

    cadastrar_tarefa("ord", "/usr/bin/sort", NULL, 0);
    definir_entrada("ord", "bom.txt");
    StatusTarefa s = definir_entrada("ord", longo);
    CHECK(s == TASK_E_CAMPO_LONGO, "esperava TASK_E_CAMPO_LONGO, veio %d", s);

    Tarefa *t = buscar_tarefa("ord");
    CHECK(strcmp(t->entrada, "bom.txt") == 0,
          "a recusa deixou a entrada corrompida: '%s'", t->entrada);
}

/* espelha o teste de aceitacao da Fase 2, agora para os campos novos */
static void t_redir_sobrevive_reescrita_do_buffer(void) {
    char buffer[300];
    char *p[64];

    strcpy(buffer, "task ord /usr/bin/sort\n");
    dividir_palavras(buffer, p, 64);
    cadastrar_tarefa(p[1], p[2], &p[3], 0);

    strcpy(buffer, "input ord nomes.txt\n");
    dividir_palavras(buffer, p, 64);
    definir_entrada(p[1], p[2]);

    strcpy(buffer, "output ord resultado.txt\n");
    dividir_palavras(buffer, p, 64);
    definir_saida(p[1], p[2], 0);

    memset(buffer, 0, sizeof buffer);
    strcpy(buffer, "run ord\n");
    dividir_palavras(buffer, p, 64);

    Tarefa *t = buscar_tarefa("ord");
    CHECK(t != NULL, "a tarefa sumiu");
    CHECK(strcmp(t->entrada, "nomes.txt") == 0, "entrada virou '%s'", t->entrada);
    CHECK(strcmp(t->saida, "resultado.txt") == 0, "saida virou '%s'", t->saida);
}

/* ---- (b) a propriedade vira descritor no filho ---- */

static void t_exec_saida_recebe_o_stdout(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "saida.txt");

    Tarefa t;
    monta(&t, "/bin/echo", "alo", NULL, arq, 0);
    int status = roda_ate_o_fim(&t);
    CHECK(status != -1, "nao houve filho pra esperar");
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "o filho nao saiu limpo (status bruto %d)", status);

    char conteudo[128];
    le_tudo(arq, conteudo, sizeof conteudo);
    CHECK(strcmp(conteudo, "alo\n") == 0,
          "o arquivo de saida ficou com '%s', esperado 'alo\\n'", conteudo);
}

static void t_exec_entrada_alimenta_o_filho(void) {
    prepara_dir();
    char entrada[256], saida[256];
    caminho(entrada, sizeof entrada, "entrada.txt");
    caminho(saida, sizeof saida, "copia.txt");
    escreve(entrada, "linha um\nlinha dois\n");

    Tarefa t;
    monta(&t, "/bin/cat", NULL, entrada, saida, 0);
    roda_ate_o_fim(&t);

    char conteudo[128];
    le_tudo(saida, conteudo, sizeof conteudo);
    CHECK(strcmp(conteudo, "linha um\nlinha dois\n") == 0,
          "o /bin/cat nao leu do arquivo: saiu '%s'", conteudo);
}

/* ACEITACAO F5, metade 1: ordenar de verdade lendo e escrevendo em arquivo */
static void t_exec_ordena_do_arquivo_para_o_arquivo(void) {
    prepara_dir();
    char entrada[256], saida[256];
    caminho(entrada, sizeof entrada, "nomes.txt");
    caminho(saida, sizeof saida, "resultado.txt");
    escreve(entrada, "carlos\nana\nbruno\n");

    Tarefa t;
    monta(&t, "/usr/bin/sort", NULL, entrada, saida, 0);
    roda_ate_o_fim(&t);

    char conteudo[128];
    le_tudo(saida, conteudo, sizeof conteudo);
    CHECK(strcmp(conteudo, "ana\nbruno\ncarlos\n") == 0,
          "resultado da ordenacao: '%s'", conteudo);
}

/* anexo = 0 -> a segunda execucao apaga a primeira */
static void t_exec_output_trunca_na_segunda_execucao(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "res.txt");

    Tarefa t;
    monta(&t, "/bin/echo", "primeira", NULL, arq, 0);
    roda_ate_o_fim(&t);
    monta(&t, "/bin/echo", "segunda", NULL, arq, 0);
    roda_ate_o_fim(&t);

    int n = conta_linhas(arq);
    CHECK(n == 1, "output deveria truncar: o arquivo ficou com %d linha(s)", n);

    char conteudo[128];
    le_tudo(arq, conteudo, sizeof conteudo);
    CHECK(strcmp(conteudo, "segunda\n") == 0, "ficou '%s'", conteudo);
}

/* ACEITACAO F5, metade 2: anexo = 1 -> a segunda NAO apaga a primeira */
static void t_exec_append_nao_trunca_na_segunda_execucao(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "hist.txt");

    Tarefa t;
    monta(&t, "/bin/echo", "primeira", NULL, arq, 1);
    roda_ate_o_fim(&t);
    monta(&t, "/bin/echo", "segunda", NULL, arq, 1);
    roda_ate_o_fim(&t);

    char conteudo[128];
    le_tudo(arq, conteudo, sizeof conteudo);
    int n = conta_linhas(arq);
    CHECK(n == 2, "append deveria somar: %d linha(s), conteudo '%s'", n, conteudo);
    CHECK(strcmp(conteudo, "primeira\nsegunda\n") == 0,
          "ordem/conteudo do append: '%s'", conteudo);
}

/* append num arquivo que ja existia tem que preservar o que estava la */
static void t_exec_append_preserva_arquivo_preexistente(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "hist2.txt");
    escreve(arq, "de antes\n");

    Tarefa t;
    monta(&t, "/bin/echo", "de agora", NULL, arq, 1);
    roda_ate_o_fim(&t);

    char conteudo[128];
    le_tudo(arq, conteudo, sizeof conteudo);
    CHECK(strcmp(conteudo, "de antes\nde agora\n") == 0,
          "append apagou o que ja existia: '%s'", conteudo);
}

/* append num arquivo que ainda nao existe tem que cria-lo */
static void t_exec_append_cria_arquivo_que_nao_existe(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "novo_por_append.txt");

    Tarefa t;
    monta(&t, "/bin/echo", "unica", NULL, arq, 1);
    int status = roda_ate_o_fim(&t);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "append em arquivo inexistente falhou (status bruto %d)", status);

    char conteudo[128];
    le_tudo(arq, conteudo, sizeof conteudo);
    CHECK(strcmp(conteudo, "unica\n") == 0, "o arquivo criado ficou com '%s'", conteudo);
}

static void t_exec_entrada_inexistente_falha_o_filho(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "nao_existe_mesmo.txt");

    Tarefa t;
    monta(&t, "/bin/cat", NULL, arq, NULL, 0);
    int status = roda_ate_o_fim(&t);
    CHECK(status != -1, "nao houve filho: o pai desistiu antes do fork?");
    CHECK(WIFEXITED(status), "o filho morreu de sinal em vez de sair com codigo");
    CHECK(WEXITSTATUS(status) != 0,
          "entrada inexistente deveria dar codigo != 0, veio %d", WEXITSTATUS(status));
    /* chegar aqui ja prova que o pai sobreviveu */
}

static void t_exec_saida_impossivel_falha_o_filho(void) {
    Tarefa t;
    monta(&t, "/bin/echo", "x", NULL, "/nao/existe/esse/caminho/saida.txt", 0);
    int status = roda_ate_o_fim(&t);
    CHECK(status != -1, "nao houve filho pra esperar");
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) != 0,
          "saida impossivel deveria dar codigo != 0 (status bruto %d)", status);
}

/* A PERGUNTA CONCEITUAL DA FASE, virada em teste: o dup2 acontece DEPOIS do
 * fork, entao o descritor 1 do proprio ProcessFlow nao pode ter mudado. */
static void t_redir_nao_vaza_para_o_pai(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "so_do_filho.txt");

    struct stat antes, depois;
    CHECK(fstat(STDOUT_FILENO, &antes) == 0, "fstat inicial falhou");

    Tarefa t;
    monta(&t, "/bin/echo", "no filho", NULL, arq, 0);
    roda_ate_o_fim(&t);

    CHECK(fstat(STDOUT_FILENO, &depois) == 0, "fstat final falhou");
    CHECK(antes.st_dev == depois.st_dev && antes.st_ino == depois.st_ino,
          "o stdout do PAI mudou depois do run: o dup2 escapou do filho");
}

/* o pai nao pode vazar o descritor aberto para o redirecionamento: se ele
 * ficasse aberto no pai, o contador de referencias nunca zeraria. */
static void t_pai_nao_fica_com_descritor_aberto(void) {
    prepara_dir();
    char arq[256];
    caminho(arq, sizeof arq, "vazamento.txt");

    int antes = dup(STDOUT_FILENO);
    CHECK(antes >= 0, "dup inicial falhou");
    close(antes);

    Tarefa t;
    monta(&t, "/bin/echo", "x", NULL, arq, 0);
    roda_ate_o_fim(&t);

    int depois = dup(STDOUT_FILENO);
    CHECK(depois >= 0, "dup final falhou");
    close(depois);
    CHECK(depois == antes,
          "o menor descritor livre subiu de %d para %d: o pai ficou com fd aberto",
          antes, depois);
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

    printf("\n=== REDIRECIONAMENTO: a tarefa guarda a propriedade (Fase 5) ===\n");
    roda("campos de redirecionamento nascem vazios", t_redir_campos_comecam_vazios);
    roda("input guarda o arquivo e nao toca na saida", t_input_guarda_o_arquivo);
    roda("output guarda o arquivo com anexo = 0", t_output_guarda_e_deixa_anexo_zero);
    roda("append guarda o arquivo com anexo = 1", t_append_liga_o_anexo);
    roda("output depois de append volta a truncar", t_output_depois_de_append_volta_a_truncar);
    roda("append depois de output liga o anexo", t_append_depois_de_output_liga_o_anexo);
    roda("input duas vezes: o ultimo vence", t_input_sobrescreve_o_anterior);
    roda("redirecionar tarefa inexistente devolve erro", t_redir_em_tarefa_inexistente);
    roda("redirecionamento nao vaza para outra tarefa", t_redir_nao_vaza_entre_tarefas);
    roda("caminho longo demais e recusado sem corromper", t_caminho_longo_nao_corrompe);
    roda("redirecionamento sobrevive a reescrita do buffer", t_redir_sobrevive_reescrita_do_buffer);

    printf("\n=== REDIRECIONAMENTO: a propriedade vira descritor (Fase 5) ===\n");
    roda("output leva o stdout do filho para o arquivo", t_exec_saida_recebe_o_stdout);
    roda("input alimenta o stdin do filho", t_exec_entrada_alimenta_o_filho);
    roda("ACEITACAO F5 (1/2): ordena arquivo -> arquivo", t_exec_ordena_do_arquivo_para_o_arquivo);
    roda("output trunca na segunda execucao", t_exec_output_trunca_na_segunda_execucao);
    roda("ACEITACAO F5 (2/2): append duas vezes nao trunca", t_exec_append_nao_trunca_na_segunda_execucao);
    roda("append preserva arquivo preexistente", t_exec_append_preserva_arquivo_preexistente);
    roda("append cria o arquivo se nao existir", t_exec_append_cria_arquivo_que_nao_existe);
    roda("entrada inexistente faz o filho falhar, nao o pai", t_exec_entrada_inexistente_falha_o_filho);
    roda("saida impossivel faz o filho falhar, nao o pai", t_exec_saida_impossivel_falha_o_filho);
    roda("CONCEITO: o dup2 nao vaza para o stdout do pai", t_redir_nao_vaza_para_o_pai);
    roda("CONCEITO: o pai nao fica com o descritor aberto", t_pai_nao_fica_com_descritor_aberto);

    printf("\n----------------------------------------\n");
    printf("%d testes, %d falha(s)\n\n", total_testes, falhas);
    return falhas > 0 ? 1 : 0;
}
