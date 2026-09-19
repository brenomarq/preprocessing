# Analisador léxico μ-Assembly

Etapas 1 e 2 do projeto de Linguagens Formais, Autômatos e Compiladores.

Grupo: Breno Marques, Thiago de Jesus, Roberty Luann e Arthur

O programa lê um arquivo `.asm` escrito em μ-Assembly, faz o pré-processamento
e em seguida a análise léxica. No fim ficam quatro arquivos: o código
normalizado (`.pre`), os tokens (`.lex`), a tabela de símbolos (`.ts`) e os
erros léxicos (`.err`).

## Compilação e execução

```
gcc main.c preprocessador.c lexico.c -o main.exe
main.exe teste.asm teste.lex
```

Os arquivos `.h` não precisam ir no comando, porque são incluídos pelos `.c`.

Com dois nomes, o `.pre`, o `.ts` e o `.err` recebem o nome do `.lex` trocando
só a extensão: o exemplo acima gera `teste.pre`, `teste.lex`, `teste.ts` e
`teste.err`. Também dá para escolher o nome do `.pre`:

```
main.exe teste.asm saida.pre teste.lex
```

Se a quantidade de argumentos estiver errada, se algum arquivo não puder ser
aberto ou se dois arquivos tiverem o mesmo nome, o programa mostra o erro e
encerra. Os erros léxicos não interrompem a execução: eles vão para o `.err`.

## Arquivos

- `main.c` — argumentos, abertura dos arquivos e a ordem das etapas.
- `preprocessador.c` e `preprocessador.h` — etapa 1.
- `lexico.c` e `lexico.h` — etapa 2.
- `docs/afd.dot` e `docs/afd.png` — o diagrama do AFD.
- `docs/lexico.md` — explicação detalhada do código do analisador léxico.
- `testes/` — arquivos `.asm` das duas etapas.

## Pré-processamento

É a etapa 1, feita pela função `PreProcessamento()`. Ela remove comentários,
linhas vazias e espaços desnecessários, troca tabulações por espaço, aceita
arquivos com quebra de linha de qualquer sistema e grava tudo com `\n`. O `#` e
os espaços só são mexidos fora das aspas, então o conteúdo das strings é
preservado. Nenhuma validação é feita nessa etapa.

## Análise léxica

É a etapa 2, feita pela função `AnaliseLexica()`. Ela lê o `.pre` caractere por
caractere usando um autômato finito determinístico (o diagrama está em
`docs/afd.png`), agrupa os caracteres em lexemas e classifica cada um.

### Tokens reconhecidos

| Categoria | Exemplos de lexema | Token |
|---|---|---|
| Diretivas | `.data`, `.word`, `.asciiz` | `DIR_DATA`, `DIR_WORD`, `DIR_ASCIIZ` |
| Instruções | `li`, `add`, `beq`, `syscall` | `INS_LI`, `INS_ADD`, `INS_BEQ`, `INS_SYSCALL` |
| Identificadores | `main`, `laco`, `_fim` | `ID` |
| Registradores | `$t0`, `$sp`, `$31` | `REG` |
| Números | `10`, `-4`, `0xFF`, `-0x20` | `NUM_INT` |
| Cadeias | `"Resultado: "` | `STRING` |
| Símbolos | `,` `:` `(` `)` | `SMB_COM`, `SMB_COL`, `SMB_OPA`, `SMB_CPA` |
| Fim do arquivo | | `TK_EOF` |

São 10 diretivas, 36 instruções e os registradores `$zero`, `$v0`–`$v1`,
`$a0`–`$a3`, `$t0`–`$t9`, `$s0`–`$s7`, `$k0`–`$k1`, `$gp`, `$sp`, `$fp`, `$ra`
e `$0`–`$31`. Instruções, diretivas e registradores são reconhecidos com
maiúsculas ou minúsculas (`ADD` é `INS_ADD`).

### Tabela de símbolos

Começa com todas as diretivas, instruções e registradores, com linha e coluna
zeradas. Quando um deles aparece pela primeira vez no programa, a posição é
preenchida. Os identificadores entram na tabela na primeira vez que aparecem e
diferenciam maiúsculas de minúsculas, então `inicio` e `Inicio` são símbolos
diferentes. Números, strings e símbolos de pontuação não entram.

### Erros léxicos

| Erro | Exemplo |
|---|---|
| `ERRO_CARACTERE_INVALIDO` | `@`, ou um `-` que não vem colado a um número |
| `ERRO_IDENTIFICADOR_MALFORMADO` | `2valor`, `soma-total`, `inicio@` |
| `ERRO_NUMERO_MALFORMADO` | `0xG1`, `0x`, `12.5` |
| `ERRO_DIRETIVA_INVALIDA` | `.dados` |
| `ERRO_REGISTRADOR_INVALIDO` | `$t10`, `$32`, `$x1` |
| `ERRO_STRING_NAO_FECHADA` | `"Mensagem` sem aspas até o fim da linha |
| `ERRO_STRING_NAO_FECHADA_EOF` | o mesmo, mas chegando ao fim do arquivo |
| `ERRO_ESCAPE_INVALIDO` | `\q` dentro de uma string |

Um lexema com erro não vira token. O erro é registrado e a análise continua a
partir do próximo delimitador, para que o `.err` tenha todos os erros do
arquivo, e não só o primeiro.

### Exemplo

Para a entrada

```
.text
main:
    li $v0, 10
    syscall
```

o `.lex` fica

```
<DIR_TEXT, .text> 1 1
<ID, main> 2 1
<SMB_COL, :> 2 5
<INS_LI, li> 3 1
<REG, $v0> 3 4
<SMB_COM, ,> 3 7
<NUM_INT, 10> 3 9
<INS_SYSCALL, syscall> 4 1
<TK_EOF, EOF> 5 1
```

o `.ts` começa com `LEXEMA,CATEGORIA,LINHA,COLUNA` e tem uma linha por
símbolo, como `main,identificador/rótulo,2,1`, e o `.err` contém
`Nenhum erro léxico encontrado.`

## Testes

A pasta `testes/lexico` tem os casos pedidos no enunciado:

- `correto_1`, `correto_2` e `correto_3` — programas sem nenhum erro léxico;
- `erro_1`, `erro_2` e `erro_3` — programas com erros de todos os tipos;
- `comentarios`, `strings`, `numeros` e `memoria` — um para cada assunto
  (`memoria` usa o formato `deslocamento(registrador)`).

A pasta `testes/preprocessamento` tem os dez casos da etapa 1. Para rodar
todos, no macOS ou Linux:

```
make testes
```

As saídas ficam em `testes/saida`. No Windows dá para rodar um por vez:

```
main.exe testes\lexico\correto_1.asm testes\saida\correto_1.lex
```

Os outros atalhos são `make` para compilar e `make limpar` para apagar o
executável e a pasta de saídas.

## Pontos em que o enunciado deixa margem

- **Linha e coluna** são contadas no `.pre`, que é o arquivo lido pela análise
  léxica. Como o pré-processamento tira a indentação e as linhas vazias, elas
  podem ser diferentes das posições no `.asm` original.
- **Quantidade de argumentos:** o texto fala em três, mas o exemplo usa dois.
  O programa aceita as duas formas.
- **`2valor`** é registrado como identificador malformado, porque é assim que
  o enunciado classifica esse exemplo.
- **`add:`** gera `INS_ADD` e `SMB_COL` sem erro. Rejeitar uma instrução usada
  como rótulo é trabalho da análise sintática.
- **Primeira ocorrência** de um identificador pode ser uma referência: em
  `.globl main`, `main` já entra na tabela com a posição do `.globl`.
