# Como funciona o analisador léxico

Este texto explica o código de `lexico.c` e `lexico.h` e como ele se liga ao
resto do programa. A ideia é que qualquer pessoa do grupo consiga ler o código
depois disto e explicar cada parte.

## 1. O caminho de um arquivo pelo programa

O `main` faz as etapas em ordem:

1. `PreProcessamento(asm, pre)` lê o `.asm` e grava o `.pre` normalizado.
2. `AnaliseLexica(pre, lex)` lê o `.pre` e grava um token por linha no `.lex`.
3. `GravarTabelaSimbolos(ts)` grava a tabela de símbolos no `.ts`.
4. `GravarErros(err)` grava os erros léxicos no `.err`.
5. `LiberarLexico()` libera a memória.

Os passos 3 e 4 existem porque o enunciado fixou a assinatura
`void AnaliseLexica(FILE *in, FILE *out)`. Ela só recebe dois arquivos, então
não tem como gravar o `.ts` e o `.err` sozinha. A tabela e os erros ficam
guardados em memória durante a análise e o `main` pede para gravá-los depois.

## 2. Estruturas de dados

As duas structs são as do enunciado:

```c
typedef struct {
    char nome[50];      // INS_ADD, REG, ID... ou ERRO_... nos erros
    char lexema[100];   // o texto como aparece no arquivo
    int linha;
    int coluna;
} Token;

typedef struct {
    char lexema[100];
    char categoria[30]; // diretiva, instrução, registrador, identificador/rótulo
    int primeiraLinha;
    int primeiraColuna;
} Simbolo;
```

A mesma `Token` é usada para guardar os erros, porque o formato de um erro no
`.err` (`<tipo, lexema> linha coluna`) é igual ao de um token no `.lex`. Só
muda o `nome`, que passa a ser o tipo do erro.

Além delas, `lexico.c` tem algumas variáveis `static`, visíveis só dentro do
arquivo:

- `entrada`, `linha` e `coluna` — de onde estamos lendo e em que posição;
- `tabela`, `total_simbolos` e `capacidade_tabela` — a tabela de símbolos;
- `erros`, `total_erros` e `capacidade_erros` — a lista de erros.

Elas precisam ser globais do arquivo porque `ProximoToken()` é chamada várias
vezes, e cada chamada continua de onde a anterior parou.

A tabela e a lista de erros são vetores que crescem com `realloc`: começam com
128 e 32 posições e dobram quando enchem. É a mesma técnica do buffer de linha
do pré-processador. A busca na tabela é sequencial, o que é mais que suficiente
para o tamanho dos programas do trabalho.

## 3. Leitura e controle de linha e coluna

Duas funções fazem toda a leitura do arquivo:

- `espiar()` olha o próximo caractere sem tirá-lo do arquivo (lê com `fgetc`
  e devolve com `ungetc`);
- `avancar()` tira o caractere do arquivo e atualiza a posição.

A regra da posição fica toda em `avancar()`: um `\n` soma uma linha e volta a
coluna para 1, e qualquer outro caractere soma uma coluna. Como ninguém mais
mexe em `linha` e `coluna`, não tem como a contagem sair errada em um lugar
e certa em outro.

Tem um detalhe: letras com acento ocupam dois bytes em UTF-8. Se cada byte
contasse uma coluna, tudo que viesse depois de um `"Olá"` na mesma linha
ficaria uma coluna adiante do que o editor mostra. Por isso os bytes de
continuação do UTF-8 (os que começam com `10` em binário) não somam coluna.

A posição de um token é a posição do seu primeiro caractere. O estado inicial
anota `linha` e `coluna` antes de consumir qualquer coisa, e é isso que vai
para o token.

## 4. O AFD

`ProximoToken()` é o autômato. O `enum Estado` lista os estados, e o `switch`
dentro do `for` tem um `case` para cada um. Cada volta do laço faz sempre a
mesma coisa: espia o próximo caractere e o `case` do estado atual decide se
consome, se muda de estado, ou se o lexema acabou.

O diagrama em `docs/afd.png` foi feito a partir do código, com os mesmos
nomes de estado.

| Estado | Entra quando lê | O que faz |
|---|---|---|
| `E_INICIAL` | — | Pula espaços e quebras de linha e decide pelo primeiro caractere do lexema |
| `E_COMENTARIO` | `#` | Consome tudo até o fim da linha, sem gerar token |
| `E_IDENTIFICADOR` | letra ou `_` | Junta letras, dígitos e `_`; no fim consulta a tabela |
| `E_DIRETIVA` | `.` | Igual, mas compara com as diretivas |
| `E_REGISTRADOR` | `$` | Igual, mas compara com os registradores |
| `E_SINAL` | `-` | Só vira número se o próximo for dígito |
| `E_ZERO` | `0` | Decide entre o número zero, um decimal ou um hexadecimal |
| `E_DECIMAL` | `1` a `9` | Junta dígitos |
| `E_HEX_INICIO` | `0x` | Exige pelo menos um dígito hexadecimal |
| `E_HEX` | dígito hexadecimal | Junta dígitos hexadecimais |
| `E_STRING` | `"` | Junta tudo até as aspas de fechamento |
| `E_ESCAPE` | `\` dentro da string | Confere o caractere seguinte |
| `E_MALFORMADO` | caractere estranho no meio de um lexema | Descarta até o próximo delimitador |

Os símbolos `,` `:` `(` `)` não têm estado próprio: `E_INICIAL` já devolve o
token direto, porque eles têm um caractere só.

### Delimitadores

Um lexema termina quando aparece um delimitador: espaço, tabulação, quebra de
linha, vírgula, dois-pontos, parênteses, aspas, `#` ou o fim do arquivo. O
estado **não consome** o delimitador, só percebe que o lexema acabou. No
diagrama isso aparece como `delim*`, e o asterisco quer dizer exatamente isso:
o caractere fica no arquivo para a próxima chamada.

Isso é o que faz `4($sp)` funcionar. O `(` encerra o número `4`, mas continua
lá e vira `SMB_OPA` na chamada seguinte. Se `E_DECIMAL` consumisse o `(`, o
token se perderia.

### Exemplo passo a passo

Para a linha `lw $t0, 4($sp)`, `AnaliseLexica()` chama `ProximoToken()` oito
vezes:

| Chamada | Caminho no AFD | Devolve |
|---|---|---|
| 1 | `INICIAL` –l→ `IDENTIFICADOR` –w→ `IDENTIFICADOR`, espaço é delimitador | `<INS_LW, lw> 1 1` |
| 2 | `INICIAL` pula o espaço –$→ `REGISTRADOR` –t→ –0→, vírgula é delimitador | `<REG, $t0> 1 4` |
| 3 | `INICIAL` lê a vírgula | `<SMB_COM, ,> 1 7` |
| 4 | `INICIAL` pula o espaço –4→ `DECIMAL`, `(` é delimitador | `<NUM_INT, 4> 1 9` |
| 5 | `INICIAL` lê o `(` | `<SMB_OPA, (> 1 10` |
| 6 | `INICIAL` –$→ `REGISTRADOR` –s→ –p→, `)` é delimitador | `<REG, $sp> 1 11` |
| 7 | `INICIAL` lê o `)` | `<SMB_CPA, )> 1 14` |
| 8 | `INICIAL` pula o `\n` e encontra o fim do arquivo | `<TK_EOF, EOF> 2 1` |

Na chamada 1, quando o lexema `lw` termina, `classificar_palavra()` procura
`lw` na tabela, encontra com a categoria `instrução` e monta o nome `INS_LW`.

## 5. Tabela de símbolos

### Carga inicial

`IniciarLexico()` coloca na tabela, com linha e coluna 0:

- as 10 diretivas do vetor `DIRETIVAS`;
- as 36 instruções do vetor `INSTRUCOES`;
- os 31 registradores com nome do vetor `REGISTRADORES`;
- os registradores numéricos `$0` a `$31`, gerados num laço.

São 109 entradas antes de ler o primeiro caractere. Para acrescentar uma
instrução nova ao trabalho, basta incluir o nome no vetor `INSTRUCOES`: o
reconhecimento e o nome do token (`INS_` + o nome em maiúsculas) saem sozinhos.

### Palavras reservadas

O enunciado diz que o analisador deve consultar a tabela para saber se um
lexema é instrução ou identificador, e é o que `reconhecer_reservada()` faz.
Ela converte o lexema para minúsculas, procura na tabela e confere a
categoria. Se achar e a posição ainda estiver zerada, anota a posição deste
uso como a primeira ocorrência.

Por converter para minúsculas, `ADD`, `Add` e `add` são a mesma instrução.
O `.lex` mostra o lexema como está no arquivo (`<INS_ADD, ADD>`), mas a tabela
guarda `add`.

### Identificadores

Se o lexema não for instrução, é um `ID`. Ele entra na tabela na primeira vez
que aparece, sem converter para minúsculas, porque o enunciado manda os
rótulos diferenciarem maiúsculas de minúsculas.

"Primeira vez que aparece" inclui referências. Em

```
j fim
...
fim:
```

`fim` entra na tabela com a posição do `j fim`. É isso que permite aceitar
referências antecipadas: o analisador não precisa saber se o rótulo vai ser
declarado depois, só registra o nome. Conferir se ele foi declarado é trabalho
da análise semântica.

### Repetições

`inserir_simbolo()` procura o lexema antes de inserir e não faz nada se ele já
estiver lá. Por isso cada lexema aparece uma vez só no `.ts`.

## 6. Erros e recuperação

Quando um lexema tem erro, `registrar_erro()` guarda o tipo, o texto e a
posição, e o autômato volta ao estado inicial para procurar o próximo token.
`ProximoToken()` nunca devolve um erro: ela só devolve tokens válidos e o
`TK_EOF`. Isso deixa o `.lex` limpo, e na etapa sintática o analisador vai
receber só tokens que fazem sentido.

A recuperação depende de onde o erro aparece:

- **Caractere inválido no começo de um lexema** (`@`): registra só aquele
  caractere e continua a partir do próximo. Se o caractere tiver acento,
  todos os bytes dele entram no erro, para o `.err` mostrar `ç` e não pedaços.
- **Caractere inválido no meio de um lexema** (`inicio@`, `0xG1`): o estado
  vai para `E_MALFORMADO`, que descarta tudo até o próximo delimitador e
  registra o lexema inteiro. Sem isso, `inicio@` viraria um `ID` válido
  seguido de um erro, o que esconderia o problema.
- **Lexema completo mas desconhecido** (`.dados`, `$t10`): a forma está certa,
  mas não está na tabela. Registra o erro no momento da consulta.
- **String sem fechar**: registra o erro com tudo o que foi lido até a quebra
  de linha. A quebra não é consumida, então a próxima linha é analisada
  normalmente.
- **Escape inválido** (`\q`): registra o erro com a posição da contrabarra e
  continua lendo a string até as aspas, para não interpretar o resto dela como
  código. A string não vira token.

Um caso que merece atenção é o número seguido de letra. `2valor` é registrado
como `ERRO_IDENTIFICADOR_MALFORMADO`, porque o enunciado dá esse exemplo como
identificador inválido. Já `12.5`, `-3abc` e `0xG1` são
`ERRO_NUMERO_MALFORMADO`. A função `erro_de_numero()` é quem faz essa escolha.

## 7. Funções de `lexico.c`

| Função | O que faz |
|---|---|
| `IniciarLexico()` | Zera a posição e carrega as palavras reservadas na tabela |
| `ProximoToken()` | O AFD: lê o próximo lexema e devolve o token |
| `AnaliseLexica()` | Chama `ProximoToken()` até o `TK_EOF` e grava cada token |
| `GravarTabelaSimbolos()` | Grava a tabela em CSV |
| `GravarErros()` | Grava os erros, ou a mensagem de que não houve nenhum |
| `LiberarLexico()` | Libera a tabela e a lista de erros |
| `espiar()`, `avancar()` | Leitura do arquivo e controle de linha e coluna |
| `consumir()` | Avança e acrescenta o caractere ao lexema |
| `eh_delimitador()` | Diz se um caractere encerra o lexema |
| `buscar_simbolo()`, `inserir_simbolo()` | Busca e inserção na tabela |
| `reconhecer_reservada()` | Consulta uma diretiva, instrução ou registrador |
| `classificar_palavra()` | Decide entre instrução e identificador |
| `registrar_erro()` | Guarda um erro na lista |
| `erro_de_numero()` | Escolhe o tipo de erro de um número malformado |

As funções públicas estão em PascalCase, como a `AnaliseLexica()` do
enunciado, e as auxiliares internas em minúsculas com `_`.

## 8. Testes

| Pedido no enunciado | Arquivo |
|---|---|
| Três programas lexicamente corretos | `correto_1`, `correto_2`, `correto_3` |
| Três programas com erros léxicos | `erro_1`, `erro_2`, `erro_3` |
| Um teste com comentários | `comentarios` |
| Um teste com cadeias de caracteres | `strings` |
| Um teste com números decimais e hexadecimais | `numeros` |
| Um teste com `deslocamento(registrador)` | `memoria` |

Os três corretos, somados, usam todas as 10 diretivas e as 36 instruções. Os
três com erros cobrem sete dos oito tipos de erro, incluindo os exemplos que
aparecem no enunciado (`@`, `$t10`, `.dados`, `0xG1`, a string não fechada,
`2valor`, `soma-total` e `inicio@`).

O oitavo, `ERRO_STRING_NAO_FECHADA_EOF`, não tem como aparecer pelo programa
completo. O pré-processador grava toda linha com `\n` no fim, então uma string
aberta sempre encontra a quebra de linha antes do fim do arquivo e gera
`ERRO_STRING_NAO_FECHADA`. O caso do EOF só acontece se `AnaliseLexica()` for
chamada direto sobre um arquivo cuja última linha não termina em `\n`, e o
código trata esse caso porque o enunciado pede.

Depois de `make testes`, os resultados ficam em `testes/saida`. Para conferir,
o mais útil é abrir lado a lado o `.pre` e o `.lex` do mesmo teste, porque as
posições do `.lex` se referem ao `.pre`.

## 9. O que ainda depende do professor

As posições de linha e coluna são contadas no `.pre`, porque é esse o arquivo
que o enunciado manda a análise léxica ler. Os exemplos do enunciado, porém,
mostram posições do `.asm` original com indentação (o `@` em `4 19`, o `lw` em
`7 5`). Rodando só o analisador léxico sobre o `.asm` original, as saídas batem
exatamente com as do enunciado. Pelo programa completo, o `@` aparece em `4 15`,
porque o pré-processador tirou os quatro espaços do começo da linha.

Vale confirmar com o professor qual das duas ele espera.
