#!/bin/bash
# run_tests.sh — testes de ponta a ponta do binario ./processflow
# Cada teste roda o programa de verdade e confere saida e codigo de retorno.

BIN=./processflow
ok=0; falhou=0

check() {   # check "nome" "condicao_ja_avaliada"  -> usa $? do caller
    if [ "$2" = "0" ]; then
        printf "  ok     %s\n" "$1"; ok=$((ok+1))
    else
        printf "  FALHA  %s\n" "$1"; falhou=$((falhou+1))
    fi
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

out=$(printf "task listar /bin/ls -l\nrun listar\nexit\n" | timeout 5 $BIN 2>&1)
echo "$out" | grep -qiE "erro|nao|não|invalid|desconhec"
check "comando 'run' de tarefa cadastrada faz algo (ou erro coerente)" $?

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
echo "----------------------------------------"
echo "$((ok+falhou)) testes, $falhou falha(s)"
echo
[ $falhou -eq 0 ]
