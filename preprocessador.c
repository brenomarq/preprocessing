/*
 preprocessador.c

 Funções de pré-processamento do código u-Assembly.
*/

#include <stdlib.h>

#include "preprocessador.h"

int eh_espaco(char c) {
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

/*
 Remoção de comentários.
 Percorre a linha e, no primeiro '#' que estiver fora de uma string,
 coloca o '\0'. Então, a linha simplesmente termina ali.
*/
int remover_comentario(char *linha) {
    size_t i = 0;
    int dentro_string = 0;

    while (linha[i] != '\0') {
        if (dentro_string) {
            // Ignora aspas escapadas, para não confundir com o fechamento da string
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
 Espaços e tabulações, sem mexer nas strings.
 
 Reescrevemos a linha dentro dela mesma usando dois índices: leitura anda
 sempre à frente de escrita, então nunca escrevemos nada por cima de algo 
 que ainda precisamos ler.
*/
void normalizar_espacos(char *linha) {
    size_t leitura = 0;
    size_t escrita = 0;
    int dentro_string = 0;
    int espaco_pendente = 0;

    while (linha[leitura] != '\0') {

        if (dentro_string) {
            char atual = linha[leitura];

            linha[escrita++] = atual;
            leitura++;

            /* Evita \" ser confundido com o fim da string. */
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

/* Chamada depois das outras funções, porque uma linha que só tinha
   comentário chega aqui já vazia. */
int linha_vazia(const char *linha) {
    size_t i;

    for (i = 0; linha[i] != '\0'; i++) {
        if (!eh_espaco(linha[i])) {
            return 0;
        }
    }

    return 1;
}

/*
 A ordem das etapas importa: o comentário sai primeiro para não gastarmos
 tempo normalizando um texto que vai ser jogado fora, e o teste de linha
 vazia fica por último porque só aí a linha está do jeito final. Por exemplo,
 uma linha que só tinha comentário só fica vazia depois das duas etapas.
*/
int processar_linha(char *linha, Estatisticas *est) {
    if (remover_comentario(linha)) {
        est->comentarios++;
    }

    normalizar_espacos(linha);

    if (linha_vazia(linha)) {
        est->removidas++;
        return 0;
    }

    return 1;
}

/*
 Quebras de linha.

 O mesmo arquivo pode ter três terminadores diferentes: "\n" no Linux e no
 macOS, "\r\n" no Windows e "\r" sozinho no Mac antigo. Então, lemos byte
 a byte e paramos nos três casos, sem incluir o terminador na string 
 devolvida.
*/
char *ler_linha(FILE *entrada, int *status) {
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
            // Confere o próximo byte para ver se é um '\n'
            int proximo = fgetc(entrada);
            if (proximo != '\n' && proximo != EOF) {
                ungetc(proximo, entrada);
            }
            break;
        }

        if (tamanho + 1 >= capacidade) {
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

    // Caso de chegar ao fim sem ler nada
    if (c == EOF && tamanho == 0) {
        free(linha);
        return NULL;
    }

    linha[tamanho] = '\0';
    return linha;
}

int processar_arquivo(FILE *entrada, FILE *saida, Estatisticas *est) {
    char *linha;
    int status = OK;

    zerar_estatisticas(est);

    while ((linha = ler_linha(entrada, &status)) != NULL) {
        est->lidas++;

        if (processar_linha(linha, est)) {
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

void zerar_estatisticas(Estatisticas *est) {
    est->lidas = 0;
    est->gravadas = 0;
    est->removidas = 0;
    est->comentarios = 0;
}

/* Mensagens de erros possíveis durante o pré-processamento. */
const char *mensagem_erro(int codigo) {
    switch (codigo) {
        case ERRO_ARGUMENTOS:
            return "Quantidade inválida de argumentos.";
        case ERRO_ABRIR_ENTRADA:
            return "Não foi possível abrir o arquivo de entrada.";
        case ERRO_ABRIR_SAIDA:
            return "Não foi possível criar o arquivo de saída.";
        case ERRO_MESMO_ARQUIVO:
            return "O arquivo de saída nao pode ser o mesmo da entrada.";
        case ERRO_LEITURA:
            return "Falha ao ler o arquivo de entrada.";
        case ERRO_ESCRITA:
            return "Falha ao gravar o arquivo de saída.";
        case ERRO_MEMORIA:
            return "Memória insuficiente.";
        default:
            return "Erro desconhecido.";
    }
}
