/*
 lexico.c

 Analisador léxico da linguagem u-Assembly.

 Quem faz o trabalho é ProximoToken(), que implementa o AFD: cada valor do
 enum Estado é um estado do diagrama (docs/afd.dot) e cada case do switch
 tem as transições que saem dele.
*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lexico.h"

#define CAT_DIRETIVA    "diretiva"
#define CAT_INSTRUCAO   "instrução"
#define CAT_REGISTRADOR "registrador"
#define CAT_ROTULO      "identificador/rótulo"

#define QTD(vetor) (sizeof(vetor) / sizeof((vetor)[0]))

typedef enum {
    E_INICIAL,
    E_IDENTIFICADOR,
    E_DIRETIVA,
    E_REGISTRADOR,
    E_SINAL,        // leu '-', falta saber se é número
    E_ZERO,         // leu '0', pode ser zero ou começo de hexadecimal
    E_DECIMAL,
    E_HEX_INICIO,   // leu "0x", falta pelo menos um dígito
    E_HEX,
    E_STRING,
    E_ESCAPE,       // leu '\' dentro de uma string
    E_COMENTARIO,
    E_MALFORMADO    // lexema com erro, descarta até o próximo delimitador
} Estado;

static const char *DIRETIVAS[] = {
    ".data", ".text", ".word", ".half", ".byte",
    ".space", ".ascii", ".asciiz", ".globl", ".align"
};

static const char *INSTRUCOES[] = {
    // transferência de dados
    "li", "la", "move", "lw", "sw", "lb", "sb", "lh", "sh",
    // aritméticas
    "add", "addi", "addu", "sub", "subu", "mul", "div", "mflo", "mfhi",
    // lógicas, deslocamentos e comparação
    "and", "or", "xor", "nor", "sll", "srl", "slt",
    // desvios e controle
    "beq", "bne", "blt", "ble", "bgt", "bge", "j", "jal", "jr",
    "syscall", "nop"
};

/* As formas numéricas $0 a $31 são geradas em IniciarLexico(). */
static const char *REGISTRADORES[] = {
    "$zero",
    "$v0", "$v1",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8", "$t9",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$k0", "$k1",
    "$gp", "$sp", "$fp", "$ra"
};

/* Estado que precisa sobreviver entre uma chamada e outra de ProximoToken() */
static FILE *entrada = NULL;
static int linha = 1;
static int coluna = 1;

static Simbolo *tabela = NULL;
static int total_simbolos = 0;
static int capacidade_tabela = 0;

static Token *erros = NULL;
static int total_erros = 0;
static int capacidade_erros = 0;

/* AnaliseLexica() é void e não tem como devolver erro, então a falta de
   memória encerra o programa aqui mesmo, com uma mensagem. */
static void sem_memoria(void) {
    fprintf(stderr, "Erro: memoria insuficiente durante a analise lexica.\n");
    exit(EXIT_FAILURE);
}

/*
 espiar() olha o próximo caractere sem consumir; avancar() consome e atualiza
 linha e coluna. Assim cada estado espia, decide, e só consome se o caractere
 fizer parte do lexema. O caractere que encerrou o lexema continua no arquivo
 para a próxima chamada.
*/
static int espiar(void) {
    int c = fgetc(entrada);
    if (c != EOF) {
        ungetc(c, entrada);
    }
    return c;
}

static int avancar(void) {
    int c = fgetc(entrada);

    if (c == '\n') {
        linha++;
        coluna = 1;
    } else if (c != EOF) {
        coluna++;
    }
    return c;
}

/* Consome o próximo caractere e acrescenta ao lexema. Se o lexema passar
   do tamanho do vetor, o resto é lido mas não é guardado. */
static int consumir(Token *t, int *tam) {
    int c = avancar();

    if (*tam < TAM_LEXEMA - 1) {
        t->lexema[(*tam)++] = (char) c;
        t->lexema[*tam] = '\0';
    }
    return c;
}

static int eh_letra(int c) {
    return isalpha(c) || c == '_';
}

static int eh_char_id(int c) {
    return isalnum(c) || c == '_';
}

/* Caracteres que encerram um lexema. Um estado que encontra um deles sabe
   que o lexema acabou, mas não consome o delimitador. */
static int eh_delimitador(int c) {
    return c == EOF || c == ' ' || c == '\t' || c == '\r' || c == '\n'
        || c == ',' || c == ':' || c == '(' || c == ')'
        || c == '"' || c == '#';
}

static void para_minusculas(char *destino, const char *origem) {
    size_t i;

    for (i = 0; origem[i] != '\0'; i++) {
        destino[i] = (char) tolower((unsigned char) origem[i]);
    }
    destino[i] = '\0';
}

/* Monta nomes como INS_ADD e DIR_ASCIIZ a partir do lexema. */
static void montar_nome(Token *t, const char *prefixo, const char *palavra) {
    size_t n = strlen(prefixo);
    size_t i;

    strcpy(t->nome, prefixo);
    for (i = 0; palavra[i] != '\0' && n + i < TAM_NOME - 1; i++) {
        t->nome[n + i] = (char) toupper((unsigned char) palavra[i]);
    }
    t->nome[n + i] = '\0';
}

static Simbolo *buscar_simbolo(const char *lexema) {
    int i;

    for (i = 0; i < total_simbolos; i++) {
        if (strcmp(tabela[i].lexema, lexema) == 0) {
            return &tabela[i];
        }
    }
    return NULL;
}

static void inserir_simbolo(const char *lexema, const char *categoria, int lin, int col) {
    Simbolo *s;

    // o mesmo lexema não pode entrar duas vezes
    if (buscar_simbolo(lexema) != NULL) {
        return;
    }

    if (total_simbolos == capacidade_tabela) {
        int nova = capacidade_tabela == 0 ? 128 : capacidade_tabela * 2;
        Simbolo *novo = realloc(tabela, nova * sizeof(Simbolo));
        if (novo == NULL) {
            sem_memoria();
        }
        tabela = novo;
        capacidade_tabela = nova;
    }

    s = &tabela[total_simbolos++];
    strncpy(s->lexema, lexema, TAM_LEXEMA - 1);
    s->lexema[TAM_LEXEMA - 1] = '\0';
    strncpy(s->categoria, categoria, TAM_CATEGORIA - 1);
    s->categoria[TAM_CATEGORIA - 1] = '\0';
    s->primeiraLinha = lin;
    s->primeiraColuna = col;
}

/*
 Procura uma palavra reservada na tabela. A busca é feita em minúsculas
 porque o enunciado manda normalizar instruções, diretivas e registradores,
 então ADD e add são a mesma instrução. Se encontrar, preenche a posição do
 primeiro uso, que até então estava zerada.
*/
static int reconhecer_reservada(const Token *t, const char *categoria) {
    char minusculo[TAM_LEXEMA];
    Simbolo *s;

    para_minusculas(minusculo, t->lexema);
    s = buscar_simbolo(minusculo);

    if (s == NULL || strcmp(s->categoria, categoria) != 0) {
        return 0;
    }

    if (s->primeiraLinha == 0) {
        s->primeiraLinha = t->linha;
        s->primeiraColuna = t->coluna;
    }
    return 1;
}

/*
 Um lexema formado por letras pode ser instrução ou identificador, e quem
 decide é a tabela de símbolos. Se não for instrução, é ID e entra na
 tabela na primeira vez que aparece, seja na declaração (laco:) ou numa
 referência antecipada (j laco). Identificadores diferenciam maiúsculas de
 minúsculas, então inicio e Inicio viram duas entradas.
*/
static void classificar_palavra(Token *t) {
    if (reconhecer_reservada(t, CAT_INSTRUCAO)) {
        montar_nome(t, "INS_", t->lexema);
        return;
    }

    strcpy(t->nome, "ID");
    inserir_simbolo(t->lexema, CAT_ROTULO, t->linha, t->coluna);
}

static void registrar_erro(const char *tipo, const Token *t) {
    if (total_erros == capacidade_erros) {
        int nova = capacidade_erros == 0 ? 32 : capacidade_erros * 2;
        Token *novo = realloc(erros, nova * sizeof(Token));
        if (novo == NULL) {
            sem_memoria();
        }
        erros = novo;
        capacidade_erros = nova;
    }

    erros[total_erros] = *t;
    strcpy(erros[total_erros].nome, tipo);
    total_erros++;
}

/* Um número seguido de letra, como 2valor, é tratado como identificador que
   começou com dígito, que é como o enunciado classifica esse exemplo.
   Qualquer outro caso (12.5, 0xG1, -3abc) é número malformado. */
static const char *erro_de_numero(const Token *t, int c) {
    if (t->lexema[0] != '-' && eh_letra(c)) {
        return "ERRO_IDENTIFICADOR_MALFORMADO";
    }
    return "ERRO_NUMERO_MALFORMADO";
}

/*
 A cada volta do laço espiamos o próximo caractere e o estado atual decide se
 consome, se muda de estado ou se o lexema acabou. Terminando bem, devolve o
 token; terminando com erro, guarda o erro e volta ao estado inicial. A análise
 nunca para no primeiro erro, porque o .err precisa ter todos eles.
*/
Token ProximoToken(void) {
    Estado estado = E_INICIAL;
    Token t;
    Token escape;                  // guarda o texto e a posição de um \x inválido
    const char *erro = NULL;       // tipo de erro usado pelo E_MALFORMADO
    int string_invalida = 0;
    int tam = 0;
    int c;

    t.nome[0] = '\0';
    t.lexema[0] = '\0';
    t.linha = linha;
    t.coluna = coluna;
    escape.linha = 0;
    escape.coluna = 0;

    for (;;) {
        c = espiar();

        switch (estado) {

        case E_INICIAL:
            // todo lexema começa aqui, então a posição dele é a atual
            tam = 0;
            t.lexema[0] = '\0';
            t.linha = linha;
            t.coluna = coluna;

            if (c == EOF) {
                strcpy(t.nome, "TK_EOF");
                strcpy(t.lexema, "EOF");
                return t;
            }

            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                avancar();
            } else if (c == '#') {
                avancar();
                estado = E_COMENTARIO;
            } else if (eh_letra(c)) {
                consumir(&t, &tam);
                estado = E_IDENTIFICADOR;
            } else if (c == '.') {
                consumir(&t, &tam);
                estado = E_DIRETIVA;
            } else if (c == '$') {
                consumir(&t, &tam);
                estado = E_REGISTRADOR;
            } else if (c == '-') {
                consumir(&t, &tam);
                estado = E_SINAL;
            } else if (c == '0') {
                consumir(&t, &tam);
                estado = E_ZERO;
            } else if (isdigit(c)) {
                consumir(&t, &tam);
                estado = E_DECIMAL;
            } else if (c == '"') {
                consumir(&t, &tam);
                string_invalida = 0;
                estado = E_STRING;
            } else if (c == ',') {
                consumir(&t, &tam);
                strcpy(t.nome, "SMB_COM");
                return t;
            } else if (c == ':') {
                consumir(&t, &tam);
                strcpy(t.nome, "SMB_COL");
                return t;
            } else if (c == '(') {
                consumir(&t, &tam);
                strcpy(t.nome, "SMB_OPA");
                return t;
            } else if (c == ')') {
                consumir(&t, &tam);
                strcpy(t.nome, "SMB_CPA");
                return t;
            } else {
                consumir(&t, &tam);
                registrar_erro("ERRO_CARACTERE_INVALIDO", &t);
            }
            break;

        case E_COMENTARIO:
            // vai até o fim da linha e não gera token
            if (c == '\n' || c == EOF) {
                estado = E_INICIAL;
            } else {
                avancar();
            }
            break;

        case E_IDENTIFICADOR:
            if (eh_char_id(c)) {
                consumir(&t, &tam);
            } else if (eh_delimitador(c)) {
                classificar_palavra(&t);
                return t;
            } else {
                erro = "ERRO_IDENTIFICADOR_MALFORMADO";
                estado = E_MALFORMADO;
            }
            break;

        case E_DIRETIVA:
            if (eh_char_id(c)) {
                consumir(&t, &tam);
            } else if (eh_delimitador(c)) {
                if (reconhecer_reservada(&t, CAT_DIRETIVA)) {
                    montar_nome(&t, "DIR_", t.lexema + 1);
                    return t;
                }
                registrar_erro("ERRO_DIRETIVA_INVALIDA", &t);
                estado = E_INICIAL;
            } else {
                erro = "ERRO_DIRETIVA_INVALIDA";
                estado = E_MALFORMADO;
            }
            break;

        case E_REGISTRADOR:
            if (eh_char_id(c)) {
                consumir(&t, &tam);
            } else if (eh_delimitador(c)) {
                if (reconhecer_reservada(&t, CAT_REGISTRADOR)) {
                    strcpy(t.nome, "REG");
                    return t;
                }
                registrar_erro("ERRO_REGISTRADOR_INVALIDO", &t);
                estado = E_INICIAL;
            } else {
                erro = "ERRO_REGISTRADOR_INVALIDO";
                estado = E_MALFORMADO;
            }
            break;

        case E_SINAL:
            // o '-' só faz parte do número se vier colado a um dígito
            if (c == '0') {
                consumir(&t, &tam);
                estado = E_ZERO;
            } else if (isdigit(c)) {
                consumir(&t, &tam);
                estado = E_DECIMAL;
            } else {
                registrar_erro("ERRO_CARACTERE_INVALIDO", &t);
                estado = E_INICIAL;
            }
            break;

        case E_ZERO:
            if (c == 'x' || c == 'X') {
                consumir(&t, &tam);
                estado = E_HEX_INICIO;
            } else if (isdigit(c)) {
                consumir(&t, &tam);
                estado = E_DECIMAL;
            } else if (eh_delimitador(c)) {
                strcpy(t.nome, "NUM_INT");
                return t;
            } else {
                erro = erro_de_numero(&t, c);
                estado = E_MALFORMADO;
            }
            break;

        case E_DECIMAL:
            if (isdigit(c)) {
                consumir(&t, &tam);
            } else if (eh_delimitador(c)) {
                strcpy(t.nome, "NUM_INT");
                return t;
            } else {
                erro = erro_de_numero(&t, c);
                estado = E_MALFORMADO;
            }
            break;

        case E_HEX_INICIO:
            // "0x" sozinho não é número
            if (isxdigit(c)) {
                consumir(&t, &tam);
                estado = E_HEX;
            } else {
                erro = "ERRO_NUMERO_MALFORMADO";
                estado = E_MALFORMADO;
            }
            break;

        case E_HEX:
            if (isxdigit(c)) {
                consumir(&t, &tam);
            } else if (eh_delimitador(c)) {
                strcpy(t.nome, "NUM_INT");
                return t;
            } else {
                erro = "ERRO_NUMERO_MALFORMADO";
                estado = E_MALFORMADO;
            }
            break;

        case E_STRING:
            if (c == '"') {
                consumir(&t, &tam);
                if (!string_invalida) {
                    strcpy(t.nome, "STRING");
                    return t;
                }
                // o erro do escape já foi registrado, a string não vira token
                estado = E_INICIAL;
            } else if (c == '\\') {
                escape.linha = linha;
                escape.coluna = coluna;
                consumir(&t, &tam);
                estado = E_ESCAPE;
            } else if (c == '\n' || c == '\r') {
                registrar_erro("ERRO_STRING_NAO_FECHADA", &t);
                estado = E_INICIAL;
            } else if (c == EOF) {
                registrar_erro("ERRO_STRING_NAO_FECHADA_EOF", &t);
                estado = E_INICIAL;
            } else {
                consumir(&t, &tam);
            }
            break;

        case E_ESCAPE:
            if (c == 'n' || c == 't' || c == '"' || c == '\\' || c == '0') {
                consumir(&t, &tam);
                estado = E_STRING;
            } else if (c == '\n' || c == '\r' || c == EOF) {
                // a string acabou sem fechar; o E_STRING registra esse erro
                estado = E_STRING;
            } else {
                escape.lexema[0] = '\\';
                escape.lexema[1] = (char) c;
                escape.lexema[2] = '\0';
                registrar_erro("ERRO_ESCAPE_INVALIDO", &escape);
                string_invalida = 1;
                consumir(&t, &tam);
                estado = E_STRING;
            }
            break;

        case E_MALFORMADO:
            // descarta o resto do lexema e só então registra o erro
            if (eh_delimitador(c)) {
                registrar_erro(erro, &t);
                estado = E_INICIAL;
            } else {
                consumir(&t, &tam);
            }
            break;
        }
    }
}

void IniciarLexico(FILE *in) {
    char nome[8];
    size_t i;
    int n;

    // se já houve uma análise antes, começa do zero
    LiberarLexico();

    entrada = in;
    linha = 1;
    coluna = 1;

    for (i = 0; i < QTD(DIRETIVAS); i++) {
        inserir_simbolo(DIRETIVAS[i], CAT_DIRETIVA, 0, 0);
    }
    for (i = 0; i < QTD(INSTRUCOES); i++) {
        inserir_simbolo(INSTRUCOES[i], CAT_INSTRUCAO, 0, 0);
    }
    for (i = 0; i < QTD(REGISTRADORES); i++) {
        inserir_simbolo(REGISTRADORES[i], CAT_REGISTRADOR, 0, 0);
    }
    for (n = 0; n <= 31; n++) {
        snprintf(nome, sizeof(nome), "$%d", n);
        inserir_simbolo(nome, CAT_REGISTRADOR, 0, 0);
    }
}

void AnaliseLexica(FILE *in, FILE *out) {
    Token t;

    IniciarLexico(in);

    do {
        t = ProximoToken();
        fprintf(out, "<%s, %s> %d %d\n", t.nome, t.lexema, t.linha, t.coluna);
    } while (strcmp(t.nome, "TK_EOF") != 0);
}

int GravarTabelaSimbolos(FILE *ts) {
    int i;

    fprintf(ts, "LEXEMA,CATEGORIA,LINHA,COLUNA\n");
    for (i = 0; i < total_simbolos; i++) {
        fprintf(ts, "%s,%s,%d,%d\n", tabela[i].lexema, tabela[i].categoria,
                tabela[i].primeiraLinha, tabela[i].primeiraColuna);
    }
    return total_simbolos;
}

int GravarErros(FILE *err) {
    int i;

    if (total_erros == 0) {
        fprintf(err, "Nenhum erro léxico encontrado.\n");
        return 0;
    }

    for (i = 0; i < total_erros; i++) {
        fprintf(err, "<%s, %s> %d %d\n", erros[i].nome, erros[i].lexema,
                erros[i].linha, erros[i].coluna);
    }
    return total_erros;
}

void LiberarLexico(void) {
    free(tabela);
    free(erros);
    tabela = NULL;
    erros = NULL;
    total_simbolos = 0;
    capacidade_tabela = 0;
    total_erros = 0;
    capacidade_erros = 0;
}
