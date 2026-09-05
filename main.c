/*
 * main.c
 *
 * Linguagens Formais, Automatos e Compiladores
 * Etapa 1 - Pre-processador da linguagem u-Assembly
 *
 * Grupo: (preencher com os nomes)
 *
 * Compilar:  gcc main.c preprocessador.c -o main.exe
 * Executar:  ./main.exe entrada.asm saida.pre
 *
 * Este arquivo cuida só dos argumentos, dos arquivos e das mensagens.
 * A limpeza do código está toda em preprocessador.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preprocessador.h"

static void exibir_uso(const char *programa)
{
    fprintf(stderr, "\nUso: %s <arquivo_entrada.asm> <arquivo_saida.pre>\n", programa);
    fprintf(stderr, "Exemplo: %s teste.asm teste.pre\n\n", programa);
}

static void exibir_resumo(const Estatisticas *est)
{
    printf("Pre-processamento concluido.\n");
    printf("  Linhas lidas ........: %ld\n", est->lidas);
    printf("  Linhas gravadas .....: %ld\n", est->gravadas);
    printf("  Linhas removidas ....: %ld\n", est->removidas);
    printf("  Comentarios removidos: %ld\n", est->comentarios);
}

int main(int argc, char *argv[])
{
    FILE *entrada;
    FILE *saida;
    Estatisticas est;
    int resultado;

    if (argc != 3) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(ERRO_ARGUMENTOS));
        exibir_uso(argv[0]);
        return EXIT_FAILURE;
    }

    /* Se os dois nomes forem iguais, abrir a saída em modo escrita apagaria o
       arquivo de entrada antes de conseguirmos ler qualquer coisa. */
    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(ERRO_MESMO_ARQUIVO));
        return EXIT_FAILURE;
    }

    /* Modo binário porque no Windows o modo texto converteria as quebras de
       linha sozinho, e quem trata isso é a função ler_linha(). */
    entrada = fopen(argv[1], "rb");
    if (entrada == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_ENTRADA), argv[1]);
        return EXIT_FAILURE;
    }

    saida = fopen(argv[2], "wb");
    if (saida == NULL) {
        fprintf(stderr, "Erro: %s (%s)\n", mensagem_erro(ERRO_ABRIR_SAIDA), argv[2]);
        fclose(entrada);
        return EXIT_FAILURE;
    }

    resultado = processar_arquivo(entrada, saida, &est);

    fclose(entrada);

    /* o fclose da saída também pode falhar, se ainda tiver dados no buffer */
    if (fclose(saida) != 0 && resultado == OK) {
        resultado = ERRO_ESCRITA;
    }

    if (resultado != OK) {
        fprintf(stderr, "Erro: %s\n", mensagem_erro(resultado));
        remove(argv[2]);   /* não deixa um .pre pela metade no disco */
        return EXIT_FAILURE;
    }

    exibir_resumo(&est);
    return EXIT_SUCCESS;
}
