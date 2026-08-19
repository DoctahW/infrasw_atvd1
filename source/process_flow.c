#include <stdio.h>
#include <stdbool.h>
#include <string.h>

bool running = true;

void leitura(FILE *arquivo, int modo){
    char buffer[300];
    
    while (running) {
        if (modo == 1){
            printf("processflow>");
        }
        char *result = fgets(buffer, 300, arquivo);
        if (result == NULL) {
            break;
        }

        if (modo == 0){
            printf("%s", buffer);
        }
        
        if (strcmp(buffer, "exit\n") == 0) {
            running = false;
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
