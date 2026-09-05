/*
 preprocessador.h

 Constantes, tipos e protótipos do módulo de pré-processamento.
 A implementacao esta em preprocessador.c.
*/

#ifndef PREPROCESSADOR_H

#define PREPROCESSADOR_H

#include <stdio.h>   /* por causa do FILE usado nos protótipos */

#define COMENTARIO '#'
#define ASPAS      '"'

/* Quebra de linha da saída. */
#define FIM_DE_LINHA "\n"

/* Tamanho inicial do buffer de uma linha que dobra sozinho ao encher. */
#define TAM_INICIAL 128

/* Códigos de retorno das funções que podem falhar */
enum {
    OK = 0,
    ERRO_ARGUMENTOS,
    ERRO_ABRIR_ENTRADA,
    ERRO_ABRIR_SAIDA,
    ERRO_MESMO_ARQUIVO,
    ERRO_LEITURA,
    ERRO_ESCRITA,
    ERRO_MEMORIA
};

/* Contadores usados só para mostrar um resumo no final */
typedef struct {
    long lidas;
    long gravadas;
    long removidas;
    long comentarios;
    long avisos;
} Estatisticas;

/* Verdadeiro para espaço, tabulação e afins */
int eh_espaco(char c);

/* Corta a linha no primeiro '#' que estiver fora de uma string (seção 2.2).
   Retorna 1 se removeu algum comentário. */
int remover_comentario(char *linha);

/* Tira os espaços das pontas e reduz os do meio a um só, sem mexer no que
   está dentro das aspas (seções 2.4 e 2.8) */
void normalizar_espacos(char *linha);

/* Verdadeiro se a linha não tem nenhum caractere útil (seção 2.3) */
int linha_vazia(const char *linha);

/* Verdadeiro se a linha termina com uma string que ficou sem fechar */
int string_aberta(const char *linha);

/* Tira o BOM do começo do arquivo, se existir */
void remover_bom(char *linha);

/* Aplica as etapas de limpeza em uma linha. Retorna 1 se ela deve ser
   gravada na saída. */
int processar_linha(char *linha, Estatisticas *est, long numero);

/* Lê uma linha aceitando "\n", "\r\n" e "\r" (seção 2.5). Devolve memória
   alocada com malloc, ou NULL no fim do arquivo. */
char *ler_linha(FILE *entrada, int *status);

/* Percorre o arquivo inteiro. Retorna OK ou um dos códigos de erro. */
int processar_arquivo(FILE *entrada, FILE *saida, Estatisticas *est);

void zerar_estatisticas(Estatisticas *est);
const char *mensagem_erro(int codigo);

#endif
