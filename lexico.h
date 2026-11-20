/*
 lexico.h

 Constantes, tipos e protótipos do módulo de análise léxica.
 A implementação está em lexico.c.
*/

#ifndef LEXICO_H
#define LEXICO_H

#include <stdio.h>

#define TAM_NOME      50
#define TAM_LEXEMA    100
#define TAM_CATEGORIA 30

typedef struct {
    char nome[TAM_NOME];
    char lexema[TAM_LEXEMA];
    int linha;
    int coluna;
} Token;

/* Uma entrada da tabela de símbolos. As palavras reservadas entram com
   linha e coluna zeradas, preenchidas no primeiro uso. */
typedef struct {
    char lexema[TAM_LEXEMA];
    char categoria[TAM_CATEGORIA];
    int primeiraLinha;
    int primeiraColuna;
} Simbolo;

/* Prepara a leitura do arquivo e carrega a tabela de símbolos com as
   palavras reservadas. Chamada automaticamente por AnaliseLexica(). */
void IniciarLexico(FILE *in);

/* Devolve o próximo token, e TK_EOF no fim. Os erros do caminho não viram
   token: ficam guardados para GravarErros(). */
Token ProximoToken(void);

/* Lê o arquivo pré-processado inteiro e grava os tokens em out. */
void AnaliseLexica(FILE *in, FILE *out);

/* Gravam a tabela de símbolos (CSV) e a lista de erros, depois de
   AnaliseLexica(). Retornam quantas linhas gravaram. */
int GravarTabelaSimbolos(FILE *ts);
int GravarErros(FILE *err);

/* Libera a memória da tabela de símbolos e da lista de erros. */
void LiberarLexico(void);

#endif
