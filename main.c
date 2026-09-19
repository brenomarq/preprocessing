/*
 main.c

 Linguagens Formais, Autômatos e Compiladores
 Etapas 1 e 2 - Pré-processador e analisador léxico da linguagem u-Assembly

 Grupo: Breno Marques, Thiago de Jesus, Roberty Luann e Arthur

 Compilação:  gcc main.c preprocessador.c lexico.c -o main.exe
 Execução:    ./main.exe entrada.asm saida.lex
          ou  ./main.exe entrada.asm saida.pre saida.lex

 Na forma com dois nomes, o .pre, o .ts e o .err recebem o mesmo nome do
 .lex, trocando só a extensão.

 Este arquivo cuida apenas dos argumentos, dos arquivos e das mensagens.
 O pré-processamento está em preprocessador.c e a análise léxica em lexico.c.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preprocessador.h"
#include "lexico.h"

static void exibir_uso(const char *programa) {
    fprintf(stderr, "\nUso: %s <entrada.asm> <saida.lex>\n", programa);
    fprintf(stderr, "  ou %s <entrada.asm> <saida.pre> <saida.lex>\n", programa);
    fprintf(stderr, "Exemplo: %s teste.asm teste.lex\n\n", programa);
}

/* Copia o nome trocando a extensão: "testes/a.lex" vira "testes/a.pre".
   Se o nome não tiver extensão, ela é só acrescentada no final. */
static void trocar_extensao(const char *nome, const char *extensao, char *destino) {
    const char *ponto = strrchr(nome, '.');
    const char *barra = strrchr(nome, '/');
    const char *contrabarra = strrchr(nome, '\\');
    size_t tamanho = strlen(nome);

    if (barra == NULL || (contrabarra != NULL && contrabarra > barra)) {
        barra = contrabarra;
    }

    // um ponto antes da última barra é nome de pasta, não extensão
    if (ponto != NULL && (barra == NULL || ponto > barra)) {
        tamanho = (size_t) (ponto - nome);
    }

    snprintf(destino, FILENAME_MAX, "%.*s%s", (int) tamanho, nome, extensao);
}

/* Nenhum dos arquivos pode ter o nome de outro. Abrir uma saída em modo
   escrita apaga o conteúdo, então um nome repetido destruiria a entrada. */
static int nomes_repetidos(const char *nomes[], int quantidade) {
    int i, j;

    for (i = 0; i < quantidade; i++) {
        for (j = i + 1; j < quantidade; j++) {
            if (strcmp(nomes[i], nomes[j]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int executar_preprocessamento(const char *arquivo_asm, const char *arquivo_pre) {
    FILE *entrada;
    FILE *saida;
    int resultado;

    /* Modo binário para o Windows, para evitar que o modo texto converta as
       quebras de linha sozinho. A função ler_linha() é responsável por isso. */
    entrada = fopen(arquivo_asm, "rb");
    if (entrada == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_ENTRADA), arquivo_asm);
        return 0;
    }

    saida = fopen(arquivo_pre, "wb");
    if (saida == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_SAIDA), arquivo_pre);
        fclose(entrada);
        return 0;
    }

    resultado = PreProcessamento(entrada, saida);

    fclose(entrada);

    // O fclose da saída pode falhar, caso ainda tenha dados no buffer.
    if (fclose(saida) != 0 && resultado == OK) {
        resultado = ERRO_ESCRITA;
    }

    if (resultado != OK) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(resultado));
        remove(arquivo_pre);
        return 0;
    }
    return 1;
}

static int executar_analise_lexica(const char *arquivo_pre, const char *arquivo_lex) {
    FILE *entrada;
    FILE *saida;
    int resultado = OK;

    entrada = fopen(arquivo_pre, "rb");
    if (entrada == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_ENTRADA), arquivo_pre);
        return 0;
    }

    saida = fopen(arquivo_lex, "wb");
    if (saida == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_SAIDA), arquivo_lex);
        fclose(entrada);
        return 0;
    }

    AnaliseLexica(entrada, saida);

    // AnaliseLexica() não devolve nada, então os erros de leitura e
    // gravação são conferidos aqui.
    if (ferror(entrada)) {
        resultado = ERRO_LEITURA;
    } else if (ferror(saida)) {
        resultado = ERRO_ESCRITA;
    }

    fclose(entrada);
    if (fclose(saida) != 0 && resultado == OK) {
        resultado = ERRO_ESCRITA;
    }

    if (resultado != OK) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(resultado));
        remove(arquivo_lex);
        return 0;
    }
    return 1;
}

/* Grava o .ts ou o .err. A tabela de símbolos e os erros ficam em memória
   depois da análise, e a função recebida é quem sabe escrevê-los.
   Retorna quantas linhas foram gravadas, ou -1 se falhar. */
static int gravar(const char *arquivo, int (*funcao_gravar)(FILE *)) {
    FILE *saida;
    int quantidade;
    int falhou;

    saida = fopen(arquivo, "wb");
    if (saida == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_SAIDA), arquivo);
        return -1;
    }

    quantidade = funcao_gravar(saida);

    falhou = ferror(saida);
    if (fclose(saida) != 0) {
        falhou = 1;
    }

    if (falhou) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ESCRITA), arquivo);
        remove(arquivo);
        return -1;
    }
    return quantidade;
}

int main(int argc, char *argv[]) {
    const char *arquivo_asm;
    const char *arquivo_lex;
    char arquivo_pre[FILENAME_MAX];
    char arquivo_ts[FILENAME_MAX];
    char arquivo_err[FILENAME_MAX];
    const char *nomes[5];
    int simbolos;
    int erros;

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(ERRO_ARGUMENTOS));
        exibir_uso(argv[0]);
        return EXIT_FAILURE;
    }

    /*
     O texto do enunciado fala em três argumentos, mas o formato e o exemplo
     usam dois (main.exe teste.asm teste.lex). Aceitamos as duas formas.
    */
    arquivo_asm = argv[1];
    arquivo_lex = argv[argc - 1];

    if (argc == 4) {
        snprintf(arquivo_pre, sizeof(arquivo_pre), "%s", argv[2]);
    } else {
        trocar_extensao(arquivo_lex, ".pre", arquivo_pre);
    }
    trocar_extensao(arquivo_lex, ".ts", arquivo_ts);
    trocar_extensao(arquivo_lex, ".err", arquivo_err);

    nomes[0] = arquivo_asm;
    nomes[1] = arquivo_pre;
    nomes[2] = arquivo_lex;
    nomes[3] = arquivo_ts;
    nomes[4] = arquivo_err;

    if (nomes_repetidos(nomes, 5)) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(ERRO_MESMO_ARQUIVO));
        return EXIT_FAILURE;
    }

    // Etapa 1: .asm -> .pre
    if (!executar_preprocessamento(arquivo_asm, arquivo_pre)) {
        return EXIT_FAILURE;
    }

    // Etapa 2: .pre -> .lex, e depois a tabela de símbolos e os erros
    if (!executar_analise_lexica(arquivo_pre, arquivo_lex)) {
        LiberarLexico();
        return EXIT_FAILURE;
    }

    simbolos = gravar(arquivo_ts, GravarTabelaSimbolos);
    erros = gravar(arquivo_err, GravarErros);
    LiberarLexico();

    if (simbolos < 0 || erros < 0) {
        return EXIT_FAILURE;
    }

    printf("Pre-processamento ..: %s\n", arquivo_pre);
    printf("Tokens .............: %s\n", arquivo_lex);
    printf("Tabela de simbolos .: %s (%d simbolos)\n", arquivo_ts, simbolos);
    printf("Erros lexicos ......: %s (%d)\n", arquivo_err, erros);

    return EXIT_SUCCESS;
}
