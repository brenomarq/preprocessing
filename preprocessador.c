/* ==========================================================================
 * preprocessador.c
 * --------------------------------------------------------------------------
 * Implementacao das funcoes de pre-processamento da linguagem u-Assembly.
 *
 * IDEIA CENTRAL DO MODULO
 * -----------------------
 * Praticamente todo o trabalho se resume a uma pergunta feita caractere a
 * caractere: "eu estou DENTRO ou FORA de uma string?".
 *
 *      FORA da string  -> '#' inicia comentario, brancos sao normalizados
 *      DENTRO da string-> tudo e conteudo e deve ser copiado como esta
 *
 * Isso e, na pratica, um automato finito de dois estados - exatamente o
 * assunto da disciplina:
 *
 *                     +---- caractere qualquer ----+
 *                     |                            |
 *                     v            "               |
 *              ( FORA_STRING ) -------------> ( DENTRO_STRING )
 *                   ^  |                            |  ^
 *                   |  +-- '#' -> corta a linha     |  +-- \x -> copia par
 *                   +------------- " ---------------+
 *
 * Cada funcao abaixo percorre a linha implementando esse mesmo automato,
 * mudando apenas o que faz em cada estado.
 * ========================================================================== */

#include <stdlib.h>   /* malloc, realloc, free */
#include <string.h>   /* memmove, strlen       */

#include "preprocessador.h"

/* ==========================================================================
 * 1. UTILITARIOS DE CARACTERE
 * ========================================================================== */

/* Espacos em branco horizontais que aparecem dentro de uma linha.
   '\n' e '\r' nao entram na lista porque o leitor de linhas ja os retirou. */
int pp_eh_branco(char c)
{
    return (c == ' ' || c == '\t' || c == '\v' || c == '\f');
}

/* ==========================================================================
 * 2. SECAO 2.2 DO ENUNCIADO - REMOCAO DE COMENTARIOS
 * --------------------------------------------------------------------------
 * Percorremos a linha da esquerda para a direita controlando o estado
 * "dentro de string". Ao encontrar '#' FORA de uma string, colocamos o
 * terminador '\0' naquela posicao: a linha simplesmente acaba ali.
 *
 * Exemplos:
 *   "add $t0, $t1, $t2   # soma"     ->  "add $t0, $t1, $t2   "
 *   "msg: .asciiz \"Valor # 1\""      ->  inalterada ('#' esta dentro da string)
 *   "# linha so de comentario"       ->  "" (vira linha vazia, some depois)
 * ========================================================================== */
int pp_remover_comentario(char *linha)
{
    size_t i = 0;
    int dentro_string = 0;

    if (linha == NULL) {
        return 0;
    }

    while (linha[i] != '\0') {

        if (dentro_string) {
            /* Dentro da string: a contrabarra protege o proximo caractere,
               entao "\"" nao fecha a string. Pulamos os dois de uma vez. */
            if (linha[i] == PP_ESCAPE && linha[i + 1] != '\0') {
                i += 2;
                continue;
            }
            if (linha[i] == PP_ASPAS) {
                dentro_string = 0;   /* aspas de fechamento */
            }
        } else {
            if (linha[i] == PP_ASPAS) {
                dentro_string = 1;   /* aspas de abertura */
            } else if (linha[i] == PP_CARACTERE_COMENTARIO) {
                linha[i] = '\0';     /* corta o comentario aqui */
                return 1;
            }
        }
        i++;
    }

    return 0;   /* nao havia comentario nesta linha */
}

/* ==========================================================================
 * 3. SECOES 2.4 E 2.8 - NORMALIZACAO DE ESPACOS / PRESERVACAO DE STRINGS
 * --------------------------------------------------------------------------
 * Usamos a tecnica dos "dois indices" (two pointers), que reescreve a string
 * dentro dela mesma, sem precisar de memoria extra:
 *
 *      leitura -> posicao que estamos lendo
 *      escrita -> posicao onde vamos gravar (nunca passa de leitura)
 *
 * A variavel espaco_pendente e o truque que resolve as tres exigencias de
 * uma vez so:
 *   - marcamos que "houve branco" em vez de copiar o branco na hora;
 *   - o espaco so e gravado imediatamente antes do proximo caractere util;
 *   - logo, brancos no inicio nunca sao gravados (escrita ainda e 0),
 *     brancos no fim nunca sao gravados (nao vem caractere util depois) e
 *     sequencias de brancos viram um unico espaco.
 *
 * Exemplo:
 *   "    add\t\t $t0,   $t1  "   ->   "add $t0, $t1"
 *   "msg: .asciiz \"A     B\""    ->   "msg: .asciiz \"A     B\"" (string intacta)
 * ========================================================================== */
void pp_normalizar_espacos(char *linha)
{
    size_t leitura = 0;
    size_t escrita = 0;
    int dentro_string = 0;
    int espaco_pendente = 0;

    if (linha == NULL) {
        return;
    }

    while (linha[leitura] != '\0') {

        /* ---- ESTADO 1: DENTRO DE UMA STRING -> copia fiel ---------------- */
        if (dentro_string) {
            char atual = linha[leitura];

            linha[escrita++] = atual;
            leitura++;

            /* Par escapado (\" , \\ , \n ...): copia o segundo caractere
               tambem, sem deixar que ele mude o estado do automato. */
            if (atual == PP_ESCAPE && linha[leitura] != '\0') {
                linha[escrita++] = linha[leitura++];
                continue;
            }
            if (atual == PP_ASPAS) {
                dentro_string = 0;   /* fechou a string */
            }
            continue;
        }

        /* ---- ESTADO 2: FORA DE STRING -> normaliza ----------------------- */
        if (pp_eh_branco(linha[leitura])) {
            /* So marca a pendencia se ja existe algo escrito; assim os
               brancos do inicio da linha desaparecem naturalmente. */
            if (escrita > 0) {
                espaco_pendente = 1;
            }
            leitura++;
            continue;
        }

        /* Chegou um caractere util: agora sim o espaco pendente vira UM espaco. */
        if (espaco_pendente) {
            linha[escrita++] = ' ';
            espaco_pendente = 0;
        }

        if (linha[leitura] == PP_ASPAS) {
            dentro_string = 1;       /* abriu uma string */
        }

        linha[escrita++] = linha[leitura++];
    }

    linha[escrita] = '\0';   /* fecha a linha ja normalizada */
}

/* ==========================================================================
 * 4. SECAO 2.3 - LINHA VAZIA
 * --------------------------------------------------------------------------
 * Chamada DEPOIS das duas etapas anteriores. Se sobrou algum caractere que
 * nao seja branco, a linha tem conteudo util e deve ser gravada.
 * ========================================================================== */
int pp_esta_vazia(const char *linha)
{
    size_t i = 0;

    if (linha == NULL) {
        return 1;
    }

    while (linha[i] != '\0') {
        if (!pp_eh_branco(linha[i])) {
            return 0;   /* achou conteudo */
        }
        i++;
    }

    return 1;   /* percorreu tudo e so encontrou brancos */
}

/* ==========================================================================
 * 5. DIAGNOSTICO - STRING ABERTA E NAO FECHADA
 * --------------------------------------------------------------------------
 * A secao 2.10 do enunciado proibe o pre-processador de acusar erro de
 * sintaxe, entao NAO interrompemos o programa. Apenas avisamos no console,
 * porque uma aspas esquecida costuma ser um erro real de digitacao e o aviso
 * poupa muito tempo de depuracao nas proximas etapas.
 * ========================================================================== */
int pp_tem_string_aberta(const char *linha)
{
    size_t i = 0;
    int dentro_string = 0;

    if (linha == NULL) {
        return 0;
    }

    while (linha[i] != '\0') {
        if (dentro_string) {
            if (linha[i] == PP_ESCAPE && linha[i + 1] != '\0') {
                i += 2;
                continue;
            }
            if (linha[i] == PP_ASPAS) {
                dentro_string = 0;
            }
        } else if (linha[i] == PP_ASPAS) {
            dentro_string = 1;
        }
        i++;
    }

    return dentro_string;
}

/* ==========================================================================
 * 6. BOM (BYTE ORDER MARK)
 * --------------------------------------------------------------------------
 * O Bloco de Notas do Windows salva arquivos UTF-8 comecando com os bytes
 * EF BB BF. Eles sao invisiveis no editor, mas para o nosso programa fazem
 * parte da primeira linha. Retiramos deslocando o resto da string 3 bytes
 * para a esquerda.
 * ========================================================================== */
void pp_remover_bom(char *linha)
{
    const unsigned char *bytes;

    if (linha == NULL) {
        return;
    }

    bytes = (const unsigned char *) linha;

    if (bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        /* +1 para levar junto o terminador '\0' */
        memmove(linha, linha + 3, strlen(linha + 3) + 1);
    }
}

/* ==========================================================================
 * 7. PROCESSAMENTO DE UMA LINHA (ordem das etapas)
 * --------------------------------------------------------------------------
 * A ORDEM IMPORTA:
 *   1) remover comentario   - senao normalizariamos espacos de um texto que
 *                             sera jogado fora, e um "#" comentado poderia
 *                             confundir a etapa seguinte;
 *   2) avisar string aberta - depois do corte, para nao avisar por causa de
 *                             uma aspas que estava dentro de um comentario;
 *   3) normalizar espacos   - trabalha sobre a linha ja limpa;
 *   4) testar se ficou vazia- so faz sentido no final, pois uma linha com
 *                             apenas um comentario vira uma linha vazia.
 *
 * Retorna 1 quando a linha deve ser gravada na saida.
 * ========================================================================== */
int pp_processar_linha(char *linha, PPEstatisticas *est, long numero_linha)
{
    if (linha == NULL) {
        return 0;
    }

    /* Etapa 1 - comentarios (secao 2.2) */
    if (pp_remover_comentario(linha)) {
        if (est != NULL) {
            est->comentarios_removidos++;
        }
    }

    /* Etapa 2 - aviso de string aberta (nao e erro, secao 2.10) */
    if (pp_tem_string_aberta(linha)) {
        if (est != NULL) {
            est->avisos++;
        }
        fprintf(stderr,
                "Aviso: linha %ld possui uma string sem aspas de fechamento.\n",
                numero_linha);
    }

    /* Etapa 3 - espacos e tabulacoes (secoes 2.4 e 2.8) */
    pp_normalizar_espacos(linha);

    /* Etapa 4 - linha vazia (secao 2.3) */
    if (pp_esta_vazia(linha)) {
        if (est != NULL) {
            est->linhas_removidas++;
        }
        return 0;   /* descartar */
    }

    return 1;       /* gravar */
}

/* ==========================================================================
 * 8. SECAO 2.5 - LEITURA DE UMA LINHA COM QUEBRA NORMALIZADA
 * --------------------------------------------------------------------------
 * Os arquivos podem vir de tres mundos diferentes:
 *
 *      Unix / macOS moderno .... "\n"      (LF)
 *      Windows ................. "\r\n"    (CRLF)
 *      Mac OS classico ......... "\r"      (CR)
 *
 * Lemos caractere a caractere e tratamos os tres casos. O terminador nunca
 * entra na string devolvida: quem grava a saida decide qual usar
 * (PP_FIM_DE_LINHA), garantindo a uniformidade exigida pelo enunciado.
 *
 * O buffer comeca com PP_TAM_INICIAL_LINHA bytes e DOBRA de tamanho sempre
 * que enche, entao nao existe limite de comprimento de linha.
 * ========================================================================== */
char *pp_ler_linha(FILE *entrada, int *status)
{
    size_t capacidade = PP_TAM_INICIAL_LINHA;
    size_t tamanho = 0;
    char *linha;
    int c = EOF;

    *status = PP_OK;

    linha = (char *) malloc(capacidade);
    if (linha == NULL) {
        *status = PP_ERRO_MEMORIA;
        return NULL;
    }

    while ((c = fgetc(entrada)) != EOF) {

        if (c == '\n') {            /* LF: fim de linha */
            break;
        }

        if (c == '\r') {            /* CR: pode ser CRLF ou CR sozinho */
            int proximo = fgetc(entrada);
            if (proximo != '\n' && proximo != EOF) {
                ungetc(proximo, entrada);   /* era CR sozinho: devolve o byte */
            }
            break;
        }

        /* Espaco para o caractere + o '\0' final? Se nao, dobra o buffer. */
        if (tamanho + 1 >= capacidade) {
            char *novo;
            capacidade *= 2;
            novo = (char *) realloc(linha, capacidade);
            if (novo == NULL) {
                free(linha);
                *status = PP_ERRO_MEMORIA;
                return NULL;
            }
            linha = novo;
        }

        linha[tamanho++] = (char) c;
    }

    /* Fim do arquivo sem nenhum caractere lido: nao ha mais linhas.
       (Isso evita inventar uma linha vazia quando o arquivo termina com "\n".) */
    if (c == EOF && tamanho == 0) {
        free(linha);
        return NULL;
    }

    linha[tamanho] = '\0';
    return linha;
}

/* ==========================================================================
 * 9. LACO PRINCIPAL - ARQUIVO INTEIRO
 * ========================================================================== */
int pp_processar_arquivo(FILE *entrada, FILE *saida, PPEstatisticas *est)
{
    char *linha;
    int status = PP_OK;
    long numero_linha = 0;

    pp_zerar_estatisticas(est);

    while ((linha = pp_ler_linha(entrada, &status)) != NULL) {

        numero_linha++;
        if (est != NULL) {
            est->linhas_lidas++;
        }

        /* O BOM so pode aparecer no comeco do arquivo. */
        if (numero_linha == 1) {
            pp_remover_bom(linha);
        }

        if (pp_processar_linha(linha, est, numero_linha)) {
            fprintf(saida, "%s%s", linha, PP_FIM_DE_LINHA);
            if (est != NULL) {
                est->linhas_gravadas++;
            }
        }

        free(linha);   /* pp_ler_linha alocou; quem chama libera */
    }

    if (status != PP_OK) {
        return status;              /* falta de memoria */
    }
    if (ferror(entrada)) {
        return PP_ERRO_LEITURA;     /* disco/permissao durante a leitura */
    }
    if (ferror(saida)) {
        return PP_ERRO_ESCRITA;     /* disco cheio, por exemplo */
    }

    return PP_OK;
}

/* ==========================================================================
 * 10. APOIO
 * ========================================================================== */
void pp_zerar_estatisticas(PPEstatisticas *est)
{
    if (est == NULL) {
        return;
    }
    est->linhas_lidas = 0;
    est->linhas_gravadas = 0;
    est->linhas_removidas = 0;
    est->comentarios_removidos = 0;
    est->avisos = 0;
}

/* Mensagens sem acentuacao de proposito: o console do Windows (cmd.exe) usa
   por padrao uma pagina de codigo que exibe acentos UTF-8 como lixo. */
const char *pp_mensagem_erro(int codigo)
{
    switch (codigo) {
        case PP_OK:
            return "Nenhum erro.";
        case PP_ERRO_ARGUMENTOS:
            return "Quantidade de argumentos invalida.";
        case PP_ERRO_ABRIR_ENTRADA:
            return "Nao foi possivel abrir o arquivo de entrada.";
        case PP_ERRO_ABRIR_SAIDA:
            return "Nao foi possivel criar o arquivo de saida.";
        case PP_ERRO_MESMO_ARQUIVO:
            return "O arquivo de saida nao pode ser o mesmo de entrada.";
        case PP_ERRO_LEITURA:
            return "Falha ao ler o arquivo de entrada.";
        case PP_ERRO_ESCRITA:
            return "Falha ao gravar o arquivo de saida.";
        case PP_ERRO_MEMORIA:
            return "Memoria insuficiente para processar o arquivo.";
        default:
            return "Erro desconhecido.";
    }
}
