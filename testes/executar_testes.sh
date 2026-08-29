#!/bin/sh
# ==========================================================================
# executar_testes.sh - bateria de testes do pre-processador (macOS / Linux)
#
# Uso:  sh testes/executar_testes.sh      (a partir da pasta do projeto)
#
# Para cada arquivo testes/NN_nome.asm o script:
#   1. executa  ./main.exe NN_nome.asm  testes/saida/NN_nome.pre
#   2. compara o resultado com testes/NN_nome.esperado
#   3. imprime OK ou FALHOU (mostrando as diferencas)
# No final testa tambem os casos de erro (argumentos, arquivo inexistente).
# ==========================================================================

# Vai para a pasta do projeto (a pasta acima deste script).
cd "$(dirname "$0")/.." || exit 1

PASTA_TESTES="testes"
PASTA_SAIDA="$PASTA_TESTES/saida"
PROGRAMA="./main.exe"

total=0
passou=0

echo ""
echo "=========================================="
echo " Compilando"
echo "=========================================="
gcc -Wall -Wextra -std=c99 main.c preprocessador.c -o main.exe || {
    echo "ERRO DE COMPILACAO"
    exit 1
}
echo "Compilado: $PROGRAMA"

mkdir -p "$PASTA_SAIDA"

echo ""
echo "=========================================="
echo " Casos de pre-processamento"
echo "=========================================="

for entrada in "$PASTA_TESTES"/*.asm; do
    nome=$(basename "$entrada" .asm)
    esperado="$PASTA_TESTES/$nome.esperado"
    obtido="$PASTA_SAIDA/$nome.pre"

    total=$((total + 1))

    # 2>/dev/null esconde os avisos; o que importa aqui e o arquivo gerado.
    "$PROGRAMA" "$entrada" "$obtido" > /dev/null 2>/dev/null

    if [ ! -f "$obtido" ]; then
        echo "FALHOU  $nome  (arquivo de saida nao foi gerado)"
        continue
    fi

    if diff -u "$esperado" "$obtido" > /dev/null 2>&1; then
        echo "OK      $nome"
        passou=$((passou + 1))
    else
        echo "FALHOU  $nome"
        echo "        --- esperado / +++ obtido ---"
        diff -u "$esperado" "$obtido" | sed 's/^/        /'
    fi
done

echo ""
echo "=========================================="
echo " Casos de erro (devem terminar com falha)"
echo "=========================================="

verificar_erro() {
    descricao="$1"
    shift
    total=$((total + 1))
    if "$PROGRAMA" "$@" > /dev/null 2>&1; then
        echo "FALHOU  $descricao (o programa deveria ter retornado erro)"
    else
        echo "OK      $descricao"
        passou=$((passou + 1))
    fi
}

verificar_erro "sem argumentos"
verificar_erro "apenas um argumento"          "$PASTA_TESTES/05_rotulos.asm"
verificar_erro "argumentos demais"            a.asm b.pre c.txt
verificar_erro "arquivo de entrada inexistente" nao_existe.asm "$PASTA_SAIDA/x.pre"
verificar_erro "entrada igual a saida"        "$PASTA_TESTES/05_rotulos.asm" "$PASTA_TESTES/05_rotulos.asm"

echo ""
echo "=========================================="
echo " Resultado: $passou de $total testes passaram"
echo "=========================================="
echo ""

[ "$passou" -eq "$total" ]
