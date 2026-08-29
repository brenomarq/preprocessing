/* ==========================================================================
 * preprocessador.h
 * --------------------------------------------------------------------------
 * Universidade Catolica de Brasilia
 * Linguagens Formais, Automatos e Compiladores
 * Projeto Compilador-Assembly - Etapa 1: PRE-PROCESSADOR
 *
 * Este cabecalho reune TODAS as declaracoes do modulo de pre-processamento:
 * constantes, tipos, codigos de erro e os prototipos das funcoes que estao
 * implementadas em preprocessador.c.
 *
 * Regra de ouro do modulo: o pre-processador so faz TRANSFORMACAO TEXTUAL.
 * Ele nao reconhece tokens, nao valida sintaxe, nao confere registradores e
 * nao monta tabela de simbolos. Isso e trabalho das etapas seguintes.
 * ========================================================================== */

#ifndef PREPROCESSADOR_H
#define PREPROCESSADOR_H

#include <stdio.h>   /* necessario por causa do tipo FILE usado nos prototipos */

/* --------------------------------------------------------------------------
 * 1. CONSTANTES DA LINGUAGEM u-Assembly
 * -------------------------------------------------------------------------- */

/* Caractere que inicia um comentario no MIPS/MARS.
   Vale apenas FORA de strings (ver secao 2.2 do enunciado). */
#define PP_CARACTERE_COMENTARIO '#'

/* Caractere que abre e fecha uma cadeia de caracteres (string). */
#define PP_ASPAS '"'

/* Contrabarra: dentro de uma string, ela "escapa" o proximo caractere.
   Serve para que "diga \"ola\"" nao seja interpretado como duas strings. */
#define PP_ESCAPE '\\'

/* Terminador de linha usado na SAIDA.
   O enunciado (secao 2.5) exige uma representacao UNIFORME de quebra de linha.
   Adotamos LF ("\n"), que e o padrao Unix e e aceito por qualquer editor
   moderno no Windows. Para gerar CRLF, troque por "\r\n" - e so mudar aqui,
   nenhum outro arquivo precisa ser alterado. */
#define PP_FIM_DE_LINHA "\n"

/* Tamanho inicial do buffer de leitura de uma linha. O buffer CRESCE sozinho
   (realloc) quando a linha e maior, entao nao existe limite de tamanho. */
#define PP_TAM_INICIAL_LINHA 128

/* --------------------------------------------------------------------------
 * 2. CODIGOS DE RETORNO
 * --------------------------------------------------------------------------
 * Toda funcao que pode falhar devolve um destes codigos. A funcao
 * pp_mensagem_erro() traduz o codigo para um texto legivel.
 * -------------------------------------------------------------------------- */
typedef enum {
    PP_OK = 0,               /* tudo certo                                     */
    PP_ERRO_ARGUMENTOS,      /* quantidade de argumentos diferente da esperada */
    PP_ERRO_ABRIR_ENTRADA,   /* nao foi possivel abrir o arquivo .asm          */
    PP_ERRO_ABRIR_SAIDA,     /* nao foi possivel criar o arquivo .pre          */
    PP_ERRO_MESMO_ARQUIVO,   /* entrada e saida apontam para o mesmo arquivo   */
    PP_ERRO_LEITURA,         /* falha de leitura no meio do arquivo            */
    PP_ERRO_ESCRITA,         /* falha de escrita no meio do arquivo            */
    PP_ERRO_MEMORIA          /* malloc/realloc devolveu NULL                   */
} PPCodigo;

/* --------------------------------------------------------------------------
 * 3. ESTATISTICAS
 * --------------------------------------------------------------------------
 * Contadores preenchidos durante o processamento. Nao sao exigidos pelo
 * enunciado, mas ajudam muito a conferir o resultado e a apresentar o
 * trabalho ("removi 12 comentarios e 9 linhas vazias").
 * -------------------------------------------------------------------------- */
typedef struct {
    long linhas_lidas;           /* linhas lidas do arquivo de entrada          */
    long linhas_gravadas;        /* linhas escritas no arquivo de saida         */
    long linhas_removidas;       /* linhas descartadas por ficarem vazias       */
    long comentarios_removidos;  /* linhas em que um comentario foi cortado     */
    long avisos;                 /* strings abertas e nao fechadas na linha     */
} PPEstatisticas;

/* --------------------------------------------------------------------------
 * 4. PROTOTIPOS
 * --------------------------------------------------------------------------
 * As funcoes estao ordenadas do nivel mais baixo (um caractere) para o mais
 * alto (o arquivo inteiro). Cada uma resolve UMA tarefa do enunciado, o que
 * permite testar cada etapa isoladamente.
 * -------------------------------------------------------------------------- */

/* --- 4.1 Utilitarios de caractere ------------------------------------------ */

/* Informa se 'c' e um espaco em branco horizontal (espaco, tab, etc.).
   Usamos esta funcao no lugar de isspace() da <ctype.h> de proposito:
   isspace() com um char negativo (acontece com acentos em UTF-8, como em
   "operacao") tem comportamento indefinido. Aqui nao ha esse risco. */
int pp_eh_branco(char c);

/* --- 4.2 Etapas do pre-processamento (atuam sobre UMA linha) --------------- */

/* Secao 2.2 - Remocao de comentarios.
   Corta a linha a partir do primeiro '#' que estiver FORA de uma string.
   Um '#' dentro de aspas e conteudo e permanece intocado.
   A linha e modificada no proprio lugar (in place).
   Retorna 1 se algum comentario foi removido, 0 caso contrario. */
int pp_remover_comentario(char *linha);

/* Secoes 2.4 e 2.8 - Normalizacao de espacos e preservacao de strings.
   Fora das aspas: troca tabulacoes por espaco, remove espacos do inicio e do
   fim e reduz sequencias de brancos a um unico espaco.
   Dentro das aspas: copia tudo exatamente como esta.
   A linha e modificada no proprio lugar (in place). */
void pp_normalizar_espacos(char *linha);

/* Secao 2.3 - Remocao de linhas vazias.
   Retorna 1 se a linha nao possui nenhum caractere util (vazia ou so brancos). */
int pp_esta_vazia(const char *linha);

/* Diagnostico opcional: retorna 1 se a linha termina com uma string que foi
   aberta e nao fechada. Nao e erro segundo o enunciado (secao 2.10), por isso
   apenas geramos um aviso, sem interromper o processamento. */
int pp_tem_string_aberta(const char *linha);

/* Remove a marca BOM (EF BB BF) que o Bloco de Notas do Windows costuma
   colocar no inicio de arquivos UTF-8. Sem isso, a primeira linha comecaria
   com tres bytes invisiveis e atrapalharia o analisador lexico. */
void pp_remover_bom(char *linha);

/* Aplica, na ordem correta, todas as transformacoes de uma linha.
   E o "miolo" do pre-processador e a funcao mais facil de testar sozinha.
   Retorna 1 se a linha deve ser gravada, 0 se deve ser descartada. */
int pp_processar_linha(char *linha, PPEstatisticas *est, long numero_linha);

/* --- 4.3 Leitura e processamento do arquivo -------------------------------- */

/* Secao 2.5 - Normalizacao das quebras de linha.
   Le UMA linha do arquivo, aceitando os tres formatos existentes:
   LF ("\n", Unix/macOS), CRLF ("\r\n", Windows) e CR sozinho ("\r", Mac antigo).
   O terminador NAO faz parte da string devolvida.

   Devolve um bloco alocado com malloc() - quem chama precisa dar free().
   Devolve NULL no fim do arquivo ou em caso de erro; o motivo vai em *status. */
char *pp_ler_linha(FILE *entrada, int *status);

/* Le todo o arquivo de entrada, aplica o pre-processamento linha a linha e
   grava o resultado no arquivo de saida. Retorna um dos codigos PPCodigo. */
int pp_processar_arquivo(FILE *entrada, FILE *saida, PPEstatisticas *est);

/* --- 4.4 Apoio -------------------------------------------------------------- */

/* Zera todos os contadores da estrutura de estatisticas. */
void pp_zerar_estatisticas(PPEstatisticas *est);

/* Traduz um codigo PPCodigo para uma mensagem de erro em portugues. */
const char *pp_mensagem_erro(int codigo);

#endif /* PREPROCESSADOR_H */
