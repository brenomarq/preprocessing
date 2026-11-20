# Como funciona o analisador sintático

Este texto explica o código de `sintatico.c` e `sintatico.h` e como ele se
encaixa no resto do projeto. Leia o [README.md](../README.md) primeiro.

## 1. O caminho de um arquivo pelo programa

O `main` executa as três etapas em ordem:

1. `PreProcessamento(asm, pre)` normaliza o código-fonte.
2. `AnaliseLexica(pre, lex)` grava um token por linha no `.lex`.
3. Se não houve erro léxico, `AnaliseSintatica(lex, dot, err)` verifica a
   gramática e grava a árvore e os diagnósticos.

A etapa 3 tem uma particularidade: ela não recebe os tokens em memória. O
enunciado fixou a assinatura `int AnaliseSintatica(FILE *tokens, FILE *arvore,
FILE *erros)`, em que `tokens` é o arquivo `.lex` aberto para leitura. Ou seja,
o analisador sintático relê o texto que o léxico escreveu.

Por isso o `main` fecha o `.lex` depois de gravá-lo e o abre de novo para ler:
o enunciado exige que a entrada de cada etapa esteja posicionada no início.

A função devolve 0 para programa válido, 1 para inválido e −1 para falha
operacional, e é esse retorno que define o código de encerramento do programa.

## 2. Reconstruir os tokens a partir do `.lex`

Cada linha do `.lex` tem o formato

```
<NOME, lexema> linha coluna
```

Separar isso parece simples, mas tem um problema: o lexema de uma `STRING`
pode ter vírgula, espaço e até o próprio `>` dentro. Uma linha válida é

```
<STRING, "a, b> c 3 5"> 2 12
```

Um `split` por vírgula ou uma busca pelo primeiro `>` quebram nesse caso. A
função `decompor()` resolve lendo **da direita para a esquerda**: primeiro o
número da coluna, depois o da linha, depois o `>` que fecha. O que sobra à
esquerda é `<NOME, lexema`, e aí sim a primeira vírgula separa os dois, porque
o nome da categoria nunca contém vírgula.

## 3. O marcador EOL

A gramática usa `EOL` para marcar o fim de cada linha lógica, mas esse token
**não existe no `.lex`**. Ele é deduzido em `carregar()`, comparando o número
da linha de tokens consecutivos: quando muda, entra um `EOL` antes do token
novo.

Além da mudança de linha, há duas regras:

- Entra sempre um `EOL` antes do `TK_EOF`, para que a última linha termine com
  o marcador mesmo quando o arquivo não tiver quebra de linha no fim.
- Linhas sem token nenhum não geram marcador. Isso sai de graça, porque linhas
  vazias e linhas só com comentário nem chegam ao `.lex`: o pré-processamento
  já as removeu.

O `EOL` também carrega uma posição própria: a coluna logo depois do último
lexema da linha. É ela que aparece quando falta um operando no fim da linha:

```
li $t0,
```

produz `Linha 3, coluna 8: Encontrado fim de linha; esperado NUM_INT.`

Se o `.lex` não tiver `TK_EOF`, o módulo acrescenta um. Se já tiver, não
duplica.

## 4. A análise descendente recursiva

Cada não terminal da gramática vira uma função. Ela olha o próximo token
para decidir qual alternativa da produção usar, chama as funções dos não
terminais que aparecem ali e consome os terminais.

Por exemplo, a produção

```
<instrucao_li> ::= "li" REG "," NUM_INT
```

vira

```c
static No *instrucao_li(void) {
    No *n = novo_no("<instrucao_li>", "", 0);

    if (!consumir(n, "INS_LI", "INS_LI(\"li\")")
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "NUM_INT", "NUM_INT")) {
        return NULL;
    }
    return n;
}
```

Toda função devolve o nó que construiu, ou `NULL` quando encontra um erro. A
produção correspondente está no comentário acima de cada uma, para dar para
conferir uma contra a outra.

São 32 funções, uma para cada não terminal:

| Grupo | Funções |
|---|---|
| Organização | `programa`, `secao`, `secao_dados`, `secao_codigo`, `linha_dados`, `linha_codigo`, `rotulo`, `diretiva_global` |
| Dados | `diretiva_dados`, `diretiva_inteiros`, `lista_inteiros`, `diretiva_string` |
| Instruções | `instrucao` e os 11 grupos, com os `op_*` correspondentes |
| Endereçamento | `endereco` |

As repetições da EBNF viram laços. Em `<lista_inteiros> ::= NUM_INT { ","
NUM_INT }`, o `{ }` é um `while (eh("SMB_COM"))`. As partes opcionais viram
`if`: em `<linha_codigo> ::= <rotulo> [ <instrucao> ] EOL`, depois do rótulo a
função pergunta se o próximo token começa uma instrução antes de chamá-la.

### Como a decisão é tomada

Três funções auxiliares concentram as decisões:

- `eh("REG")` diz se o token atual é de uma categoria;
- `inicia_diretiva_dados()` e `inicia_instrucao()` dizem se o token atual pode
  começar uma diretiva de dados ou uma instrução;
- `pertence(OP_TRES, ...)` diz se o token está em um grupo de instruções.

Os grupos são vetores declarados no começo do arquivo, um para cada `<op_*>`
da gramática. Para acrescentar uma instrução ao subconjunto, basta incluí-la
no vetor do grupo certo.

### Um ponto fácil de errar

A repetição `{ <linha_dados> }` só termina quando aparece `.data`, `.text` ou
o fim do arquivo. Uma instrução dentro da seção `.data` **não** encerra a
seção: ela é um erro. O enunciado é explícito nesse ponto, e tratar como fim
de seção faria o analisador aceitar um programa inválido.

## 5. Diagnósticos e recuperação

Quando o token encontrado não é o esperado, `erro()` grava duas linhas no
`.sin.err`:

```
Linha 4, coluna 9:
Encontrado REG("$t1"); esperado SMB_COM(",").
```

O texto de "esperado" vem de quem chamou. Para os terminais mais comuns a
função `descricao()` monta a forma `SMB_COM(",")` a partir da categoria, para
não repetir essas strings em dezenas de lugares. Quando o esperado é uma
construção inteira, e não um token, o texto é descritivo: `esperado rótulo,
instrução ou DIR_GLOBL(".globl")`.

O token encontrado tem três formas: `CATEGORIA("lexema")` no caso comum,
`fim de linha` quando é o `EOL` e `TK_EOF` no fim do arquivo.

### A recuperação

Depois de registrar o erro, `recuperar()` descarta o resto da linha e retoma na
linha seguinte:

- se o token atual já for o `EOL`, consome só ele;
- se for o `TK_EOF`, não faz nada, porque não há mais linhas;
- nos outros casos, avança até o `EOL` e o consome.

Fora do fim do arquivo a posição sempre avança pelo menos um token, senão a
análise entraria em laço repetindo o mesmo diagnóstico. Tokens da linha
seguinte nunca são descartados, então um erro na linha 3 não esconde um erro
na linha 5.

A seção ativa é preservada: a recuperação acontece dentro do laço de
`secao_dados()` ou `secao_codigo()`, que continua rodando.

## 6. A árvore sintática concreta

Cada nó guarda o rótulo que vai para o `.dot`:

```c
typedef struct No {
    char nome[TAM_NOME];      /* "<programa>" ou "REG" */
    char lexema[TAM_LEXEMA];  /* só nos terminais que têm lexema */
    int terminal;
    int id;
    struct No **filhos;
    int total;
    int capacidade;
} No;
```

Os filhos ficam em um vetor que dobra de tamanho quando enche, porque o número
varia muito: `<rotulo>` tem dois filhos e `<secao_codigo>` tem um por linha do
programa.

Todo nó criado também entra em um vetor único, o `criados`, o que deixa a
gente liberar tudo de uma vez no fim, sem percorrer a árvore. Isso importa
porque, quando há erro, sobram pedaços de árvore sem pai.

### A gravação

São duas passagens. A primeira, `declarar()`, numera os nós em pré-ordem e
escreve as declarações; a segunda, `ligar()`, escreve as arestas. Numerar em
pré-ordem garante identificadores exclusivos e deixa o arquivo na mesma ordem
do exemplo do enunciado.

Os rótulos passam por `escapar()`, que protege `"` e `\`. Sem isso, uma string
como `.asciiz "ele disse \"ola\""` quebraria o arquivo DOT.

### Programa inválido não tem árvore

O `.dot` só é escrito quando `total_erros == 0`. Para programa inválido o
arquivo é criado e fica **vazio**, como o enunciado exige, e nunca com uma
árvore pela metade. A árvore continua sendo construída durante a análise, mas é
simplesmente descartada.

## 7. Testando uma parte isolada

Como `AnaliseSintatica()` recebe `FILE *`, dá para testá-la sem rodar as
etapas anteriores, escrevendo um `.lex` na mão:

```c
#include <stdio.h>
#include "sintatico.h"

int main(void) {
    FILE *tok = fopen("meu.lex", "rb");
    FILE *dot = fopen("saida.dot", "wb");
    FILE *err = fopen("saida.err", "wb");

    printf("retorno: %d\n", AnaliseSintatica(tok, dot, err));
    fclose(tok); fclose(dot); fclose(err);
    return 0;
}
```

Isso é útil para testar o `EOL` e o `TK_EOF` em situações que o léxico nunca
produz, como um `.lex` sem token de fim de arquivo.

## 8. Coisas que já estão resolvidas no código

1. **Separar a linha do `.lex` pelo primeiro `>` ou por vírgula.** Quebra em
   qualquer programa que tenha uma string com esses caracteres.
2. **Esquecer o `EOL` da última linha** quando o arquivo não termina em quebra
   de linha. A última construção do programa fica sem terminador e vira erro.
3. **Tratar construção proibida como fim de seção.** Faz o analisador aceitar
   instrução dentro de `.data`.
4. **Recuperação que não avança.** Se `recuperar()` puder sair sem consumir
   nada, o mesmo erro é registrado para sempre.
5. **Gravar árvore parcial de programa inválido.** O enunciado pede o `.dot`
   vazio nesse caso.
6. **Esquecer de escapar os rótulos do DOT.** Um programa com strings gera um
   arquivo que o Graphviz recusa.

## 9. Ligação com a próxima etapa

A árvore sintática é a entrada natural da análise semântica, que vai percorrer
os mesmos nós para montar a tabela de símbolos definitiva, conferir se os
rótulos referenciados existem, se algum foi declarado duas vezes e se os
valores numéricos cabem nos operandos. É tudo o que esta etapa de propósito
não verifica.
