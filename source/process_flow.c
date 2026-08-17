#include <stdio.h>
#include <stdbool.h>

bool isappopen = true;

int main (int argc, char *argv[]){
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            printf("Lendo o arquivo: %s",argv[i]);
        }
    }
    else {
        printf("Iniciando modo interativo...");
    }
}
