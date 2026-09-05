/*
 * preprocessador.c
 *
 * Funções de pré-processamento do código u-Assembly.
 *
 * Quase tudo aqui depende de saber se o caractere que estamos lendo está
 * dentro ou fora de uma string, porque fora das aspas o '#' começa um
 * comentário e os espaços podem ser normalizados, e dentro delas nada pode
 * ser alterado. Por isso as funções que percorrem a linha usam sempre uma
 * variável dentro_string para controlar isso.
 */

#include <stdlib.h>
#include <string.h>

#include "preprocessador.h"

int eh_espaco(char c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

/*
 * Seção 2.2 - remoção de comentários.
 * Percorre a linha e, no primeiro '#' que estiver fora de uma string,
 * coloca o '\0'. A linha simplesmente termina ali, sem precisar copiar nada.
 */
int remover_comentario(char *linha)
{
    size_t i = 0;
    int dentro_string = 0;

    while (linha[i] != '\0') {
        if (dentro_string) {
            /* a contrabarra protege o próximo caractere, então \" não fecha
               a string */
            if (linha[i] == '\\' && linha[i + 1] != '\0') {
                i += 2;
                continue;
            }
            if (linha[i] == ASPAS) {
                dentro_string = 0;
            }
        } else {
            if (linha[i] == ASPAS) {
                dentro_string = 1;
            } else if (linha[i] == COMENTARIO) {
                linha[i] = '\0';
                return 1;
            }
        }
        i++;
    }

    return 0;
}

/*
 * Seções 2.4 e 2.8 - espaços e tabulações, sem mexer nas strings.
 *
 * Reescrevemos a linha dentro dela mesma usando dois índices: leitura anda
 * sempre à frente de escrita, então nunca sobrescrevemos nada que ainda
 * precisamos ler.
 *
 * O detalhe importante é a variável espaco_pendente. Em vez de copiar o
 * branco na hora, só anotamos que ele apareceu; o espaço é gravado quando
 * chega o próximo caractere útil. Com isso os brancos do começo não são
 * gravados (nada foi escrito ainda), os do fim também não (não vem mais
 * nada depois) e uma sequência de dez vira um espaço só.
 */
void normalizar_espacos(char *linha)
{
    size_t leitura = 0;
    size_t escrita = 0;
    int dentro_string = 0;
    int espaco_pendente = 0;

    while (linha[leitura] != '\0') {

        if (dentro_string) {
            char atual = linha[leitura];

            linha[escrita++] = atual;
            leitura++;

            /* copia o par escapado inteiro para o \" não ser confundido
               com o fechamento da string */
            if (atual == '\\' && linha[leitura] != '\0') {
                linha[escrita++] = linha[leitura++];
                continue;
            }
            if (atual == ASPAS) {
                dentro_string = 0;
            }
            continue;
        }

        if (eh_espaco(linha[leitura])) {
            if (escrita > 0) {
                espaco_pendente = 1;
            }
            leitura++;
            continue;
        }

        if (espaco_pendente) {
            linha[escrita++] = ' ';
            espaco_pendente = 0;
        }

        if (linha[leitura] == ASPAS) {
            dentro_string = 1;
        }

        linha[escrita++] = linha[leitura++];
    }

    linha[escrita] = '\0';
}

/* Seção 2.3. Chamada depois das outras, porque uma linha que só tinha
   comentário chega aqui já vazia. */
int linha_vazia(const char *linha)
{
    size_t i;

    for (i = 0; linha[i] != '\0'; i++) {
        if (!eh_espaco(linha[i])) {
            return 0;
        }
    }

    return 1;
}

/*
 * Só para avisar o usuário. Não podemos tratar como erro porque a seção 2.10
 * proíbe o pré-processador de validar sintaxe, mas uma aspas esquecida quase
 * sempre é erro de digitação e o aviso ajuda a achar.
 */
int string_aberta(const char *linha)
{
    size_t i = 0;
    int dentro_string = 0;

    while (linha[i] != '\0') {
        if (dentro_string) {
            if (linha[i] == '\\' && linha[i + 1] != '\0') {
                i += 2;
                continue;
            }
            if (linha[i] == ASPAS) {
                dentro_string = 0;
            }
        } else if (linha[i] == ASPAS) {
            dentro_string = 1;
        }
        i++;
    }

    return dentro_string;
}

/* O Bloco de Notas salva arquivos UTF-8 começando com os bytes EF BB BF.
   Eles não aparecem no editor, mas grudam na primeira linha e atrapalham. */
void remover_bom(char *linha)
{
    unsigned char *b = (unsigned char *) linha;

    if (b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) {
        memmove(linha, linha + 3, strlen(linha + 3) + 1);
    }
}

/*
 * A ordem das etapas importa: o comentário sai primeiro para não gastarmos
 * tempo normalizando um texto que vai ser jogado fora, o aviso vem depois do
 * corte para não acusar uma aspas que estava dentro do comentário, e o teste
 * de linha vazia fica por último porque só aí a linha está do jeito final.
 */
int processar_linha(char *linha, Estatisticas *est, long numero)
{
    if (remover_comentario(linha)) {
        est->comentarios++;
    }

    if (string_aberta(linha)) {
        est->avisos++;
        fprintf(stderr, "Aviso: linha %ld tem uma string sem fechar.\n", numero);
    }

    normalizar_espacos(linha);

    if (linha_vazia(linha)) {
        est->removidas++;
        return 0;
    }

    return 1;
}

/*
 * Seção 2.5 - quebras de linha.
 *
 * O mesmo arquivo pode ter três terminadores diferentes: "\n" no Linux e no
 * macOS, "\r\n" no Windows e "\r" sozinho no Mac antigo. Lemos byte a byte e
 * paramos nos três casos, sem incluir o terminador na string devolvida. Quem
 * grava a saída é que escolhe qual usar, e é assim que a saída fica uniforme.
 */
char *ler_linha(FILE *entrada, int *status)
{
    size_t capacidade = TAM_INICIAL;
    size_t tamanho = 0;
    char *linha;
    int c = EOF;

    *status = OK;

    linha = malloc(capacidade);
    if (linha == NULL) {
        *status = ERRO_MEMORIA;
        return NULL;
    }

    while ((c = fgetc(entrada)) != EOF) {

        if (c == '\n') {
            break;
        }

        if (c == '\r') {
            /* espia o próximo byte: se não for '\n', era um CR sozinho e
               precisamos devolver o byte para não perdê-lo */
            int proximo = fgetc(entrada);
            if (proximo != '\n' && proximo != EOF) {
                ungetc(proximo, entrada);
            }
            break;
        }

        if (tamanho + 1 >= capacidade) {   /* o +1 reserva espaço para o '\0' */
            char *novo;
            capacidade *= 2;
            novo = realloc(linha, capacidade);
            if (novo == NULL) {
                free(linha);
                *status = ERRO_MEMORIA;
                return NULL;
            }
            linha = novo;
        }

        linha[tamanho++] = (char) c;
    }

    /* chegou ao fim sem ler nada: acabou o arquivo de verdade. Sem esse
       teste, um arquivo terminado em "\n" geraria uma linha vazia a mais. */
    if (c == EOF && tamanho == 0) {
        free(linha);
        return NULL;
    }

    linha[tamanho] = '\0';
    return linha;
}

int processar_arquivo(FILE *entrada, FILE *saida, Estatisticas *est)
{
    char *linha;
    int status = OK;
    long numero = 0;

    zerar_estatisticas(est);

    while ((linha = ler_linha(entrada, &status)) != NULL) {
        numero++;
        est->lidas++;

        if (numero == 1) {
            remover_bom(linha);
        }

        if (processar_linha(linha, est, numero)) {
            fprintf(saida, "%s%s", linha, FIM_DE_LINHA);
            est->gravadas++;
        }

        free(linha);
    }

    if (status != OK) {
        return status;
    }
    if (ferror(entrada)) {
        return ERRO_LEITURA;
    }
    if (ferror(saida)) {
        return ERRO_ESCRITA;
    }

    return OK;
}

void zerar_estatisticas(Estatisticas *est)
{
    est->lidas = 0;
    est->gravadas = 0;
    est->removidas = 0;
    est->comentarios = 0;
    est->avisos = 0;
}

/* Mensagens sem acento de propósito: o console do Windows costuma exibir
   acentuação UTF-8 como lixo. */
const char *mensagem_erro(int codigo)
{
    switch (codigo) {
        case ERRO_ARGUMENTOS:
            return "Quantidade de argumentos invalida.";
        case ERRO_ABRIR_ENTRADA:
            return "Nao foi possivel abrir o arquivo de entrada.";
        case ERRO_ABRIR_SAIDA:
            return "Nao foi possivel criar o arquivo de saida.";
        case ERRO_MESMO_ARQUIVO:
            return "O arquivo de saida nao pode ser o mesmo de entrada.";
        case ERRO_LEITURA:
            return "Falha ao ler o arquivo de entrada.";
        case ERRO_ESCRITA:
            return "Falha ao gravar o arquivo de saida.";
        case ERRO_MEMORIA:
            return "Memoria insuficiente.";
        default:
            return "Erro desconhecido.";
    }
}
