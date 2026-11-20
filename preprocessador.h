/*
 preprocessador.h

 Constantes, tipos e protótipos do módulo de pré-processamento.
 A implementação esta em preprocessador.c.
*/

#ifndef PREPROCESSADOR_H

#define PREPROCESSADOR_H

#include <stdio.h>

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

/* Retorna verdadeiro para espaço, tabulação e afins */
int eh_espaco(char c);

/* Corta a linha no primeiro '#' que estiver fora de uma string.
   Retorna 1 se removeu algum comentário. */
int remover_comentario(char *linha);

/* Tira os espaços das pontas e reduz os do meio a um só, sem mexer nas
   aspas. */
void normalizar_espacos(char *linha);

/* Retorna verdadeiro se a linha não tem nenhum caractere útil. */
int linha_vazia(const char *linha);

/* Aplica as etapas de limpeza em uma linha. Retorna 1 se ela deve ser
   gravada na saída. */
int processar_linha(char *linha);

/* Lê uma linha aceitando "\n", "\r\n" e "\r". Devolve memória do malloc,
   ou NULL no fim do arquivo. */
char *ler_linha(FILE *entrada, int *status);

/* Função pedida no enunciado. Lê o .asm inteiro e grava a versão
   normalizada em out. Retorna OK ou um código de erro. */
int PreProcessamento(FILE *in, FILE *out);

const char *mensagem_erro(int codigo);

#endif
