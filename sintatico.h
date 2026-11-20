/*
 sintatico.h

 Constantes, tipos e protótipos do módulo de análise sintática.
 A implementação está em sintatico.c.
*/

#ifndef SINTATICO_H
#define SINTATICO_H

#include <stdio.h>

/* Retornos de AnaliseSintatica() */
#define SIN_VALIDO    0
#define SIN_INVALIDO  1
#define SIN_FALHA   (-1)

/*
 Verifica se os tokens respeitam a gramática, e grava o resultado:

   tokens - o .lex, para leitura;
   arvore - o .dot, que só recebe a árvore quando o programa é válido;
   erros  - o .sin.err, que sempre recebe conteúdo.

 Devolve SIN_VALIDO, SIN_INVALIDO ou SIN_FALHA.
*/
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);

#endif
