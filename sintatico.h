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
 Lê a sequência de tokens, verifica se ela respeita a gramática e grava os
 resultados nos arquivos recebidos:

   tokens - o .lex, aberto para leitura;
   arvore - o .dot, aberto para escrita. Só recebe a árvore sintática quando
            o programa é válido; para programa inválido fica vazio;
   erros  - o .sin.err, aberto para escrita. Sempre recebe conteúdo.

 Devolve SIN_VALIDO, SIN_INVALIDO ou SIN_FALHA.

 Cada não terminal da gramática tem uma função de reconhecimento própria,
 declarada como static em sintatico.c.
*/
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);

#endif
