#include <string.h>

void dividir_palavras(char *linha, char **palavras, int quant) {
    char *saveptr;
    char *token;

    token = strtok_r(linha, " \t\n", &saveptr);
    while (token != NULL && quant > 1) {
        *palavras++ = token;
        quant--;
        token = strtok_r(NULL, " \t\n", &saveptr);
    }
    *palavras = NULL;
}