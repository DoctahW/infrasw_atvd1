#!/bin/bash
# run_tests.sh — testes de ponta a ponta do binario ./processflow
# Cada teste roda o programa de verdade e confere saida e codigo de retorno.

BIN=./processflow
TESTDIR=$(dirname "$0")
ok=0; falhou=0

check() {   # check "nome" "condicao_ja_avaliada"  -> usa $? do caller
    if [ "$2" = "0" ]; then
        printf "  ok     %s\n" "$1"; ok=$((ok+1))
    else
        printf "  FALHA  %s\n" "$1"; falhou=$((falhou+1))
    fi
}

cronometra() {   # cronometra <arquivo.pf>  -> ecoa milissegundos
    local ini fim
    ini=$(date +%s%N)
    timeout 30 $BIN "$1" >/dev/null 2>&1
    fim=$(date +%s%N)
    echo $(( (fim - ini) / 1000000 ))
}

echo
echo "=== MODO INTERATIVO ==="

out=$(echo "exit" | timeout 5 $BIN); rc=$?
[ $rc -eq 0 ]; check "sai com 'exit' sem travar (rc=$rc)" $?

echo "$out" | grep -q "processflow>"; check "mostra o prompt processflow>" $?

out=$(printf "" | timeout 5 $BIN); rc=$?
[ $rc -eq 0 ]; check "CTRL-D (EOF) sem 'exit' encerra limpo (rc=$rc)" $?

printf "\n\n\nexit\n" | timeout 5 $BIN > /dev/null 2>&1; rc=$?
[ $rc -eq 0 ]; check "linha vazia nao quebra (rc=$rc)" $?

printf "   task    listar   /bin/ls   -l   \nexit\n" | timeout 5 $BIN > /dev/null 2>&1; rc=$?
[ $rc -eq 0 ]; check "multiplos espacos nao quebram (rc=$rc)" $?

echo
echo "=== MODO WORKFLOW ==="

cat > /tmp/wf1.pf << 'EOF'
task listar /bin/ls -l
exit
EOF

out=$(timeout 5 $BIN /tmp/wf1.pf); rc=$?
[ $rc -eq 0 ]; check "roda arquivo .pf e sai (rc=$rc)" $?

echo "$out" | grep -q "task listar /bin/ls -l"; check "imprime a linha lida antes de processar" $?

echo "$out" | grep -q "processflow>"; ! echo "$out" | grep -q "processflow>"
check "NAO mostra prompt no modo workflow" $?

cat > /tmp/wf2.pf << 'EOF'
task listar /bin/ls
EOF
timeout 5 $BIN /tmp/wf2.pf > /dev/null 2>&1; rc=$?
[ $rc -eq 0 ]; check "arquivo .pf sem 'exit' encerra no EOF (rc=$rc)" $?

echo
echo "=== ERROS DE INICIALIZACAO ==="

out=$(timeout 5 $BIN /tmp/nao_existe_mesmo.pf 2>&1); rc=$?
[ $rc -ne 0 ]; check "arquivo inexistente encerra com codigo != 0 (rc=$rc)" $?
[ -n "$out" ]; check "arquivo inexistente imprime mensagem" $?

out=$(timeout 5 $BIN a b c 2>&1); rc=$?
[ $rc -ne 0 ]; check "argumentos demais encerra com codigo != 0 (rc=$rc)" $?
[ -n "$out" ]; check "argumentos demais imprime mensagem" $?

echo
echo "=== COMANDOS DA ESPECIFICACAO ==="

out=$(printf "task eco /bin/echo um dois tres\nrun eco\nexit\n" | timeout 5 $BIN 2>/dev/null)
echo "$out" | grep -q "um dois tres"
check "run executa e entrega TODOS os argumentos (pega off-by-one no argv)" $?

out=$(printf "task lst /bin/ls -l\nrun lst\nexit\n" | timeout 5 $BIN 2>/dev/null)
echo "$out" | grep -qE "^[d-][rwx-]{9}"
check "run passa a opcao -l (linhas com bits de permissao)" $?

ini=$(date +%s%N)
printf "task slp /bin/sleep 1\nrun slp\nexit\n" | timeout 10 $BIN >/dev/null 2>&1
ms=$(( ($(date +%s%N) - ini) / 1000000 ))
[ "$ms" -ge 900 ]; check "run espera o filho terminar (levou ${ms}ms, esperado >=900)" $?

out=$(printf "task quebrada /bin/nao_existe_mesmo\nrun quebrada\nzzz\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "no such file|nao existe|erro"
check "exec que falha imprime mensagem de erro" $?

n=$(echo "$out" | grep -o "desconhec" | wc -l)
[ "$n" -eq 1 ]
check "exec que falha NAO cria um segundo processflow (msg ${n}x, esperado 1)" $?

out=$(printf "task ruim /bin/nao_existe_mesmo\nrun ruim\ntask ok /bin/echo vivo\nrun ok\nexit\n" | timeout 5 $BIN 2>/dev/null)
echo "$out" | grep -q "vivo"
check "exec que falha NAO encerra o processflow (comando seguinte roda)" $?

out=$(printf "run nao_cadastrada\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "erro|nao existe|não existe|inexist"
check "'run' de tarefa inexistente imprime erro" $?

out=$(printf "xyzzy\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "erro|desconhec"
check "comando desconhecido imprime erro" $?

echo
echo "=== ROBUSTEZ ==="

# uma unica linha longa + exit: o prompt deve aparecer 2x, nao 3x
long=$(python3 -c "print('task ' + 'a'*400)")
n=$(printf "%s\nexit\n" "$long" | timeout 5 $BIN 2>&1 | grep -o "processflow>" | wc -l)
[ "$n" -le 2 ]; check "linha > 299 chars nao vira dois comandos (prompt apareceu ${n}x, esperado 2)" $?

echo
echo "=== FASE 4: SEQUENCIAL X PARALELO ==="
# O criterio aqui e TEMPO, nao mensagem. Tres tarefas de sleep 3s, 1s e 2s:
#   sequencial -> uma espera a outra -> ~6s     paralelo -> todas juntas -> ~3s
# Paralelo em ~6s = a espera esta dentro do laco de criacao.

ms=$(cronometra $TESTDIR/sequencial.pf)
[ "$ms" -ge 5500 ] && [ "$ms" -le 8000 ]
check "sequencial espera cada tarefa (3+1+2 = ~6s, medido ${ms}ms)" $?

mp=$(cronometra $TESTDIR/paralelo.pf)
[ "$mp" -ge 2500 ] && [ "$mp" -le 4500 ]
check "paralelo roda as tres juntas (max(3,1,2) = ~3s, medido ${mp}ms)" $?

[ "$mp" -lt "$ms" ]
check "paralelo e mais rapido que sequencial (${mp}ms < ${ms}ms)" $?

# dorme1 existe, fantasma nao, dorme1 de novo: deve rodar as duas que existem
ini=$(date +%s%N)
out=$(printf "task d1 /bin/sleep 1\nrun sequential d1 fantasma d1\nexit\n" | timeout 20 $BIN 2>&1)
mi=$(( ($(date +%s%N) - ini) / 1000000 ))
echo "$out" | grep -qiE "fantasma"
check "avisa que a tarefa inexistente nao existe" $?
[ "$mi" -ge 1800 ] && [ "$mi" -le 3500 ]
check "pula a inexistente e roda as outras duas (~2s, medido ${mi}ms)" $?

out=$(printf "run sequential\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "erro|uso"
check "'run sequential' sem nenhum nome imprime erro" $?

out=$(printf "task d1 /bin/sleep 1\nrun sequential d1\nexit\n" | timeout 10 $BIN 2>&1)
echo "$out" | grep -qiE "erro|uso|mais de uma"
check "'run sequential' com UM nome so imprime erro (enunciado: mais de uma)" $?

out=$(printf "run parallel\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "erro|uso"
check "'run parallel' sem nenhum nome imprime erro" $?

out=$(printf "run sequential nada1 nada2\ntask viv /bin/echo vivo\nrun viv\nexit\n" | timeout 10 $BIN 2>/dev/null)
echo "$out" | grep -q "vivo"
check "lista toda inexistente nao derruba o processflow" $?

echo
echo "=== CODIGOS DE SAIDA (Fase 10) ==="
# "2>&1 >/dev/null" captura SO o stderr: a saida do programa vai pro lixo.

out=$(printf "task f /bin/false\nrun f\nexit\n" | timeout 5 $BIN 2>&1 >/dev/null)
echo "$out" | grep -q "1"
check "reporta codigo de saida diferente de zero" $?

out=$(printf "task ok /bin/echo tudo_bem\nrun ok\nexit\n" | timeout 5 $BIN 2>&1 >/dev/null)
[ -z "$out" ]
check "NAO reporta nada quando o codigo e zero (sem ruido)" $?

# a mensagem nao pode cair no stdout: na Fase 5 isso sujaria o arquivo de saida
out=$(printf "task f /bin/false\nrun f\nexit\n" | timeout 5 $BIN 2>/dev/null)
! echo "$out" | grep -qiE "codigo|código|sinal"
check "mensagem de status sai no stderr, nao no stdout" $?

if [ -x /usr/bin/timeout ]; then
    out=$(printf "task t /usr/bin/timeout 1 /bin/sleep 5\nrun t\nexit\n" | timeout 10 $BIN 2>&1 >/dev/null)
    echo "$out" | grep -q "124"
    check "reporta o codigo exato do programa (timeout -> 124)" $?
fi

out=$(printf "task f /bin/false\ntask ok /bin/echo x\nrun sequential f ok\nexit\n" | timeout 5 $BIN 2>&1 >/dev/null)
n=$(echo "$out" | grep -c ":")
[ "$n" -eq 1 ]
check "sequencial reporta so a tarefa que falhou (${n} linha(s), esperado 1)" $?

out=$(printf "task f /bin/false\ntask ok /bin/echo x\nrun parallel f ok\nexit\n" | timeout 5 $BIN 2>&1 >/dev/null)
n=$(echo "$out" | grep -c ":")
[ "$n" -eq 1 ]
check "paralelo reporta so a tarefa que falhou (${n} linha(s), esperado 1)" $?

echo
echo "=== FASE 5: REDIRECIONAMENTO ==="
# Aqui a massa de entrada e $TESTDIR/nomes.txt (versionada). As saidas vao pro
# /tmp, que e descartavel: teste que suja o repo nao roda duas vezes em paz.

F5=/tmp/pf_f5
rm -rf $F5; mkdir -p $F5
SORT=$(command -v sort)

# --- ACEITACAO DA FASE, metade 1: input + output com uma tarefa de ordenacao
cat > $F5/ordena.pf << EOF
task ordenar $SORT
input ordenar $TESTDIR/nomes.txt
output ordenar $F5/resultado.txt
run ordenar
exit
EOF
timeout 10 $BIN $F5/ordena.pf >/dev/null 2>&1

[ -f $F5/resultado.txt ]; check "ACEITACAO F5 (1/2): output criou o arquivo" $?

$SORT $TESTDIR/nomes.txt | diff -q - $F5/resultado.txt >/dev/null 2>&1
check "ACEITACAO F5 (1/2): conteudo ordenado bate com o esperado" $?

# o programa leu do arquivo, nao do stdin herdado: se o input nao tivesse
# colado, o sort leria do terminal e o arquivo sairia vazio
[ -s $F5/resultado.txt ]; check "input alimentou mesmo o stdin do filho (arquivo nao vazio)" $?

# nada do redirecionamento pode vazar pro stdout do processflow
out=$(timeout 10 $BIN $F5/ordena.pf 2>/dev/null)
! echo "$out" | grep -q "bruno"
check "a saida da tarefa foi pro arquivo, nao pro stdout do processflow" $?

# --- output trunca: rodar duas vezes deixa uma linha so
cat > $F5/trunca.pf << EOF
task eco /bin/echo
output eco $F5/trunca.txt
run eco
run eco
exit
EOF
timeout 10 $BIN $F5/trunca.pf >/dev/null 2>&1
n=$(wc -l < $F5/trunca.txt)
[ "$n" -eq 1 ]
check "output trunca a cada execucao (${n} linha(s), esperado 1)" $?

# --- ACEITACAO DA FASE, metade 2: append duas vezes nao trunca
rm -f $F5/historico.txt
cat > $F5/ap1.pf << EOF
task eco /bin/echo primeira
append eco $F5/historico.txt
run eco
exit
EOF
cat > $F5/ap2.pf << EOF
task eco /bin/echo segunda
append eco $F5/historico.txt
run eco
exit
EOF
timeout 10 $BIN $F5/ap1.pf >/dev/null 2>&1
timeout 10 $BIN $F5/ap2.pf >/dev/null 2>&1
n=$(wc -l < $F5/historico.txt 2>/dev/null || echo 0)
[ "$n" -eq 2 ]
check "ACEITACAO F5 (2/2): dois 'append' somam, nao truncam (${n} linha(s), esperado 2)" $?

[ "$(head -1 $F5/historico.txt 2>/dev/null)" = "primeira" ]
check "ACEITACAO F5 (2/2): a primeira execucao sobreviveu a segunda" $?

# --- append num arquivo que ja existia preserva o conteudo anterior
printf 'de antes\n' > $F5/pre.txt
cat > $F5/pre.pf << EOF
task eco /bin/echo de_agora
append eco $F5/pre.txt
run eco
exit
EOF
timeout 10 $BIN $F5/pre.pf >/dev/null 2>&1
grep -q "de antes" $F5/pre.txt
check "append preserva o que ja estava no arquivo" $?

# --- os tres comandos so anexam propriedade: nao executam nada
out=$(printf "task eco /bin/echo naoDeviaSair\noutput eco $F5/nada.txt\nexit\n" | timeout 5 $BIN 2>/dev/null)
! echo "$out" | grep -q "naoDeviaSair" && [ ! -f $F5/nada.txt ]
check "'output' sozinho nao executa a tarefa nem cria o arquivo" $?

# --- o redirecionamento viaja com a tarefa ate o sequencial e o paralelo
cat > $F5/seq.pf << EOF
task a /bin/echo tarefa_a
task b /bin/echo tarefa_b
output a $F5/sa.txt
output b $F5/sb.txt
run sequential a b
exit
EOF
timeout 10 $BIN $F5/seq.pf >/dev/null 2>&1
grep -q "tarefa_a" $F5/sa.txt 2>/dev/null && grep -q "tarefa_b" $F5/sb.txt 2>/dev/null
check "redirecionamento vale tambem no 'run sequential'" $?

cat > $F5/par.pf << EOF
task a /bin/echo paralela_a
task b /bin/echo paralela_b
output a $F5/pa.txt
output b $F5/pb.txt
run parallel a b
exit
EOF
timeout 10 $BIN $F5/par.pf >/dev/null 2>&1
grep -q "paralela_a" $F5/pa.txt 2>/dev/null && grep -q "paralela_b" $F5/pb.txt 2>/dev/null
check "redirecionamento vale tambem no 'run parallel'" $?

# --- redirecionar uma tarefa nao contamina a outra
# Modo interativo de proposito: o modo workflow ecoa cada linha lida, e o eco
# do proprio "task com /bin/echo redirecionada" apareceria no stdout, dando
# falso negativo. Aqui o unico jeito da palavra sair e a tarefa te-la escrito.
out=$(printf "task com /bin/echo redirecionada\ntask sem /bin/echo no_stdout\noutput com $F5/com.txt\nrun com\nrun sem\nexit\n" | timeout 10 $BIN 2>/dev/null)
echo "$out" | grep -q "no_stdout"
check "tarefa sem redirecionamento continua escrevendo no stdout" $?
! echo "$out" | grep -q "redirecionada"
check "tarefa com redirecionamento nao escreve no stdout do processflow" $?
grep -q "redirecionada" $F5/com.txt 2>/dev/null
check "o texto da tarefa redirecionada foi parar no arquivo" $?

echo
echo "=== FASE 5: ERROS DE REDIRECIONAMENTO (Fase 10) ==="

# --- arquivo de entrada que nao abre: erro que NAO encerra
out=$(printf "task ler /bin/cat\ninput ler $F5/nao_existe.txt\nrun ler\ntask viv /bin/echo VIVO\nrun viv\nexit\n" | timeout 5 $BIN 2>&1); rc=$?
[ $rc -eq 0 ]; check "entrada inexistente NAO encerra o processflow (rc=$rc)" $?
echo "$out" | grep -q "VIVO"; check "entrada inexistente: o comando seguinte roda" $?
echo "$out" | grep -qiE "no such file|nao existe|não existe|erro"
check "entrada inexistente imprime mensagem de erro" $?

# a mensagem tem que sair no stderr, senao suja o arquivo de saida
out=$(printf "task ler /bin/cat\ninput ler $F5/nao_existe.txt\nrun ler\nexit\n" | timeout 5 $BIN 2>/dev/null)
! echo "$out" | grep -qiE "no such file|nao existe|não existe"
check "erro de entrada sai no stderr, nao no stdout" $?

# --- arquivo de saida que nao abre: erro que NAO encerra
out=$(printf "task eco /bin/echo x\noutput eco /nao/existe/esse/caminho/s.txt\nrun eco\ntask viv /bin/echo VIVO\nrun viv\nexit\n" | timeout 5 $BIN 2>&1); rc=$?
[ $rc -eq 0 ]; check "saida impossivel NAO encerra o processflow (rc=$rc)" $?
echo "$out" | grep -q "VIVO"; check "saida impossivel: o comando seguinte roda" $?
echo "$out" | grep -qiE "no such file|nao existe|não existe|erro"
check "saida impossivel imprime mensagem de erro" $?

# --- redirecionar tarefa que nao existe
for cmd in input output append; do
    out=$(printf "$cmd fantasma /tmp/x.txt\ntask viv /bin/echo VIVO\nrun viv\nexit\n" | timeout 5 $BIN 2>&1)
    echo "$out" | grep -qiE "erro|nao encontrada|não encontrada|nao existe" && echo "$out" | grep -q "VIVO"
    check "'$cmd' em tarefa inexistente: erro que nao encerra" $?
done

# --- numero errado de argumentos
for cmd in input output append; do
    out=$(printf "task eco /bin/echo x\n$cmd eco\nexit\n" | timeout 5 $BIN 2>&1)
    echo "$out" | grep -qiE "erro|uso"
    check "'$cmd' com argumentos de menos imprime erro" $?
done

out=$(printf "task eco /bin/echo x\ninput eco a.txt b.txt\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "erro|uso"
check "'input' com argumentos de mais imprime erro" $?

# --- a mensagem do output/append nao pode falar em 'entrada' (copy-paste)
out=$(printf "output fantasma /tmp/x.txt\nexit\n" | timeout 5 $BIN 2>&1)
! echo "$out" | grep -qi "entrada"
check "a mensagem de erro do 'output' nao fala em 'entrada'" $?


echo
echo "----------------------------------------"
echo "$((ok+falhou)) testes, $falhou falha(s)"
echo
[ $falhou -eq 0 ]
