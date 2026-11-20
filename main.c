/*
 main.c

 Linguagens Formais, Autômatos e Compiladores
 Etapas 1, 2 e 3 - Pré-processador, analisador léxico e analisador sintático
 da linguagem u-Assembly

 Grupo: Breno Marques, Thiago de Jesus, Roberty Luann e Arthur

 Compilação:  gcc main.c preprocessador.c lexico.c sintatico.c -o main.exe
 Execução:    ./main.exe <entrada.asm> <prefixo_saida>

 De um prefixo saem seis arquivos. Com "resultado":

   resultado.pre      código normalizado pelo pré-processamento
   resultado.lex      tokens reconhecidos
   resultado.ts       tabela de símbolos
   resultado.lex.err  erros léxicos
   resultado.sin.err  erros sintáticos
   resultado.dot      árvore sintática, quando o programa é válido

 Este arquivo cuida apenas dos argumentos, dos arquivos e da ordem das
 etapas. As implementações estão em preprocessador.c, lexico.c e sintatico.c.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preprocessador.h"
#include "lexico.h"
#include "sintatico.h"

/* Códigos de encerramento exigidos pelo enunciado */
#define SAIDA_VALIDO    0
#define SAIDA_REJEITADO 1
#define SAIDA_FALHA     2

#define TOTAL_SAIDAS 6

static void exibir_uso(const char *programa) {
    fprintf(stderr, "\nUso: %s <arquivo_entrada.asm> <prefixo_saida>\n", programa);
    fprintf(stderr, "Exemplo: %s teste.asm resultado\n\n", programa);
}

static void montar_nome(const char *prefixo, const char *sufixo, char *destino) {
    snprintf(destino, FILENAME_MAX, "%s%s", prefixo, sufixo);
}

static FILE *abrir_entrada(const char *arquivo) {
    /* Modo binário nas duas pontas: no Windows o modo texto converteria as
       quebras de linha por conta própria, e quem trata isso é ler_linha(). */
    FILE *entrada = fopen(arquivo, "rb");

    if (entrada == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_ENTRADA), arquivo);
    }
    return entrada;
}

static FILE *abrir_saida(const char *arquivo) {
    /* "wb" trunca o arquivo, então resultados de execuções anteriores não
       sobrevivem e são confundidos com os atuais. */
    FILE *saida = fopen(arquivo, "wb");

    if (saida == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_SAIDA), arquivo);
    }
    return saida;
}

/* Fecha conferindo se a gravação deu certo. Devolve 0 se algo falhou. */
static int fechar_saida(FILE *saida, const char *arquivo) {
    int falhou = ferror(saida);

    if (fclose(saida) != 0) {
        falhou = 1;
    }
    if (falhou) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ESCRITA), arquivo);
        return 0;
    }
    return 1;
}

/* Etapa 1: .asm -> .pre */
static int executar_preprocessamento(const char *arquivo_asm, const char *arquivo_pre) {
    FILE *entrada;
    FILE *saida;
    int resultado;

    entrada = abrir_entrada(arquivo_asm);
    if (entrada == NULL) {
        return 0;
    }

    saida = abrir_saida(arquivo_pre);
    if (saida == NULL) {
        fclose(entrada);
        return 0;
    }

    resultado = PreProcessamento(entrada, saida);

    if (ferror(entrada)) {
        resultado = ERRO_LEITURA;
    }
    fclose(entrada);

    if (fclose(saida) != 0 && resultado == OK) {
        resultado = ERRO_ESCRITA;
    }

    if (resultado != OK) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(resultado));
        return 0;
    }
    return 1;
}

/*
 Etapa 2: .pre -> .lex, .ts e .lex.err

 Devolve a quantidade de erros léxicos encontrados, ou -1 se alguma operação
 de arquivo falhar. É por esse retorno que main() descobre se pode seguir
 para a análise sintática.
*/
static int executar_analise_lexica(const char *arquivo_pre, const char *arquivo_lex,
                                   const char *arquivo_ts, const char *arquivo_lex_err) {
    FILE *entrada;
    FILE *lex;
    FILE *ts;
    FILE *err;
    int erros;
    int ok;

    entrada = abrir_entrada(arquivo_pre);
    if (entrada == NULL) {
        return -1;
    }

    lex = abrir_saida(arquivo_lex);
    if (lex == NULL) {
        fclose(entrada);
        return -1;
    }

    AnaliseLexica(entrada, lex);

    ok = !ferror(entrada) && !ferror(lex);
    fclose(entrada);
    if (!fechar_saida(lex, arquivo_lex)) {
        ok = 0;
    }

    ts = abrir_saida(arquivo_ts);
    err = abrir_saida(arquivo_lex_err);
    if (ts == NULL || err == NULL) {
        if (ts != NULL) {
            fclose(ts);
        }
        if (err != NULL) {
            fclose(err);
        }
        LiberarLexico();
        return -1;
    }

    GravarTabelaSimbolos(ts);
    erros = GravarErros(err);
    LiberarLexico();

    if (!fechar_saida(ts, arquivo_ts)) {
        ok = 0;
    }
    if (!fechar_saida(err, arquivo_lex_err)) {
        ok = 0;
    }

    return ok ? erros : -1;
}

/* Etapa 3: .lex -> .dot e .sin.err */
static int executar_analise_sintatica(const char *arquivo_lex, const char *arquivo_dot,
                                      const char *arquivo_sin_err) {
    FILE *tokens;
    FILE *arvore;
    FILE *erros;
    int resultado;

    /* O .lex foi fechado depois de gravado e é reaberto aqui, para que a
       leitura comece do início do arquivo. */
    tokens = abrir_entrada(arquivo_lex);
    if (tokens == NULL) {
        return SIN_FALHA;
    }

    arvore = abrir_saida(arquivo_dot);
    if (arvore == NULL) {
        fclose(tokens);
        return SIN_FALHA;
    }

    erros = abrir_saida(arquivo_sin_err);
    if (erros == NULL) {
        fclose(tokens);
        fclose(arvore);
        return SIN_FALHA;
    }

    resultado = AnaliseSintatica(tokens, arvore, erros);

    fclose(tokens);
    if (!fechar_saida(arvore, arquivo_dot)) {
        resultado = SIN_FALHA;
    }
    if (!fechar_saida(erros, arquivo_sin_err)) {
        resultado = SIN_FALHA;
    }

    return resultado;
}

/*
 Quando existem erros léxicos a etapa 3 não roda, mas os dois arquivos dela
 ainda precisam existir: o .dot vazio e o .sin.err com o motivo.
*/
static int registrar_nao_execucao(const char *arquivo_dot, const char *arquivo_sin_err) {
    FILE *arvore;
    FILE *erros;
    int ok = 1;

    arvore = abrir_saida(arquivo_dot);
    if (arvore == NULL) {
        return 0;
    }
    if (!fechar_saida(arvore, arquivo_dot)) {
        ok = 0;
    }

    erros = abrir_saida(arquivo_sin_err);
    if (erros == NULL) {
        return 0;
    }
    fprintf(erros, "Análise sintática não executada: existem erros léxicos.\n");
    if (!fechar_saida(erros, arquivo_sin_err)) {
        ok = 0;
    }
    return ok;
}

int main(int argc, char *argv[]) {
    const char *arquivo_asm;
    char saidas[TOTAL_SAIDAS][FILENAME_MAX];
    const char *sufixos[TOTAL_SAIDAS] = {".pre", ".lex", ".ts", ".lex.err",
                                         ".sin.err", ".dot"};
    int erros_lexicos;
    int resultado;
    int i;

    if (argc != 3) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(ERRO_ARGUMENTOS));
        exibir_uso(argv[0]);
        return SAIDA_FALHA;
    }

    arquivo_asm = argv[1];
    for (i = 0; i < TOTAL_SAIDAS; i++) {
        montar_nome(argv[2], sufixos[i], saidas[i]);

        /* Abrir uma saída em modo escrita apaga o conteúdo, então nenhuma
           delas pode ter o nome do arquivo de entrada. */
        if (strcmp(arquivo_asm, saidas[i]) == 0) {
            fprintf(stderr, "Erro: %s\n", mensagem_erro(ERRO_MESMO_ARQUIVO));
            return SAIDA_FALHA;
        }
    }

    if (!executar_preprocessamento(arquivo_asm, saidas[0])) {
        return SAIDA_FALHA;
    }

    erros_lexicos = executar_analise_lexica(saidas[0], saidas[1], saidas[2], saidas[3]);
    if (erros_lexicos < 0) {
        return SAIDA_FALHA;
    }

    if (erros_lexicos > 0) {
        if (!registrar_nao_execucao(saidas[5], saidas[4])) {
            return SAIDA_FALHA;
        }
        printf("ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS.\n");
        printf("Erros em %s: %d\n", saidas[3], erros_lexicos);
        return SAIDA_REJEITADO;
    }

    resultado = executar_analise_sintatica(saidas[1], saidas[5], saidas[4]);

    if (resultado == SIN_FALHA) {
        return SAIDA_FALHA;
    }

    if (resultado == SIN_VALIDO) {
        printf("PROGRAMA SINTATICAMENTE VÁLIDO\n");
        printf("Árvore sintática em %s\n", saidas[5]);
        return SAIDA_VALIDO;
    }

    printf("PROGRAMA SINTATICAMENTE INVÁLIDO\n");
    printf("Diagnósticos em %s\n", saidas[4]);
    return SAIDA_REJEITADO;
}
