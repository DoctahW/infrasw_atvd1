#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

#define MAX_PALAVRAS 64

bool running = true;

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

        if (modo == 0){
            printf("%s", buffer);
        }

        dividir_palavras(buffer, palavras, MAX_PALAVRAS);

        for (int i = 0; palavras[i] != NULL; i++) {
            printf("%s ", palavras[i]);
        }
        printf("\n");
        
        if (palavras[0] != NULL) {
            if (strcmp(palavras[0], "exit") == 0) {
                running = false;
            }
        }
    }
}

int main (int argc, char *argv[]){
    if (argc == 1) {
        leitura(stdin, 1);
    }
    if (argc == 2) {
        FILE *arquivo = fopen(argv[1], "r");
        if (arquivo == NULL) {
            printf("Erro: arquivo não encontrado.\n");
            return 1;
        }
        leitura(arquivo, 0);
        fclose(arquivo);
    }
    if (argc > 2) {
        printf("Erro: número de argumentos inválido.\n");
        return 1;
    }
}

