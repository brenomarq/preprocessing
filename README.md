# Pré-processador μ-Assembly

Etapa 1 do projeto de Linguagens Formais, Autômatos e Compiladores.

Grupo: *(preencher)*

O programa lê um arquivo `.asm` escrito em μ-Assembly (subconjunto do MIPS,
sintaxe do MARS) e grava uma versão limpa em `.pre`, que será a entrada do
analisador léxico na próxima etapa.

## Compilação e execução

```
gcc main.c preprocessador.c -o main.exe
main.exe teste.asm teste.pre
```

O `preprocessador.h` não precisa ir no comando, porque é incluído pelos `.c`.

São exigidos exatamente dois argumentos. Se faltar algum, se sobrar, ou se um
dos arquivos não puder ser aberto, o programa mostra o erro e encerra sem
gravar nada.

## Arquivos

- `main.c` — argumentos da linha de comando, abertura dos arquivos e mensagens.
- `preprocessador.c` — as funções de limpeza do código.
- `preprocessador.h` — constantes, tipos e protótipos.
- `testes/` — arquivos de teste e os resultados esperados.

## O que o pré-processador faz

- Remove comentários, ou seja, tudo do `#` até o fim da linha (seção 2.2).
- Remove linhas vazias e linhas que só têm espaços (2.3).
- Troca tabulações por espaço, tira os espaços do início e do fim da linha e
  reduz sequências de espaços a um só (2.4).
- Aceita arquivos com `\n`, `\r\n` ou `\r` e grava a saída sempre com `\n` (2.5).
- Preserva rótulos, diretivas e o conteúdo das strings (2.6, 2.7 e 2.8).

O ponto que exige atenção é que o `#` e os espaços só têm significado especial
**fora** das aspas. Em `msg: .asciiz "Resultado # obtido"` o `#` faz parte do
texto e continua lá.

## O que ele não faz

Nada de análise. Conforme a seção 2.10 do enunciado, esta etapa não reconhece
tokens, não valida sintaxe, não confere a quantidade de operandos nem os nomes
dos registradores, não monta tabela de símbolos e não calcula endereços.

Por isso `add $t0, $t1` (falta um operando) e `add $t0, $t1, $t99` (registrador
que não existe) passam sem erro. Quem reclama disso são as etapas seguintes.

## Exemplo

Entrada:

```
# Exemplo de programa

.data

msg:    .asciiz "Resultado # obtido"   # mensagem

.text
main:
        li      $t0, 5

        li      $t1, 10      # segundo número
        add     $t2, $t0, $t1
```

Saída:

```
.data
msg: .asciiz "Resultado # obtido"
.text
main:
li $t0, 5
li $t1, 10
add $t2, $t0, $t1
```

## Testes

Cada arquivo `.asm` da pasta `testes` tem um `.esperado` correspondente com o
resultado correto. Para rodar todos de uma vez:

```
testes\executar_testes.bat        (Windows)
sh testes/executar_testes.sh      (macOS e Linux)
```

O script compila, roda cada caso e compara a saída com o esperado. Também
testa se o programa realmente falha quando recebe argumentos errados ou um
arquivo inexistente.

Os casos cobrem comentários (01), strings (02), espaços e tabulações (03),
linhas vazias (04) e rótulos (05). O 06 é o exemplo do enunciado, o 07 e o 08
são arquivos com quebra de linha do Windows e do Mac antigo, o 09 tem código
sintaticamente errado que deve passar sem reclamação e o 10 é um arquivo vazio.

Para acrescentar um caso, basta criar `11_nome.asm` e `11_nome.esperado`; o
script encontra sozinho.

## Observações sobre a implementação

Os arquivos são abertos em modo binário (`"rb"` e `"wb"`) porque no Windows o
modo texto converteria as quebras de linha por conta própria. Quem trata isso
é a função `ler_linha()`, o que deixa o comportamento igual em qualquer
sistema.

Não há limite de tamanho de linha: o buffer começa com 128 bytes e dobra com
`realloc()` sempre que enche.

O programa recusa executar quando o arquivo de entrada e o de saída têm o mesmo
nome, senão o código-fonte seria apagado antes de ser lido.

Se uma string ficar sem fechar, o programa mostra um aviso e continua. Não pode
ser tratado como erro porque a seção 2.10 não deixa esta etapa validar sintaxe.
