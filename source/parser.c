#include <string.h>
#include "parser.h"

int dividir_palavras(char *linha, char **palavras, int quant) {
    if (quant <= 0) {
        return 0;
    }
    char *saveptr;
    char *token = strtok_r(linha, " \t\n", &saveptr);
    int n = 0;

    while (token != NULL && n < quant - 1) {
        palavras[n] = token;
        n++;
        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    palavras[n] = NULL;
    return n;
}
