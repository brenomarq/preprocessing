# Pré-processador μ-Assembly (MIPS/MARS)

**Universidade Católica de Brasília — Linguagens Formais, Autômatos e Compiladores**
Projeto Compilador-Assembly · **Etapa 1: Pré-processamento**

---

## 1. O que esta entrega precisa fazer

O pré-processador é a **primeira etapa do tradutor**. Ele recebe um arquivo `.asm`
escrito em μ-Assembly (subconjunto do MIPS, sintaxe do MARS) e produz um arquivo
`.pre` **limpo e padronizado**, que será a entrada do analisador léxico na
próxima entrega.

Ele faz **apenas transformação de texto**. Não entende o programa.

| O pré-processador **FAZ** | O pré-processador **NÃO FAZ** |
|---|---|
| Remove comentários (`#` fora de string) | Reconhecer/classificar tokens |
| Remove linhas vazias | Validar sintaxe das instruções |
| Troca tabulações por espaços | Conferir quantidade de operandos |
| Remove espaços do início/fim da linha | Validar nomes de registradores |
| Reduz sequências de espaços a um só | Verificar se um rótulo existe |
| Uniformiza as quebras de linha | Montar a tabela de símbolos |
| Preserva rótulos, diretivas e strings | Calcular endereços |
| | Expandir pseudoinstruções |
| | Gerar código de máquina |

> **Consequência prática:** `add $t0, $t1` (falta operando) e `add $t0, $t1, $t99`
> (registrador inexistente) passam **sem erro** por esta etapa. Isso é exigido
> pela seção 2.10 do enunciado — quem reclama disso é a análise sintática/semântica.
> O teste `09_sem_validacao` existe justamente para provar esse comportamento.

### Exemplo (o mesmo da seção 2.9 do enunciado)

<table>
<tr><th>Entrada <code>.asm</code></th><th>Saída <code>.pre</code></th></tr>
<tr><td><pre>
# Exemplo de programa

.data

msg:    .asciiz "Resultado # obtido"   # mensagem

.text
main:
        li      $t0, 5

        li      $t1, 10      # segundo número
        add     $t2, $t0, $t1
</pre></td><td><pre>
.data
msg: .asciiz "Resultado # obtido"
.text
main:
li $t0, 5
li $t1, 10
add $t2, $t0, $t1
</pre></td></tr>
</table>

Repare que o `#` **dentro da string** foi preservado, enquanto o `#` do
comentário foi removido. Essa distinção é o coração do trabalho.

---

## 2. Estrutura do projeto

```
preprocessing/
├── main.c                  ← argumentos, arquivos, mensagens (só isso)
├── preprocessador.c        ← toda a lógica de transformação
├── preprocessador.h        ← constantes, tipos e protótipos
├── Makefile                ← atalhos para macOS/Linux
├── README.md               ← este arquivo
├── docs/
│   └── FUNCIONAMENTO.md    ← explicação detalhada do algoritmo
└── testes/
    ├── 01_comentarios.asm  + .esperado
    ├── 02_strings.asm      + .esperado
    ├── ... (10 casos)
    ├── executar_testes.sh  ← bateria de testes (macOS/Linux)
    └── executar_testes.bat ← bateria de testes (Windows)
```

A divisão em três arquivos é **obrigatória** pelo enunciado (seção 2.12).
A regra que seguimos para decidir o que vai onde:

- `main.c` — conversa com o **sistema operacional** (linha de comando, arquivos).
- `preprocessador.c` — conversa com o **texto** (transformações).
- `preprocessador.h` — o **contrato** entre os dois.

Se alguém precisar mudar como o programa é chamado, mexe só no `main.c`.
Se precisar mudar uma regra de limpeza, mexe só no `preprocessador.c`.

---

## 3. Como compilar e executar

### Windows (ambiente exigido pelo professor)

```bat
gcc main.c preprocessador.c -o main.exe
main.exe teste.asm teste.pre
```

### macOS / Linux (para desenvolver)

```bash
make
./main.exe teste.asm teste.pre
```

> `preprocessador.h` **não** entra no comando de compilação: ele é incluído
> pelos `.c` através de `#include "preprocessador.h"`.

O programa exige **exatamente dois argumentos**. Se faltar, sobrar, ou se algum
arquivo não puder ser aberto, ele mostra uma mensagem clara e encerra com código
de erro — sem travar e sem deixar arquivo pela metade.

```
$ ./main.exe
Erro: Quantidade de argumentos invalida.
Esperados 2 argumentos, recebidos 0.

Pre-processador u-Assembly (MIPS/MARS)
--------------------------------------
Uso:
  ./main.exe <arquivo_entrada.asm> <arquivo_saida.pre>
...
```

### Saída normal

```
$ ./main.exe testes/06_exemplo_enunciado.asm saida.pre
Pre-processamento concluido com sucesso.
  Entrada ..............: testes/06_exemplo_enunciado.asm
  Saida ................: saida.pre
  Linhas lidas .........: 12
  Linhas gravadas ......: 7
  Linhas removidas .....: 5
  Comentarios removidos : 3
```

---

## 4. Como rodar os testes

```bash
make testes                    # macOS / Linux
```
```bat
testes\executar_testes.bat     REM Windows
```

O script compila, roda os **10 casos de pré-processamento** e os **5 casos de
erro**, e compara byte a byte com o resultado esperado:

```
OK      01_comentarios
OK      02_strings
...
Resultado: 15 de 15 testes passaram
```

| Teste | O que cobre (item do enunciado) |
|---|---|
| `01_comentarios` | 2.2 — comentário no fim da linha, linha só de comentário, `#` dentro de string, `#` colado na instrução |
| `02_strings` | 2.8 — espaços múltiplos, tabulação, `:` `,` `#` dentro da string, string vazia, aspas escapadas `\"` |
| `03_espacos` | 2.4 — tabulações, espaços no início/fim, sequências de espaços |
| `04_linhas_vazias` | 2.3 — linha em branco, linha só com espaços/tabs, linha que fica vazia após remover o comentário |
| `05_rotulos` | 2.6 e 2.7 — rótulos em `.data` e `.text`, rótulo sozinho na linha, identificador com `_`, diretivas preservadas |
| `06_exemplo_enunciado` | 2.9 — reproduz exatamente o exemplo do PDF |
| `07_quebras_windows` | 2.5 — arquivo com CRLF (`\r\n`) |
| `08_quebras_mac_antigo` | 2.5 — arquivo com CR (`\r`) e sem quebra no final |
| `09_sem_validacao` | 2.10 — código sintaticamente errado deve passar sem erro |
| `10_arquivo_vazio` | caso limite — arquivo de entrada vazio |

**Para adicionar um teste novo:** crie `testes/11_algo.asm` e
`testes/11_algo.esperado` com o resultado desejado. O script encontra sozinho.

---

## 5. Mapa: enunciado → código

Cada exigência do PDF tem uma função dedicada. Use esta tabela para achar
qualquer coisa e para responder na apresentação.

| Seção do enunciado | Função responsável | Arquivo |
|---|---|---|
| 2.1 Entrada (abrir arquivos, argumentos) | `main()` | `main.c` |
| 2.2 Remoção de comentários | `pp_remover_comentario()` | `preprocessador.c` |
| 2.3 Remoção de linhas vazias | `pp_esta_vazia()` | `preprocessador.c` |
| 2.4 Normalização de espaços e tabulações | `pp_normalizar_espacos()` | `preprocessador.c` |
| 2.5 Normalização das quebras de linha | `pp_ler_linha()` + `PP_FIM_DE_LINHA` | `preprocessador.c` / `.h` |
| 2.6 Preservação de rótulos | *(consequência: nada os remove)* | — |
| 2.7 Preservação das diretivas | *(consequência: nada as interpreta)* | — |
| 2.8 Preservação de strings | estado `dentro_string` nas duas funções acima | `preprocessador.c` |
| 2.9 Saída esperada | `pp_processar_arquivo()` | `preprocessador.c` |
| 2.10 Delimitação da etapa | *(consequência: não existe função de validação)* | — |
| 2.12 Requisitos (3 arquivos, CLI, erros) | `main()` + `pp_mensagem_erro()` | `main.c` / `preprocessador.c` |

> **Rótulos e diretivas são preservados por construção**, não por uma função
> específica: como o pré-processador só corta comentários e ajusta brancos,
> `main:` e `.asciiz` simplesmente atravessam o processo intactos. Se alguém
> perguntar "onde está o código que preserva os rótulos?", a resposta certa é
> *"não existe — preservar é não fazer nada, e é isso que o enunciado pede
> nesta etapa"*.

---

## 6. Decisões de projeto (e por que tomamos cada uma)

Estas são as perguntas que o professor pode fazer. Todas têm resposta no código.

**1. Por que ler os arquivos em modo binário (`"rb"` / `"wb"`)?**
No Windows, o modo texto converte quebras de linha automaticamente e de forma
invisível. Como o enunciado (2.5) exige tratar CRLF, LF e CR, preferimos fazer
isso **explicitamente** em `pp_ler_linha()`. Assim o programa se comporta
igual em qualquer sistema operacional.

**2. Por que a saída usa `\n` e não `\r\n`?**
"Representação uniforme" é o que o enunciado pede; escolhemos LF. A constante
`PP_FIM_DE_LINHA` no `.h` centraliza a decisão — trocar para `"\r\n"` é uma
linha só.

**3. Por que remover o comentário *antes* de normalizar os espaços?**
Porque normalizar um texto que será descartado é trabalho jogado fora, e porque
uma aspas perdida dentro de um comentário poderia confundir a etapa seguinte.
A ordem correta está documentada em `pp_processar_linha()`.

**4. Por que uma função `pp_eh_branco()` em vez de `isspace()` da `<ctype.h>`?**
`isspace()` recebe um `int` e tem **comportamento indefinido** para valores
negativos. Um `char` com acento em UTF-8 (o `ç` de `"operação"`, por exemplo)
vira um valor negativo. Nossa função compara caracteres diretamente e não corre
esse risco.

**5. O que acontece com uma tabulação *dentro* de uma string?**
Ela é preservada. As seções 2.4 e 2.8 dizem que o conteúdo da string não pode
ser alterado, e no MARS um tab dentro de `.asciiz` é um byte de tab de verdade.
Coberto pelo teste `02_strings`.

**6. Por que tratar `\"` (aspas escapada) dentro da string?**
Para que `"ele disse \"ola\""` não seja lido como duas strings separadas. O
enunciado não exige, mas sem isso um arquivo válido no MARS seria corrompido.

**7. E se o programador esquecer de fechar uma string?**
O programa **avisa** no console (`stderr`) e **continua**, mantendo a linha como
está. Não é erro: a seção 2.10 proíbe o pré-processador de validar sintaxe.
O aviso existe só para poupar tempo de depuração.

**8. Por que o programa recusa entrada e saída com o mesmo nome?**
Porque abrir a saída em modo escrita **apaga o arquivo** antes da leitura
começar. Sem essa proteção, `main.exe teste.asm teste.asm` destruiria o
código-fonte do usuário.

**9. Existe limite de tamanho de linha?**
Não. `pp_ler_linha()` começa com um buffer de 128 bytes e **dobra** o tamanho
com `realloc()` sempre que enche. Testado com linhas de 500+ caracteres.

**10. Por que remover o BOM?**
O Bloco de Notas do Windows salva UTF-8 começando com três bytes invisíveis
(`EF BB BF`). Sem removê-los, a primeira linha do arquivo chegaria suja ao
analisador léxico e `.data` não seria reconhecido.

**11. Por que as mensagens do programa não têm acento?**
O console do Windows (`cmd.exe`) usa por padrão uma página de código que exibe
acentos UTF-8 como lixo. Os comentários do código têm acento normalmente; só as
mensagens impressas são ASCII puro.

---

## 7. Checklist da entrega

- [x] Escrito em C, compilando com GCC (`-Wall -Wextra -pedantic`, sem avisos)
- [x] Modularizado em `main.c`, `preprocessador.c`, `preprocessador.h`
- [x] Executado por linha de comando com dois argumentos
- [x] Mensagem de erro clara e encerramento controlado em todas as falhas
- [x] Remoção de comentários respeitando strings
- [x] Remoção de linhas vazias
- [x] Normalização de espaços e tabulações
- [x] Normalização de quebras de linha (LF, CRLF, CR)
- [x] Preservação de rótulos, diretivas e strings
- [x] Nenhuma validação léxica/sintática (respeitando a seção 2.10)
- [x] Casos de teste para comentários, strings, espaços, linhas vazias e rótulos

---

## 8. Próximas etapas do projeto

O arquivo `.pre` gerado aqui é a entrada do **analisador léxico** (etapa 2), que
transformará cada linha em tokens:

```
main:            →  IDENTIFICADOR("main")  DOIS_PONTOS
li $t0, 10       →  PSEUDOINSTRUCAO("li")  REGISTRADOR("$t0")  VIRGULA  INTEIRO("10")
```

Por isso o `.pre` precisa estar limpo: quanto menos ruído, mais simples fica o
autômato do lexer.

📖 **Para entender o algoritmo em detalhe, leia [docs/FUNCIONAMENTO.md](docs/FUNCIONAMENTO.md).**
