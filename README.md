# ProcessFlow

Orquestrador de processos escrito em C para a disciplina Infraestrutura de
Software (00016) da CESAR School. O programa lê comandos (do terminal ou de um
arquivo de workflow), cadastra tarefas e as executa criando processos com
`fork()` e `exec*()`, com redirecionamento de entrada/saída via `dup2()` e
comunicação entre tarefas via `pipe()`.

## Arquivos

```
.
├── source
│   ├── process_flow.c    main, laco de leitura e os tratadores de cada comando
│   ├── parser.c          quebra uma linha em palavras (strtok_r)
│   ├── parser.h
│   ├── tasks.c           tabela de tarefas: cadastro, busca, entrada/saida
│   ├── tasks.h
│   ├── executor.c        cria e espera processos; sequencial, paralelo, pipeline
│   ├── executor.h
│   ├── jobs.c            tabela de jobs em segundo plano (start, jobs, wait)
│   └── jobs.h
├── tests
│   ├── test_tasks.c      testes unitarios de parser, tasks e executor
│   ├── run_tests.sh      testes de ponta a ponta sobre o binario
│   ├── sequencial.pf     workflow de exemplo usado nos testes de tempo
│   ├── paralelo.pf       workflow de exemplo usado nos testes de tempo
│   └── nomes.txt         entrada usada nos testes de redirecionamento
├── objects               objetos gerados pela compilacao (criado pelo make)
├── evidencias.log        registro das execucoes usadas como evidencia
├── Makefile
├── processflow           executavel gerado pelo make
└── README.md
```

Cada `.h` declara o que o `.c` de mesmo nome expõe. Os testes unitários rodam
cada caso em um processo filho, para que a tabela estática de tarefas volte
limpa a cada teste.

## Compilar

```
make
```

Gera o executável `processflow` na raiz do projeto; os objetos ficam em
`objects/`. Para limpar tudo:

```
make clean
```

## Executar

Modo interativo (mostra o prompt `processflow> `):

```
./processflow
```

Modo workflow (lê um arquivo, sem prompt, ecoando cada linha lida):

```
./processflow arquivo.pf
```

Comandos aceitos:

```
task <nome> <programa> [args...]      cadastra uma tarefa
input <tarefa> <arquivo>              redireciona a entrada da tarefa
output <tarefa> <arquivo>             redireciona a saída (trunca)
append <tarefa> <arquivo>             redireciona a saída (acrescenta)
workdir <diretório>                   muda o diretório de trabalho
run <tarefa>                          executa e espera terminar
run sequential <t1> <t2> ...          executa uma após a outra
run parallel <t1> <t2> ...            executa todas ao mesmo tempo
run pipe <t1> <t2> ...                liga a saída de uma à entrada da seguinte
start <tarefa>                        executa em segundo plano e imprime [id] pid
jobs                                  lista os jobs em segundo plano
wait <jobId>                          espera um job em segundo plano terminar
exit                                  encerra o programa
```

Exemplo:

```
task contar /usr/bin/wc -l
input contar tests/nomes.txt
run contar
exit
```

## Testar

```
make test
```

Roda as duas suítes e retorna diferente de zero se alguma falhar. Para rodar
separadamente:

```
make test-unit    # apenas os testes unitários (test_tasks)
make test-e2e     # apenas os testes de ponta a ponta (run_tests.sh)
```

## Sistema operacional

Implementado e testado em Linux (Arch Linux, kernel 7.0.9, x86_64) com GCC 16.1.1
e padrão `gnu11`. Usa apenas chamadas POSIX (`fork`, `execv`, `waitpid`, `pipe`,
`dup2`, `open`, `chdir`), então deve compilar em qualquer sistema POSIX com GCC.
