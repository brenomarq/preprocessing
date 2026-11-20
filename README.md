# Analisador sintático μ-Assembly

Etapas 1, 2 e 3 do projeto de Linguagens Formais, Autômatos e Compiladores.

Grupo: Breno Marques, Thiago de Jesus, Roberty Luann e Arthur

O programa recebe um arquivo `.asm` escrito em μ-Assembly e executa três
etapas em sequência: pré-processamento, análise léxica e análise sintática.

## Compilação e execução

```
gcc main.c preprocessador.c lexico.c sintatico.c -o main.exe
main.exe teste.asm resultado
```

Os arquivos `.h` não precisam ir no comando, porque são incluídos pelos `.c`.

O segundo argumento é um **prefixo**, e dele saem seis arquivos:

| Arquivo | Conteúdo |
|---|---|
| `resultado.pre` | código normalizado pelo pré-processamento |
| `resultado.lex` | tokens reconhecidos |
| `resultado.ts` | tabela de símbolos |
| `resultado.lex.err` | erros léxicos |
| `resultado.sin.err` | erros sintáticos |
| `resultado.dot` | árvore sintática, quando o programa é válido |

Ao terminar, o programa informa no terminal se o programa é válido, e encerra
com um destes códigos:

| Código | Situação |
|---|---|
| 0 | programa sintaticamente válido |
| 1 | rejeitado por erros léxicos ou sintáticos |
| 2 | falha operacional ou uso incorreto da linha de comando |

Se existirem erros léxicos, a análise sintática não roda: o programa avisa
`ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS.`, deixa o `.dot`
vazio e registra o motivo no `.sin.err`.

## Arquivos

- `main.c` — argumentos, abertura dos arquivos e a ordem das etapas.
- `preprocessador.c` e `preprocessador.h` — etapa 1.
- `lexico.c` e `lexico.h` — etapa 2.
- `sintatico.c` e `sintatico.h` — etapa 3.
- `docs/afd.dot` e `docs/afd.png` — o autômato do analisador léxico.
- `docs/arvore.png` — uma árvore sintática renderizada pelo Graphviz.
- `docs/lexico.md` e `docs/sintatico.md` — explicação do código das etapas 2 e 3.
- `docs/relatorio.md` e `docs/relatorio.pdf` — o relatório técnico da etapa 2.
- `testes/` — arquivos `.asm` das três etapas.

## Pré-processamento

É a etapa 1, feita pela função `PreProcessamento()`. Ela remove comentários,
linhas vazias e espaços desnecessários, troca tabulações por espaço, aceita
arquivos com quebra de linha de qualquer sistema e grava tudo com `\n`. O `#` e
os espaços só são mexidos fora das aspas, então o conteúdo das strings é
preservado. Nenhuma validação é feita nessa etapa.

## Análise léxica

É a etapa 2, feita pela função `AnaliseLexica()`. Ela lê o `.pre` caractere por
caractere usando um autômato finito determinístico (diagrama em
`docs/afd.png`), agrupa os caracteres em lexemas e classifica cada um.

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

A tabela de símbolos começa com as 109 palavras reservadas (10 diretivas, 36
instruções e os registradores) e recebe os identificadores conforme eles
aparecem. Os oito tipos de erro léxico vão para o `.lex.err` no formato
`<tipo, lexema> linha coluna`. Os detalhes estão em `docs/lexico.md`.

## Análise sintática

É a etapa 3, feita pela função `AnaliseSintatica()`. Ela lê o `.lex`,
reconstrói os tokens e verifica se a sequência respeita a gramática, usando
análise descendente recursiva: há uma função para cada um dos 32 não terminais,
e elas se chamam conforme as produções.

### O que é verificado

- A organização do programa em seções, que precisa ter pelo menos uma.
- Que instruções só aparecem em `.text` e diretivas de dados só em `.data`.
- A quantidade, a ordem e a categoria dos operandos de cada instrução.
- A presença de vírgulas, dois-pontos e parênteses.
- Que a entrada é consumida inteira até o `TK_EOF`.

Não são verificados, por pertencerem à análise semântica: rótulos
referenciados que não existem, rótulos declarados duas vezes, limites
numéricos dos operandos e códigos de serviço do `syscall`.

### Diagnósticos e recuperação

Cada erro vai para o `.sin.err` em duas linhas, com a localização, o que foi
encontrado e o que era esperado:

```
Linha 4, coluna 9:
Encontrado REG("$t1"); esperado SMB_COM(",").
```

Depois de um erro, o analisador descarta o resto da linha e retoma na linha
seguinte, preservando a seção ativa. Por isso erros em linhas diferentes são
todos encontrados na mesma execução. Qualquer erro torna o programa inválido,
mesmo que a recuperação permita reconhecer o resto.

### Árvore sintática

Para programas válidos é gravada a árvore sintática concreta em formato DOT,
com `<programa>` na raiz, os não terminais nos nós internos e os terminais nas
folhas, incluindo `EOL` e `TK_EOF`. Para programas inválidos o `.dot` fica
vazio, nunca com uma árvore parcial.

Para gerar a imagem:

```
dot -Tpng resultado.dot -o resultado.png
```

Um exemplo pronto está em `docs/arvore.png`.

## Testes

A pasta `testes/sintatico` tem os casos da etapa 3:

- `valido_1`, `valido_2`, `valido_3` — programas sem erro, cobrindo as três
  formas de organização, todas as diretivas e as 36 instruções;
- `valido_4_limites` — rótulo inexistente e rótulo declarado duas vezes, que
  são válidos nesta etapa;
- `valido_5_sem_quebra_final` — arquivo sem quebra de linha no fim;
- `invalido_1_operandos` — operandos ausentes, excedentes, fora de ordem e de
  categoria errada, além de token sobrando depois de uma instrução completa;
- `invalido_2_pontuacao` — vírgula, dois-pontos e parênteses ausentes e lista
  de inteiros terminada por vírgula;
- `invalido_3_secoes` — instrução em `.data`, diretiva de dados em `.text` e
  linha fora de qualquer seção;
- `invalido_4_recuperacao` — erros em linhas distintas, com as construções
  seguintes reconhecidas;
- `invalido_5_vazio` — arquivo vazio, que não tem a seção exigida.

As pastas `testes/lexico` e `testes/preprocessamento` têm os casos das etapas
anteriores, que continuam servindo de regressão. Para rodar todos:

```
make testes
```

As saídas ficam em `testes/saida`. No Windows dá para rodar um por vez:

```
main.exe testes\sintatico\valido_1.asm testes\saida\valido_1
```

Os outros atalhos são `make` para compilar e `make limpar` para apagar o
executável e a pasta de saídas.

## Pontos em que o enunciado deixa margem

- **Linha e coluna** são contadas no `.pre`, que é o arquivo que as etapas
  seguintes leem. Como o pré-processamento tira a indentação e as linhas
  vazias, elas podem ser diferentes das posições no `.asm` original.
- **`add:`** é recusado na análise sintática. O léxico classifica `add` como
  `INS_ADD`, e não como `ID`, então `<linha_codigo>` segue pelo caminho da
  instrução e o `:` não encaixa em nenhuma produção. O diagnóstico sai como
  `Encontrado SMB_COL(":"); esperado REG`.
- **Primeira ocorrência** de um identificador pode ser uma referência: em
  `.globl main`, `main` já entra na tabela de símbolos com a posição do
  `.globl`.
- **O `.sin.err` é sempre gravado**, inclusive quando não há erro nenhum, caso
  em que contém `Nenhum erro sintático encontrado.`
