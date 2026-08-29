/* ==========================================================================
 * main.c
 * --------------------------------------------------------------------------
 * Universidade Catolica de Brasilia
 * Linguagens Formais, Automatos e Compiladores
 * Projeto Compilador-Assembly - Etapa 1: PRE-PROCESSADOR
 *
 * Responsabilidades deste arquivo (e SOMENTE estas):
 *   1. receber e validar os argumentos da linha de comando;
 *   2. abrir os arquivos de entrada e de saida;
 *   3. acionar o modulo de pre-processamento;
 *   4. relatar o resultado e encerrar de forma controlada.
 *
 * Toda a logica de transformacao do codigo esta em preprocessador.c.
 *
 * COMPILACAO
 *   gcc main.c preprocessador.c -o main.exe
 *
 * EXECUCAO
 *   ./main.exe <arquivo_entrada.asm> <arquivo_saida.pre>
 *   Exemplo: ./main.exe teste.asm teste.pre
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preprocessador.h"

/* Posicao de cada argumento em argv, para o codigo ficar autoexplicativo. */
#define ARG_PROGRAMA 0
#define ARG_ENTRADA  1
#define ARG_SAIDA    2
#define TOTAL_ARGS   3   /* nome do programa + entrada + saida */

/* --------------------------------------------------------------------------
 * Mostra como o programa deve ser usado. Chamado quando os argumentos estao
 * errados. Sem acentos, por causa do console do Windows.
 * -------------------------------------------------------------------------- */
static void exibir_uso(const char *nome_programa)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "Pre-processador u-Assembly (MIPS/MARS)\n");
    fprintf(stderr, "--------------------------------------\n");
    fprintf(stderr, "Uso:\n");
    fprintf(stderr, "  %s <arquivo_entrada.asm> <arquivo_saida.pre>\n\n",
            nome_programa);
    fprintf(stderr, "Exemplo:\n");
    fprintf(stderr, "  %s teste.asm teste.pre\n\n", nome_programa);
    fprintf(stderr, "O programa le o codigo Assembly do primeiro arquivo,\n");
    fprintf(stderr, "remove comentarios, linhas vazias e espacos desnecessarios,\n");
    fprintf(stderr, "e grava o resultado normalizado no segundo arquivo.\n\n");
}

/* --------------------------------------------------------------------------
 * Imprime o resumo do processamento. Ajuda a conferir o resultado sem
 * precisar abrir os dois arquivos lado a lado.
 * -------------------------------------------------------------------------- */
static void exibir_resumo(const char *entrada, const char *saida,
                          const PPEstatisticas *est)
{
    printf("Pre-processamento concluido com sucesso.\n");
    printf("  Entrada ..............: %s\n", entrada);
    printf("  Saida ................: %s\n", saida);
    printf("  Linhas lidas .........: %ld\n", est->linhas_lidas);
    printf("  Linhas gravadas ......: %ld\n", est->linhas_gravadas);
    printf("  Linhas removidas .....: %ld\n", est->linhas_removidas);
    printf("  Comentarios removidos : %ld\n", est->comentarios_removidos);

    if (est->avisos > 0) {
        printf("  Avisos ...............: %ld (strings sem fechamento)\n",
               est->avisos);
    }
}

/* ==========================================================================
 * FUNCAO PRINCIPAL
 * ========================================================================== */
int main(int argc, char *argv[])
{
    FILE *arquivo_entrada = NULL;
    FILE *arquivo_saida = NULL;
    PPEstatisticas estatisticas;
    int resultado;

    /* ---- 1. Validacao da quantidade de argumentos ------------------------ */
    if (argc != TOTAL_ARGS) {
        fprintf(stderr, "Erro: %s\n", pp_mensagem_erro(PP_ERRO_ARGUMENTOS));
        fprintf(stderr, "Esperados %d argumentos, recebidos %d.\n",
                TOTAL_ARGS - 1, argc - 1);
        exibir_uso(argv[ARG_PROGRAMA]);
        return EXIT_FAILURE;
    }

    /* ---- 2. Protecao: entrada e saida nao podem ser o mesmo arquivo ------ *
     * Sem esta checagem, abrir a saida em modo "w" apagaria o codigo-fonte
     * do usuario antes mesmo de ele ser lido. A comparacao por nome nao
     * cobre todos os casos (caminhos diferentes para o mesmo arquivo), mas
     * evita o acidente mais comum.                                          */
    if (strcmp(argv[ARG_ENTRADA], argv[ARG_SAIDA]) == 0) {
        fprintf(stderr, "Erro: %s\n", pp_mensagem_erro(PP_ERRO_MESMO_ARQUIVO));
        return EXIT_FAILURE;
    }

    /* ---- 3. Abertura dos arquivos ---------------------------------------- *
     * Modo BINARIO ("rb" / "wb") de proposito: no Windows o modo texto
     * converteria as quebras de linha automaticamente, e quem trata isso
     * aqui e a funcao pp_ler_linha(), de forma explicita e igual em todos
     * os sistemas operacionais (secao 2.5 do enunciado).                    */
    arquivo_entrada = fopen(argv[ARG_ENTRADA], "rb");
    if (arquivo_entrada == NULL) {
        fprintf(stderr, "Erro: %s\n", pp_mensagem_erro(PP_ERRO_ABRIR_ENTRADA));
        fprintf(stderr, "Arquivo: %s\n", argv[ARG_ENTRADA]);
        return EXIT_FAILURE;
    }

    arquivo_saida = fopen(argv[ARG_SAIDA], "wb");
    if (arquivo_saida == NULL) {
        fprintf(stderr, "Erro: %s\n", pp_mensagem_erro(PP_ERRO_ABRIR_SAIDA));
        fprintf(stderr, "Arquivo: %s\n", argv[ARG_SAIDA]);
        fclose(arquivo_entrada);   /* nao deixa arquivo aberto para tras */
        return EXIT_FAILURE;
    }

    /* ---- 4. Pre-processamento propriamente dito -------------------------- */
    resultado = pp_processar_arquivo(arquivo_entrada, arquivo_saida,
                                     &estatisticas);

    /* ---- 5. Encerramento controlado -------------------------------------- */
    fclose(arquivo_entrada);

    /* O fclose da saida tambem pode falhar (dados ainda no buffer do
       sistema), por isso o retorno dele e verificado. */
    if (fclose(arquivo_saida) != 0 && resultado == PP_OK) {
        resultado = PP_ERRO_ESCRITA;
    }

    if (resultado != PP_OK) {
        fprintf(stderr, "Erro: %s\n", pp_mensagem_erro(resultado));
        remove(argv[ARG_SAIDA]);   /* nao deixa um .pre incompleto no disco */
        return EXIT_FAILURE;
    }

    exibir_resumo(argv[ARG_ENTRADA], argv[ARG_SAIDA], &estatisticas);
    return EXIT_SUCCESS;
}
