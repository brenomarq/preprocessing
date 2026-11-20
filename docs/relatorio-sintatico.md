# Relatório técnico: analisador sintático μ-Assembly

Universidade Católica de Brasília  
Linguagens Formais, Autômatos e Compiladores - Professor Marcelo Eustáquio

Grupo: Breno Marques, Thiago de Jesus, Roberty Luann e Arthur

## 1. Visão geral

O programa recebe um arquivo `.asm` escrito em μ-Assembly e executa três
etapas em sequência:

1. **Pré-processamento**: normaliza o código-fonte removendo comentários,
   linhas vazias e espaços desnecessários, preservando o conteúdo das cadeias
   de caracteres.
2. **Análise léxica**: lê o código normalizado, agrupa os caracteres em
   lexemas, classifica cada um e monta a tabela de símbolos.
3. **Análise sintática**: relê os tokens gravados pela etapa anterior,
   verifica a gramática por descida recursiva, registra os erros encontrados e
   grava a árvore sintática concreta.

A terceira etapa só é executada quando a segunda não encontra nenhum erro
léxico.

O código está dividido em sete arquivos:

| Arquivo                   | Conteúdo                                                           |
| ------------------------- | ------------------------------------------------------------------ |
| `main.c`                  | Validação dos argumentos, abertura dos arquivos e ordem das etapas |
| `preprocessador.c` / `.h` | Etapa 1                                                            |
| `lexico.c` / `.h`         | Etapa 2                                                            |
| `sintatico.c` / `.h`      | Etapa 3                                                            |

A interface é a exigida no enunciado:

```
./main.exe <arquivo_entrada.asm> <prefixo_saida>
```

De um único prefixo saem seis arquivos. Com o prefixo `resultado`:

| Arquivo             | Conteúdo                                       |
| ------------------- | ---------------------------------------------- |
| `resultado.pre`     | Código resultante do pré-processamento         |
| `resultado.lex`     | Tokens produzidos pelo analisador léxico       |
| `resultado.ts`      | Tabela de símbolos da etapa léxica             |
| `resultado.lex.err` | Diagnósticos léxicos                           |
| `resultado.sin.err` | Diagnósticos sintáticos                        |
| `resultado.dot`     | Árvore sintática concreta de programa válido   |

Todas as saídas são abertas em modo `"wb"`, que trunca o arquivo. Assim, o
resultado de uma execução anterior nunca sobrevive para ser apresentado como
atual. Antes de abrir qualquer uma delas, `main()` confere se o nome montado
coincide com o do arquivo de entrada e recusa a execução nesse caso, para não
apagar o código-fonte do usuário.

Os códigos de encerramento são os três previstos:

| Código | Situação                                                       |
| ------ | -------------------------------------------------------------- |
| 0      | Execução concluída e programa sintaticamente válido            |
| 1      | Rejeição por erros léxicos ou sintáticos                       |
| 2      | Falha operacional ou uso incorreto da linha de comando         |

### A integração entre as etapas

A assinatura da etapa 3 foi fixada pelo enunciado:

```c
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);
```

O parâmetro `tokens` é o arquivo `.lex` aberto para leitura. O analisador
sintático, portanto, não recebe os tokens em memória: ele relê o texto que o
léxico gravou. Como o enunciado exige que a entrada de cada etapa esteja
posicionada no início, `main()` fecha o `.lex` depois de gravá-lo e o abre
novamente para leitura, em vez de reaproveitar o descritor com um `rewind()`.

A função devolve `0` para programa válido, `1` para inválido e `-1` para falha
operacional, e é esse retorno que determina o código de encerramento.

A decisão de executar ou não a etapa 3 vem de `executar_analise_lexica()`, que
devolve a quantidade de erros léxicos encontrados (ou `-1` se alguma operação
de arquivo falhar). Quando esse número é maior que zero, a análise sintática
não roda, mas os dois arquivos dela ainda são criados: o `.dot` fica vazio e o
`.sin.err` recebe o motivo.

```
Análise sintática não executada: existem erros léxicos.
```

No terminal aparece `ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS.`
e o programa encerra com código 1.

## 2. Estruturas de dados

### Token

A etapa 3 reaproveita a estrutura definida na etapa anterior, incluindo
`lexico.h` apenas para isso. Não houve necessidade de alterar o analisador
léxico: a gramática desta etapa usa exatamente as 10 diretivas e as 36
instruções que ele já reconhece.

```c
typedef struct {
    char nome[TAM_NOME];      /* categoria: "INS_ADD", "REG", "ID"... */
    char lexema[TAM_LEXEMA];
    int  linha;
    int  coluna;
} Token;
```

### O fluxo de tokens

Os tokens são carregados de uma vez em um vetor dinâmico, que é a entrada do
analisador:

```c
static Token *fluxo = NULL;
static int total = 0;        /* tokens carregados */
static int capacidade = 0;   /* espaço alocado */
static int atual = 0;        /* posição de leitura */
```

O vetor começa com 256 posições e dobra de tamanho por `realloc()` quando
enche, então não existe limite fixo para o tamanho do programa. Carregar
tudo antes de analisar simplifica a decisão de qual produção aplicar: basta
olhar `fluxo[atual]`, sem precisar devolver tokens ao arquivo.

### No, o nó da árvore

```c
typedef struct No {
    char nome[TAM_NOME];      /* "<programa>" ou "REG" */
    char lexema[TAM_LEXEMA];  /* só nos terminais que têm lexema */
    int terminal;             /* 1 para folha, 0 para não terminal */
    int id;                   /* número do nó no arquivo DOT */
    struct No **filhos;
    int total;
    int capacidade;
} No;
```

O vetor de filhos também dobra de tamanho quando enche, porque a quantidade
varia muito de um nó para outro: `<rotulo>` tem sempre dois filhos, enquanto
`<secao_codigo>` tem um filho por linha do programa.

### A arena de nós

Todo nó criado é registrado em um vetor único:

```c
static No **criados = NULL;
static int qtd_criados = 0;
static int cap_criados = 0;
```

Assim dá para liberar tudo de uma vez no fim, sem percorrer a árvore e sem
precisar definir quem é dono de quem. Isso importa porque, quando há erro, as
funções devolvem `NULL` e sobram pedaços de árvore sem pai, que uma liberação
recursiva a partir da raiz não alcançaria.

### Os grupos de instruções

Cada não terminal `<op_*>` da gramática corresponde a um vetor de categorias:

```c
static const char *OP_TRES[] = {"INS_ADD", "INS_ADDU", "INS_SUB", "INS_SUBU",
                                "INS_MUL", "INS_AND", "INS_OR", "INS_XOR",
                                "INS_NOR", "INS_SLT"};
static const char *OP_DOIS[]  = {"INS_MOVE", "INS_DIV"};
static const char *OP_UM[]    = {"INS_MFLO", "INS_MFHI", "INS_JR"};
static const char *OP_DESL[]  = {"INS_SLL", "INS_SRL"};
static const char *OP_MEM[]   = {"INS_LW", "INS_SW", "INS_LB", "INS_SB",
                                 "INS_LH", "INS_SH"};
static const char *OP_DESV[]  = {"INS_BEQ", "INS_BNE", "INS_BLT", "INS_BLE",
                                 "INS_BGT", "INS_BGE"};
static const char *OP_SALTO[] = {"INS_J", "INS_JAL"};
static const char *OP_SEM[]   = {"INS_SYSCALL", "INS_NOP"};
```

Deixar os grupos em tabelas, e não em cadeias de `if`, mantém o código das
produções curto e torna a correspondência com a gramática visível: para
acrescentar uma instrução ao subconjunto basta incluí-la no vetor do grupo
certo. As instruções `addi`, `li` e `la` não formam grupo, porque cada uma tem
uma produção própria.

### Estado do módulo

```c
static FILE *saida_erros = NULL;  /* arquivo .sin.err */
static int total_erros = 0;       /* erros sintáticos registrados */
static int falha = 0;             /* falha operacional: memória ou formato */
```

O campo `falha` separa as duas situações que o enunciado trata de formas
diferentes: um programa inválido é uma rejeição (código 1), enquanto um
`realloc()` que não conseguiu memória ou um `.lex` com formato inesperado é
uma falha operacional (código 2).

## 3. Reconstruir os tokens a partir do `.lex`

Cada linha do `.lex` tem o formato

```
<NOME, lexema> linha coluna
```

Separar isso parece simples, mas tem um problema: o lexema de uma `STRING`
pode ter vírgula, espaço e até o próprio `>` dentro. Uma linha válida é

```
<STRING, "a, b> c 3 5"> 2 12
```

Um `split` por vírgula, ou uma busca pelo primeiro `>`, extrai lexema e
posições erradas nesse caso. A função `decompor()` resolve lendo da direita
para a esquerda: primeiro o número da coluna, depois o da linha, depois o `>`
que fecha. O que sobra à esquerda é `<NOME, lexema`, e aí a primeira vírgula
separa os dois com segurança, porque o nome da categoria nunca contém vírgula.

A leitura usa um buffer de tamanho fixo, dimensionado a partir dos limites do
próprio analisador léxico (`TAM_LEXEMA + TAM_NOME + 64`, ou 214 bytes). Como um
lexema tem no máximo 99 caracteres e uma categoria no máximo 49, nenhuma linha
que o léxico seja capaz de gravar passa desse tamanho.

Verificação: alimentando o módulo com a linha acima, o lexema reconstruído é
exatamente `"a, b> c 3 5"`, na linha 2, coluna 12.

## 4. O tratamento de EOL e TK_EOF

### EOL

A gramática usa `EOL` para marcar o fim de cada linha lógica, mas esse token
não existe no `.lex`, e o enunciado determina que o formato desse arquivo não
seja alterado. O marcador é produzido internamente por `carregar()`, a
partir da localização dos tokens:

```c
if (tem_token && t.linha != linha_anterior) {
    if (!acrescentar(EOL, "", linha_anterior, coluna_fim)) return 0;
}
```

Ou seja: quando o número da linha do token lido é diferente do anterior, entra
um `EOL` antes dele. Há duas regras complementares:

- Entra sempre um `EOL` antes do `TK_EOF`, para que a última linha do arquivo
  termine com o marcador mesmo quando não houver quebra de linha final.
- Linhas sem token nenhum não geram marcador. Isso sai de graça: linhas vazias
  e linhas contendo apenas comentários não chegam ao `.lex`, porque o
  pré-processamento já as removeu.

O `EOL` carrega uma posição própria, que é a coluna imediatamente posterior ao
último lexema da linha:

```c
coluna_fim = t.coluna + (int) strlen(t.lexema);
```

É essa posição que aparece no diagnóstico quando falta um operando no fim da
linha. Para `li $t0,` na linha 3, o lexema `,` está na coluna 7 e tem
comprimento 1, então o diagnóstico aponta a coluna 8.

### TK_EOF

O analisador léxico encerra o `.lex` com `<TK_EOF, EOF>`, mas o módulo
sintático não depende disso: se o token não estiver presente, ele é
acrescentado ao fluxo; se já estiver, não é duplicado. A verificação foi feita
chamando `AnaliseSintatica()` diretamente sobre um `.lex` escrito à mão, sem
`TK_EOF` e sem quebra de linha final:

```
<DIR_TEXT, .text> 1 1
<INS_SYSCALL, syscall> 2 1
```

A árvore resultante traz as duas folhas sintetizadas, o `EOL` da linha 2 e o
`TK_EOF`, e o programa é aceito.

O `TK_EOF` é consumido por `<programa>` como último filho da raiz, o que
garante o consumo integral da entrada: tokens remanescentes depois da última
seção fazem o reconhecimento falhar em vez de serem ignorados.

## 5. Correspondência entre os não terminais e as funções

A gramática do enunciado tem 32 não terminais, e cada um deles corresponde a
uma função de `sintatico.c`. A correspondência é um para um, sem exceções:

| Produção                       | Função                          |
| ------------------------------ | ------------------------------- |
| `<programa>`                   | `programa()`                    |
| `<secao>`                      | `secao()`                       |
| `<secao_dados>`                | `secao_dados()`                 |
| `<secao_codigo>`               | `secao_codigo()`                |
| `<linha_dados>`                | `linha_dados()`                 |
| `<linha_codigo>`               | `linha_codigo()`                |
| `<rotulo>`                     | `rotulo()`                      |
| `<diretiva_global>`            | `diretiva_global()`             |
| `<diretiva_dados>`             | `diretiva_dados()`              |
| `<diretiva_inteiros>`          | `diretiva_inteiros()`           |
| `<lista_inteiros>`             | `lista_inteiros()`              |
| `<diretiva_string>`            | `diretiva_string()`             |
| `<instrucao>`                  | `instrucao()`                   |
| `<instrucao_tres_registradores>` | `instrucao_tres_registradores()` |
| `<op_tres_registradores>`      | `op_tres_registradores()`       |
| `<instrucao_dois_registradores>` | `instrucao_dois_registradores()` |
| `<op_dois_registradores>`      | `op_dois_registradores()`       |
| `<instrucao_um_registrador>`   | `instrucao_um_registrador()`    |
| `<op_um_registrador>`          | `op_um_registrador()`           |
| `<instrucao_imediato>`         | `instrucao_imediato()`          |
| `<instrucao_deslocamento>`     | `instrucao_deslocamento()`      |
| `<op_deslocamento>`            | `op_deslocamento()`             |
| `<instrucao_li>`               | `instrucao_li()`                |
| `<instrucao_la>`               | `instrucao_la()`                |
| `<instrucao_memoria>`          | `instrucao_memoria()`           |
| `<op_memoria>`                 | `op_memoria()`                  |
| `<endereco>`                   | `endereco()`                    |
| `<instrucao_desvio>`           | `instrucao_desvio()`            |
| `<op_desvio>`                  | `op_desvio()`                   |
| `<instrucao_salto>`            | `instrucao_salto()`             |
| `<op_salto>`                   | `op_salto()`                    |
| `<instrucao_sem_operandos>`    | `instrucao_sem_operandos()`     |

Cada função devolve o nó que construiu, ou `NULL` quando encontra um erro, e
tem a produção correspondente escrita no comentário acima dela, para que uma
possa ser conferida contra a outra. O caso mais simples mostra o padrão
inteiro:

```c
/* <instrucao_li> ::= "li" REG "," NUM_INT */
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

`consumir()` confere a categoria do token atual; se bater, cria a folha,
pendura-a no nó e avança; se não bater, registra o diagnóstico e devolve 0. O
segundo argumento é a categoria esperada e o terceiro é o texto que aparece no
diagnóstico.

### Como a EBNF é traduzida

As repetições `{ ... }` viram laços. Em
`<lista_inteiros> ::= NUM_INT { "," NUM_INT }`, o `{ }` é um
`while (eh("SMB_COM"))`. As partes opcionais `[ ... ]` viram `if` sobre o
token atual: em `<linha_codigo> ::= <rotulo> [ <instrucao> ] EOL`, depois de
reconhecer o rótulo a função pergunta se o próximo token pode iniciar uma
instrução antes de chamá-la. Quando a parte opcional está ausente, nenhum nó é
criado, como o enunciado exige para a árvore.

As alternativas `|` viram decisões sobre o token atual, tomadas por três
funções auxiliares:

- `eh("REG")` diz se o token atual pertence a uma categoria;
- `inicia_diretiva_dados()` e `inicia_instrucao()` dizem se o token atual pode
  iniciar uma diretiva de dados ou uma instrução;
- `pertence(OP_TRES, QTD(OP_TRES))` diz se o token atual está em um dos grupos
  de instruções.

### O reconhecimento das seções

```c
/* <secao_dados> ::= ".data" EOL { <linha_dados> } */
```

A repetição `{ <linha_dados> }` termina quando aparece `.data`, `.text` ou o
fim do arquivo:

```c
while (!eh("DIR_DATA") && !eh("DIR_TEXT") && !eh(TK_EOF)) {
```

Isso atende a um ponto em que o enunciado é explícito: uma construção não
permitida na seção atual, como uma instrução dentro de `.data`, é um erro, e
não o encerramento válido da seção. Tratá-la como fim de seção
faria o analisador aceitar um programa inválido.

`<programa> ::= <secao> { <secao> } TK_EOF` admite alternância livre entre as
seções e seções vazias, e não exige um rótulo chamado `main`.

## 6. Diagnóstico de erros

Quando o token encontrado não é o esperado, `erro()` grava duas linhas no
`.sin.err`, no formato do enunciado:

```
Linha 4, coluna 9:
Encontrado REG("$t1"); esperado SMB_COM(",").
```

Cada diagnóstico traz a linha, a coluna, a categoria e o lexema do token
encontrado, e o que era esperado naquela posição.

O token encontrado aparece em três formas:

| Situação                | Texto                 |
| ----------------------- | --------------------- |
| Token comum             | `CATEGORIA("lexema")` |
| Marcador de fim de linha | `fim de linha`        |
| Fim do arquivo          | `TK_EOF`              |

O texto do "esperado" vem de quem chamou. Para os terminais mais frequentes, a
função `descricao()` monta a forma `SMB_COM(",")` a partir da categoria, o que
evita repetir essas cadeias em dezenas de pontos do código. Quando o esperado
é uma construção inteira, e não um único token, o texto é descritivo:

```
Encontrado DIR_WORD(".word"); esperado rótulo, instrução ou DIR_GLOBL(".globl").
```

O primeiro erro já marca o programa como inválido, ainda que a recuperação
permita reconhecer todas as construções seguintes.

### Os cinco exemplos do enunciado

A seção 5 do enunciado lista cinco construções que precisam ser rejeitadas.
Todas são rejeitadas, com o diagnóstico abaixo (`.word 10,` testado dentro de
uma seção `.data`, como pedido):

| Construção          | Diagnóstico produzido                                        |
| ------------------- | ------------------------------------------------------------ |
| `add $t0, $t1`      | `Encontrado fim de linha; esperado SMB_COM(",").`            |
| `li $t0, mensagem`  | `Encontrado ID("mensagem"); esperado NUM_INT.`               |
| `lw $t0, 4 $sp`     | `Encontrado REG("$sp"); esperado SMB_OPA("(").`              |
| `syscall $t0`       | `Encontrado REG("$t0"); esperado fim de linha.`              |
| `.word 10,`         | `Encontrado fim de linha; esperado NUM_INT.`                 |

O exemplo da seção 4 do enunciado também é reproduzido literalmente. Para a
linha `add $t0 $t1, $t2`, sem espaços iniciais, na linha 4:

```
Linha 4, coluna 9:
Encontrado REG("$t1"); esperado SMB_COM(",").
```

## 7. Recuperação de erros

Depois de registrar o diagnóstico, `recuperar()` descarta o restante da linha
corrente e retoma na linha seguinte. São três casos:

- se o token atual já for o `EOL`, consome apenas esse marcador;
- se for o `TK_EOF`, não faz nada, porque não há linha seguinte;
- nos outros casos, avança até o `EOL` e o consome.

Fora do fim do arquivo, a posição de leitura avança sempre pelo menos um
token. É isso que garante o avanço na entrada e impede que o mesmo diagnóstico
seja repetido para sempre.

Tokens de linhas posteriores nunca são descartados: a recuperação para no
primeiro `EOL`. Por isso um erro na linha 3 não esconde um erro na linha 5.

A seção ativa é preservada, porque a recuperação acontece dentro do laço de
`secao_dados()` ou `secao_codigo()`, que continua executando. Se o token que
abre a linha seguinte for `.data` ou `.text`, o laço termina normalmente e a
mudança de seção é reconhecida.

O arquivo `testes/sintatico/invalido_4_recuperacao.asm` existe só para medir
isso. Ele tem erros nas linhas 3, 5 e 7 do código normalizado, intercalados com
linhas corretas:

```
.text
main:
li   $t0,              <- erro: falta o inteiro
li   $t1, 20
add  $t2, $t0          <- erro: falta o terceiro registrador
add  $t3, $t0, $t1
beq  $t0 $t1, fim      <- erro: falta a virgula
bne  $t0, $t1, fim
fim:
syscall
```

O resultado são exatamente três diagnósticos, um por linha com erro, e nenhum
nas linhas corretas entre elas:

```
Linha 3, coluna 8:
Encontrado fim de linha; esperado NUM_INT.
Linha 5, coluna 13:
Encontrado fim de linha; esperado SMB_COM(",").
Linha 7, coluna 9:
Encontrado REG("$t1"); esperado SMB_COM(",").
```

## 8. A construção da árvore sintática concreta

### Durante a análise

A árvore é montada junto com o reconhecimento, de baixo para cima. Cada função
de não terminal cria o seu nó com `novo_no()` e pendura nele, na ordem da
produção, os nós dos símbolos que reconhece:

- os terminais entram como folhas criadas por `folha()`, que copia a categoria
  e o lexema do token;
- os não terminais entram como a subárvore devolvida pela função
  correspondente.

Como os filhos são acrescentados na ordem em que os símbolos aparecem na
produção, a ordem exigida pelo enunciado é consequência direta da estrutura do
código, e não de uma ordenação posterior. As repetições da EBNF aparecem como
filhos sucessivos do mesmo nó, porque o laço acrescenta um filho por iteração.
Partes opcionais ausentes não geram nós, porque o `if` não chega a chamar a
função.

Todos os terminais entram na árvore, inclusive os separadores, os
delimitadores, os `EOL` e o `TK_EOF`.

### A gravação do DOT

A gravação é feita em duas passagens sobre a árvore pronta:

1. `declarar()` numera os nós em pré-ordem, atribuindo `n0`, `n1`, `n2`...
   e escrevendo uma linha de declaração para cada um. Numerar em pré-ordem
   garante identificador exclusivo por ocorrência e deixa o arquivo na mesma
   ordem do exemplo do enunciado.
2. `ligar()` percorre a árvore outra vez e escreve uma aresta `pai -> filho`
   para cada ligação.

O rótulo de um não terminal é o seu nome entre `<` e `>`. O rótulo de um
terminal é a categoria e, quando existe, o lexema em uma segunda linha
(`REG\n$t0`). Os `EOL` e o `TK_EOF` aparecem apenas com a categoria.

Os rótulos passam por `escapar()`, que protege `"` e `\`. Sem isso, uma
diretiva como `.asciiz "ele disse \"ola\""` produziria um arquivo que o
Graphviz recusa. O resultado para essa diretiva é:

```
n39 [label="STRING\n\"ele disse \\\"ola\\\" e saiu\""];
```

### Programa inválido não tem árvore

O `.dot` só é escrito quando `total_erros == 0`. Para programa inválido o
arquivo é criado e permanece vazio, como o enunciado determina, e nunca com
uma árvore pela metade. A árvore continua sendo construída durante a análise, e é descartada no
final.

### Exemplo completo

Para `testes/sintatico/valido_5_sem_quebra_final.asm`, cujo código normalizado
é

```
.text
main:
li $v0, 10
syscall
```

o `.lex` tem nove linhas (a última é o `TK_EOF`) e a árvore tem 24 nós:

```
digraph ArvoreSintatica {
    graph [ordering=out];
    node [shape=box, fontname="Arial"];
    edge [arrowhead=none];

    n0 [label="<programa>"];
    n1 [label="<secao>"];
    n2 [label="<secao_codigo>"];
    n3 [label="DIR_TEXT\n.text"];
    n4 [label="EOL"];
    n5 [label="<linha_codigo>"];
    n6 [label="<rotulo>"];
    n7 [label="ID\nmain"];
    n8 [label="SMB_COL\n:"];
    n9 [label="EOL"];
    n10 [label="<linha_codigo>"];
    n11 [label="<instrucao>"];
    n12 [label="<instrucao_li>"];
    n13 [label="INS_LI\nli"];
    n14 [label="REG\n$v0"];
    n15 [label="SMB_COM\n,"];
    n16 [label="NUM_INT\n10"];
    n17 [label="EOL"];
    n18 [label="<linha_codigo>"];
    n19 [label="<instrucao>"];
    n20 [label="<instrucao_sem_operandos>"];
    n21 [label="INS_SYSCALL\nsyscall"];
    n22 [label="EOL"];
    n23 [label="TK_EOF"];

    n0 -> n1;
    n0 -> n23;
    n1 -> n2;
    n2 -> n3;
    n2 -> n4;
    n2 -> n5;
    n2 -> n10;
    n2 -> n18;
    n5 -> n6;
    n5 -> n9;
    n6 -> n7;
    n6 -> n8;
    n10 -> n11;
    n10 -> n17;
    n11 -> n12;
    n12 -> n13;
    n12 -> n14;
    n12 -> n15;
    n12 -> n16;
    n18 -> n19;
    n18 -> n22;
    n19 -> n20;
    n20 -> n21;
}
```

O `EOL` do nó `n22` mostra o caso da última linha: o `.asm` desse teste não
termina em quebra de linha, e o marcador é produzido mesmo assim.

### Conferência com o exemplo do enunciado

O enunciado traz, na seção 6, um programa de exemplo e o código DOT da sua
árvore. Executando o programa sobre esse mesmo exemplo, o arquivo gerado é
idêntico ao publicado, linha por linha: mesma numeração de nós, mesmos
rótulos e mesma ordem de arestas. A única diferença são duas linhas em branco
de separação que o nosso arquivo mantém.

### Imagem renderizada

A imagem `docs/arvore.png` é a árvore completa de
`testes/preprocessamento/06_exemplo_enunciado.asm`, um programa com as duas
seções, rótulo, diretiva de cadeia e três instruções, com 51 nós. Foi gerada
com o comando do enunciado:

```
dot -Tpng testes/saida/06_exemplo_enunciado.dot -o docs/arvore.png
```

## 9. Limites entre sintaxe e semântica

O analisador verifica a estrutura das construções e não as suas propriedades
semânticas. Não são tratadas como erro, conforme o enunciado:

- referências a rótulos inexistentes;
- declarações duplicadas de rótulos;
- inteiros fora dos limites de representação dos operandos;
- quantidades de deslocamento inválidas em `sll` e `srl`;
- valores semanticamente inválidos em `.space` e `.align`;
- códigos de serviço inválidos ou não preparados antes de `syscall`.

O arquivo `testes/sintatico/valido_4_limites.asm` existe para registrar esse
limite: ele salta para `nao_existe`, desvia para `tambem_nao_existe` e declara
`repetido:` duas vezes. É aceito, com código de encerramento 0, porque todas as
construções respeitam as produções. A verificação desses pontos pertence à
análise semântica.

## 10. Compilação e execução

Compilação direta, em qualquer sistema com um compilador C:

```
gcc main.c preprocessador.c lexico.c sintatico.c -o main.exe
```

Execução:

```
./main.exe teste.asm resultado
```

No Windows, `main.exe teste.asm resultado`.

O projeto tem um `Makefile` com três atalhos:

| Comando       | Efeito                                                     |
| ------------- | ---------------------------------------------------------- |
| `make`        | Compila com `-std=c99 -Wall -Wextra`                       |
| `make testes` | Executa o programa sobre os 30 arquivos de teste           |
| `make limpar` | Apaga o executável e a pasta de saídas                     |

As saídas de `make testes` ficam em `testes/saida/`, com o nome de cada teste
como prefixo.

Para renderizar uma árvore, com o Graphviz instalado:

```
dot -Tpng resultado.dot -o resultado.png
```

## 11. Casos de teste

A pasta `testes/sintatico` tem dez arquivos, cinco válidos e cinco inválidos,
acima do mínimo de três e três. Somados aos vinte arquivos das etapas
anteriores, que continuam sendo executados pelo pipeline completo, são 30
casos.

### Cobertura exigida

| Categoria            | Casos obrigatórios                                                      | Onde está                                             |
| -------------------- | ----------------------------------------------------------------------- | ----------------------------------------------------- |
| Organização          | Programa iniciado por `.text`; `.data` e `.text`; alternância e seção vazia | `valido_1`, `valido_2`, `valido_3`                |
| Rótulos              | Rótulo isolado; rótulo seguido de instrução ou diretiva na mesma linha   | `valido_1`, `valido_2`, `05_rotulos`                  |
| Diretivas            | Todas as diretivas previstas na gramática                               | `valido_2`                                            |
| Instruções           | Todos os grupos e todas as operações previstas                          | `valido_1`, `valido_3`                                |
| Operandos            | Decimais, hexadecimais, cadeias, listas de inteiros e identificadores   | `valido_2`, `valido_3`                                |
| Memória              | Endereço por identificador e por deslocamento com registrador           | `valido_3`, `memoria`                                 |
| Linhas               | Linhas vazias, comentários e arquivo sem quebra de linha final          | `valido_1`, `01_comentarios`, `valido_5_sem_quebra_final` |
| Entrada inválida     | Arquivo vazio e programa sem abertura de seção                          | `invalido_5_vazio`, `invalido_3_secoes`                |
| Seção incorreta      | Instrução em `.data` e diretiva de dados em `.text`                     | `invalido_3_secoes`                                   |
| Estrutura incorreta  | Operandos ausentes, excedentes, fora de ordem ou de categoria inadequada | `invalido_1_operandos`                               |
| Pontuação            | Vírgulas, dois-pontos ou parênteses ausentes; lista terminada por vírgula | `invalido_2_pontuacao`                              |
| Consumo completo     | Tokens adicionais após uma construção completa                          | `invalido_1_operandos`, `invalido_2_pontuacao`         |
| Recuperação          | Erros em linhas distintas, com reconhecimento das construções posteriores | `invalido_4_recuperacao`                            |
| Limites da etapa     | Referência a rótulo inexistente e declaração duplicada de rótulo        | `valido_4_limites`                                    |

As 36 instruções e as 10 diretivas do subconjunto aparecem ao menos uma vez nos
programas válidos. Os cinco programas com erro sintático são todos
lexicalmente válidos, como o enunciado exige: o `.lex.err` de cada um contém
`Nenhum erro léxico encontrado.`

### Resultados esperados e obtidos

| Teste                       | Resultado esperado                 | Obtido | Código | Diagnósticos | Nós do `.dot` |
| --------------------------- | ---------------------------------- | ------ | ------ | ------------ | ------------- |
| `valido_1`                  | válido                             | válido | 0      | 0            | 174           |
| `valido_2`                  | válido                             | válido | 0      | 0            | 156           |
| `valido_3`                  | válido                             | válido | 0      | 0            | 315           |
| `valido_4_limites`          | válido (erros são semânticos)      | válido | 0      | 0            | 49            |
| `valido_5_sem_quebra_final` | válido                             | válido | 0      | 0            | 24            |
| `invalido_1_operandos`      | inválido, 6 linhas com erro        | inválido | 1    | 6            | vazio         |
| `invalido_2_pontuacao`      | inválido, 6 linhas com erro        | inválido | 1    | 6            | vazio         |
| `invalido_3_secoes`         | inválido, 3 linhas com erro        | inválido | 1    | 3            | vazio         |
| `invalido_4_recuperacao`    | inválido, 3 linhas com erro        | inválido | 1    | 3            | vazio         |
| `invalido_5_vazio`          | inválido (arquivo vazio)           | inválido | 1    | 1            | vazio         |

Os vinte testes das etapas anteriores, executados pelo pipeline completo:

| Teste                   | Resultado esperado            | Obtido                 | Código | Nós do `.dot` |
| ----------------------- | ----------------------------- | ---------------------- | ------ | ------------- |
| `correto_1`             | válido                        | válido                 | 0      | 124           |
| `correto_2`             | válido                        | válido                 | 0      | 125           |
| `correto_3`             | válido                        | válido                 | 0      | 405           |
| `comentarios`           | válido                        | válido                 | 0      | 37            |
| `strings`               | válido                        | válido                 | 0      | 87            |
| `numeros`               | válido                        | válido                 | 0      | 105           |
| `memoria`               | válido                        | válido                 | 0      | 182           |
| `erro_1`                | 4 erros léxicos, etapa 3 não executada | igual         | 1      | vazio         |
| `erro_2`                | 2 erros léxicos, etapa 3 não executada | igual         | 1      | vazio         |
| `erro_3`                | 8 erros léxicos, etapa 3 não executada | igual         | 1      | vazio         |
| `01_comentarios`        | válido                        | válido                 | 0      | 63            |
| `02_strings`            | válido                        | válido                 | 0      | 77            |
| `03_espacos`            | válido                        | válido                 | 0      | 54            |
| `04_linhas_vazias`      | válido                        | válido                 | 0      | 33            |
| `05_rotulos`            | válido                        | válido                 | 0      | 122           |
| `06_exemplo_enunciado`  | válido                        | válido                 | 0      | 51            |
| `07_quebras_windows`    | válido                        | válido                 | 0      | 33            |
| `08_quebras_mac_antigo` | válido                        | válido                 | 0      | 19            |
| `09_sem_validacao`      | 2 erros léxicos, etapa 3 não executada | igual         | 1      | vazio         |
| `10_arquivo_vazio`      | inválido (sem seção)          | inválido               | 1      | vazio         |

Nenhuma entrada válida foi rejeitada e nenhuma entrada inválida foi aceita. Os
20 programas válidos geram 20 árvores; as 10 entradas rejeitadas deixam o
`.dot` vazio.

### Verificações adicionais

- Compilação com `-std=c99 -Wall -Wextra -pedantic`: nenhum aviso.
- Execução dos 30 testes com AddressSanitizer e UndefinedBehaviorSanitizer:
  nenhuma ocorrência.
- Execução dos 30 testes sob o verificador de vazamentos do sistema: 0 bytes
  vazados em todos. (A detecção de vazamentos do AddressSanitizer não está
  disponível em macOS, por isso essa verificação foi feita separadamente.)
- Os 20 arquivos DOT gerados foram submetidos ao Graphviz: todos são aceitos
  sem erro de sintaxe.

## 12. Os diagnósticos gerados

Conteúdo integral dos arquivos `.sin.err` dos programas inválidos. As posições
referem-se ao código normalizado (`.pre`), conforme explicado na seção 13.

### `invalido_1_operandos`

Operandos ausentes, excedentes, fora de ordem e de categoria inadequada.

```
Linha 5, coluna 13:
Encontrado fim de linha; esperado SMB_COM(",").
Linha 6, coluna 9:
Encontrado ID("mensagem"); esperado NUM_INT.
Linha 7, coluna 11:
Encontrado SMB_COM(","); esperado fim de linha.
Linha 8, coluna 10:
Encontrado NUM_INT("5"); esperado REG.
Linha 9, coluna 5:
Encontrado fim de linha; esperado REG.
Linha 10, coluna 9:
Encontrado REG("$t0"); esperado fim de linha.
```

As linhas correspondem, na ordem, a `add $t0, $t1`, `li $t0, mensagem`,
`li $t0, 10, 20`, `add $t0, 5, $t1`, `mflo` e `syscall $t0`. As duas últimas
linhas do arquivo, `li $v0, 10` e `syscall`, estão corretas e não produzem
diagnóstico.

### `invalido_2_pontuacao`

Vírgulas, dois-pontos e parênteses ausentes.

```
Linha 2, coluna 17:
Encontrado fim de linha; esperado NUM_INT.
Linha 3, coluna 16:
Encontrado NUM_INT("2"); esperado fim de linha.
Linha 5, coluna 5:
Encontrado fim de linha; esperado SMB_COL(":").
Linha 6, coluna 11:
Encontrado REG("$sp"); esperado SMB_OPA("(").
Linha 7, coluna 14:
Encontrado fim de linha; esperado SMB_CPA(")").
Linha 8, coluna 9:
Encontrado REG("$t0"); esperado SMB_COM(",").
```

A primeira é a lista `.word 10,` terminada por vírgula, dentro da seção
`.data`. A terceira é `main` sem os dois-pontos.

### `invalido_3_secoes`

Instrução antes de qualquer seção, `syscall` dentro de `.data` e `.word`
dentro de `.text`.

```
Linha 1, coluna 1:
Encontrado INS_LI("li"); esperado DIR_DATA(".data") ou DIR_TEXT(".text").
Linha 4, coluna 1:
Encontrado INS_SYSCALL("syscall"); esperado rótulo, diretiva de dados ou DIR_GLOBL(".globl").
Linha 7, coluna 1:
Encontrado DIR_WORD(".word"); esperado rótulo, instrução ou DIR_GLOBL(".globl").
```

O segundo e o terceiro diagnósticos mostram o ponto tratado na seção 5 deste
relatório: a construção proibida é diagnosticada e a seção continua ativa.
A linha `nop`, que vem depois do erro em `.text`, é reconhecida normalmente.

### `invalido_4_recuperacao`

Os três diagnósticos estão transcritos na seção 7.

### `invalido_5_vazio` e `10_arquivo_vazio`

Arquivo vazio e arquivo apenas com comentários. Nos dois casos a produção
`<programa>` exige pelo menos uma seção:

```
Linha 1, coluna 1:
Encontrado TK_EOF; esperado DIR_DATA(".data") ou DIR_TEXT(".text").
```

### Programas válidos

Para todos os programas válidos o `.sin.err` contém exatamente a linha exigida
pelo enunciado:

```
Nenhum erro sintático encontrado.
```

## 13. Decisões tomadas em pontos não fechados pelo enunciado

**Linha e coluna.** O enunciado determina que a análise léxica leia o arquivo
produzido pelo pré-processamento, e as posições foram contadas nesse arquivo. É
a mesma decisão da etapa anterior, e os diagnósticos sintáticos a herdam, já
que usam a localização gravada no `.lex`. Na prática, um erro em uma linha
indentada aparece em uma coluna menor do que no `.asm` original, e as linhas de
comentário removidas deslocam a numeração. Executando apenas as etapas 2 e 3
sobre o código original, as posições coincidem com as dos exemplos do
enunciado. Vale confirmar com o professor qual das duas contagens ele espera.

**Rótulos e a categoria ID.** O analisador léxico não usa categoria própria
para rótulos: `main:` produz `ID` e `SMB_COL`. Como a gramática usa `ID` nessas
posições, nenhuma adaptação foi necessária.

**Instrução usada como rótulo.** Construções como `add:` produzem os tokens
`INS_ADD` e `SMB_COL`, sem erro léxico. Na análise sintática a linha é
rejeitada: dentro de `.text`, `INS_ADD` inicia uma instrução, `add` é
reconhecido como `<op_tres_registradores>` e o `:` aparece onde se esperava um
registrador. O diagnóstico é
`Encontrado SMB_COL(":"); esperado REG.`

**Um diagnóstico por linha.** O enunciado não exige a identificação de todos
os erros de uma mesma linha. O analisador registra o primeiro e descarta o
resto da linha, o que evita a cascata de mensagens derivadas do mesmo problema.

**`.globl` nas duas seções.** A gramática admite `<diretiva_global>` tanto em
`<linha_dados>` quanto em `<linha_codigo>`, e as duas funções a tratam. O
teste `valido_2` usa `.globl` em `.data` e o `valido_1` usa em `.text`.
