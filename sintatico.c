/*
 sintatico.c

 Analisador sintático descendente recursivo da linguagem u-Assembly.

 A entrada é o arquivo .lex da etapa anterior. O módulo relê esse texto,
 reconstrói os tokens e acrescenta os marcadores EOL, que não existem no
 arquivo e precisam ser deduzidos do número da linha de cada token.
*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "sintatico.h"
#include "lexico.h"   /* reaproveita o tipo Token e o tamanho dos campos */

#define EOL    "EOL"
#define TK_EOF "TK_EOF"

/* Sequência de tokens já com os marcadores EOL */
static Token *fluxo = NULL;
static int total = 0;
static int capacidade = 0;
static int atual = 0;

static int acrescentar(const char *nome, const char *lexema, int linha, int coluna) {
    Token *t;

    if (total == capacidade) {
        int nova = capacidade == 0 ? 256 : capacidade * 2;
        Token *novo = realloc(fluxo, nova * sizeof(Token));
        if (novo == NULL) {
            return 0;
        }
        fluxo = novo;
        capacidade = nova;
    }

    t = &fluxo[total++];
    strncpy(t->nome, nome, TAM_NOME - 1);
    t->nome[TAM_NOME - 1] = '\0';
    strncpy(t->lexema, lexema, TAM_LEXEMA - 1);
    t->lexema[TAM_LEXEMA - 1] = '\0';
    t->linha = linha;
    t->coluna = coluna;
    return 1;
}

static int branco(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/*
 Desmonta uma linha do .lex no formato

     <NOME, lexema> linha coluna

 A leitura é feita da direita para a esquerda: primeiro a coluna, depois a
 linha e por fim o '>'. O motivo é que o lexema de uma STRING pode conter
 vírgula, espaço e até o próprio '>', como em <STRING, "a, b> c"> 3 5.
 Procurar o separador da esquerda para a direita quebraria nesse caso.

 O nome, esse sim, nunca contém vírgula, então vem da primeira vírgula.
 Devolve 0 se a linha não tiver o formato esperado.
*/
static int decompor(char *texto, Token *t) {
    char *virgula;
    char *p;
    char *fim_digitos;
    size_t tamanho;

    if (texto[0] != '<') {
        return 0;
    }

    virgula = strchr(texto, ',');
    if (virgula == NULL) {
        return 0;
    }

    p = texto + strlen(texto);
    while (p > texto && branco(p[-1])) {
        p--;
    }
    *p = '\0';

    /* coluna */
    fim_digitos = p;
    while (p > texto && isdigit((unsigned char) p[-1])) {
        p--;
    }
    if (p == fim_digitos) {
        return 0;
    }
    t->coluna = atoi(p);

    while (p > texto && (p[-1] == ' ' || p[-1] == '\t')) {
        p--;
    }

    /* linha */
    fim_digitos = p;
    while (p > texto && isdigit((unsigned char) p[-1])) {
        p--;
    }
    if (p == fim_digitos) {
        return 0;
    }
    t->linha = atoi(p);

    while (p > texto && (p[-1] == ' ' || p[-1] == '\t')) {
        p--;
    }

    if (p == texto || p[-1] != '>') {
        return 0;
    }
    p--;
    *p = '\0';          /* o que sobra é "<NOME, lexema" */

    if (virgula >= p) {
        return 0;       /* a vírgula encontrada era do lexema, não do separador */
    }

    tamanho = (size_t) (virgula - texto - 1);
    if (tamanho >= TAM_NOME) {
        tamanho = TAM_NOME - 1;
    }
    memcpy(t->nome, texto + 1, tamanho);
    t->nome[tamanho] = '\0';

    virgula++;
    if (*virgula == ' ') {
        virgula++;      /* o espaço que o léxico escreve depois da vírgula */
    }
    strncpy(t->lexema, virgula, TAM_LEXEMA - 1);
    t->lexema[TAM_LEXEMA - 1] = '\0';

    return 1;
}

/*
 Lê o arquivo inteiro montando o fluxo de tokens.

 O EOL é inserido sempre que o número da linha muda, e também antes do
 TK_EOF, para que a última linha termine com o marcador mesmo quando o
 arquivo não tiver quebra de linha no fim. A coluna do EOL é a posição logo
 depois do último lexema da linha, que é o que os diagnósticos usam quando
 falta um elemento no fim da linha.

 Linhas sem tokens não existem no .lex, então não geram marcador nenhum.

 Devolve 0 em caso de falha de memória ou de formato.
*/
static int carregar(FILE *entrada) {
    char linha[TAM_LEXEMA + TAM_NOME + 64];
    Token t;
    int linha_anterior = 0;
    int coluna_fim = 1;
    int tem_token = 0;
    int achou_eof = 0;

    while (fgets(linha, sizeof(linha), entrada) != NULL) {
        int i = 0;
        while (branco(linha[i])) {
            i++;
        }
        if (linha[i] == '\0') {
            continue;
        }

        if (!decompor(linha + i, &t)) {
            return 0;
        }

        if (strcmp(t.nome, TK_EOF) == 0) {
            achou_eof = 1;
            break;
        }

        if (tem_token && t.linha != linha_anterior) {
            if (!acrescentar(EOL, "", linha_anterior, coluna_fim)) {
                return 0;
            }
        }

        if (!acrescentar(t.nome, t.lexema, t.linha, t.coluna)) {
            return 0;
        }

        tem_token = 1;
        linha_anterior = t.linha;
        coluna_fim = t.coluna + (int) strlen(t.lexema);
    }

    if (tem_token) {
        if (!acrescentar(EOL, "", linha_anterior, coluna_fim)) {
            return 0;
        }
    }

    /* O TK_EOF entra uma vez só: o do arquivo, se existir, ou um criado aqui. */
    if (achou_eof) {
        if (!acrescentar(TK_EOF, "EOF", t.linha, t.coluna)) {
            return 0;
        }
    } else {
        if (!acrescentar(TK_EOF, "EOF", linha_anterior + 1, 1)) {
            return 0;
        }
    }

    atual = 0;
    return 1;
}

static void liberar_fluxo(void) {
    free(fluxo);
    fluxo = NULL;
    total = 0;
    capacidade = 0;
    atual = 0;
}

/*
 A árvore sintática concreta.

 Cada nó guarda o rótulo que vai para o arquivo .dot. Os não terminais usam
 o nome entre sinais de menor e maior, como <programa>; os terminais usam a
 categoria do token e, quando faz sentido, o lexema.

 Todos os nós criados ficam registrados em um vetor único, o que permite
 liberar tudo de uma vez no fim, sem precisar percorrer a árvore.
*/
typedef struct No {
    char nome[TAM_NOME];
    char lexema[TAM_LEXEMA];
    int terminal;
    int id;
    struct No **filhos;
    int total;
    int capacidade;
} No;

static No **criados = NULL;
static int qtd_criados = 0;
static int cap_criados = 0;

static FILE *saida_erros = NULL;
static int total_erros = 0;
static int falha = 0;          /* falha operacional: memória ou formato */

#define QTD(v) ((int) (sizeof(v) / sizeof((v)[0])))

static No *novo_no(const char *nome, const char *lexema, int terminal) {
    No *n;

    if (qtd_criados == cap_criados) {
        int nova = cap_criados == 0 ? 256 : cap_criados * 2;
        No **novo = realloc(criados, nova * sizeof(No *));
        if (novo == NULL) {
            falha = 1;
            return NULL;
        }
        criados = novo;
        cap_criados = nova;
    }

    n = calloc(1, sizeof(No));
    if (n == NULL) {
        falha = 1;
        return NULL;
    }

    strncpy(n->nome, nome, TAM_NOME - 1);
    strncpy(n->lexema, lexema, TAM_LEXEMA - 1);
    n->terminal = terminal;
    criados[qtd_criados++] = n;
    return n;
}

static void adicionar(No *pai, No *filho) {
    if (pai == NULL || filho == NULL) {
        return;
    }

    if (pai->total == pai->capacidade) {
        int nova = pai->capacidade == 0 ? 4 : pai->capacidade * 2;
        No **novo = realloc(pai->filhos, nova * sizeof(No *));
        if (novo == NULL) {
            falha = 1;
            return;
        }
        pai->filhos = novo;
        pai->capacidade = nova;
    }
    pai->filhos[pai->total++] = filho;
}

static void liberar_tudo(void) {
    int i;

    for (i = 0; i < qtd_criados; i++) {
        free(criados[i]->filhos);
        free(criados[i]);
    }
    free(criados);
    criados = NULL;
    qtd_criados = 0;
    cap_criados = 0;
    liberar_fluxo();
}

/*
 Diagnósticos.

 O texto de "esperado" vem de quem chamou. Para os terminais mais comuns a
 função descricao() monta a forma SMB_COM(","), que é a usada no enunciado.
*/
static const char *descricao(const char *categoria) {
    static const struct {
        const char *categoria;
        const char *texto;
    } tabela[] = {
        {"SMB_COM",    "SMB_COM(\",\")"},
        {"SMB_COL",    "SMB_COL(\":\")"},
        {"SMB_OPA",    "SMB_OPA(\"(\")"},
        {"SMB_CPA",    "SMB_CPA(\")\")"},
        {"DIR_DATA",   "DIR_DATA(\".data\")"},
        {"DIR_TEXT",   "DIR_TEXT(\".text\")"},
        {"DIR_GLOBL",  "DIR_GLOBL(\".globl\")"},
        {"DIR_WORD",   "DIR_WORD(\".word\")"},
        {"DIR_HALF",   "DIR_HALF(\".half\")"},
        {"DIR_BYTE",   "DIR_BYTE(\".byte\")"},
        {"DIR_ASCII",  "DIR_ASCII(\".ascii\")"},
        {"DIR_ASCIIZ", "DIR_ASCIIZ(\".asciiz\")"},
        {"DIR_SPACE",  "DIR_SPACE(\".space\")"},
        {"DIR_ALIGN",  "DIR_ALIGN(\".align\")"},
        {EOL,          "fim de linha"}
    };
    int i;

    for (i = 0; i < QTD(tabela); i++) {
        if (strcmp(categoria, tabela[i].categoria) == 0) {
            return tabela[i].texto;
        }
    }
    return categoria;
}

static Token *corrente(void) {
    return &fluxo[atual];
}

static int eh(const char *categoria) {
    return strcmp(corrente()->nome, categoria) == 0;
}

/* Nunca passa do TK_EOF, que é sempre o último token do fluxo. */
static void avancar(void) {
    if (atual < total - 1) {
        atual++;
    }
}

static void erro(const char *esperado) {
    Token *t = corrente();

    if (strcmp(t->nome, EOL) == 0) {
        fprintf(saida_erros, "Linha %d, coluna %d:\n", t->linha, t->coluna);
        fprintf(saida_erros, "Encontrado fim de linha; esperado %s.\n", esperado);
    } else if (strcmp(t->nome, TK_EOF) == 0) {
        fprintf(saida_erros, "Linha %d, coluna %d:\n", t->linha, t->coluna);
        fprintf(saida_erros, "Encontrado TK_EOF; esperado %s.\n", esperado);
    } else {
        fprintf(saida_erros, "Linha %d, coluna %d:\n", t->linha, t->coluna);
        fprintf(saida_erros, "Encontrado %s(\"%s\"); esperado %s.\n",
                t->nome, t->lexema, esperado);
    }
    total_erros++;
}

/*
 Recuperação: descarta o resto da linha e retoma na linha seguinte.

 Se o token atual já for o EOL, consome só ele. Se for o TK_EOF, não faz
 nada, porque não há mais linhas. Nos outros casos avança até o EOL e o
 consome. Em qualquer situação que não seja o fim do arquivo a posição
 avança pelo menos um token, o que impede repetir o mesmo diagnóstico para
 sempre. Tokens da linha seguinte nunca são descartados.
*/
static void recuperar(void) {
    if (eh(TK_EOF)) {
        return;
    }
    while (!eh(EOL) && !eh(TK_EOF)) {
        avancar();
    }
    if (eh(EOL)) {
        avancar();
    }
}

/* Cria a folha do token corrente e avança. */
static No *folha(void) {
    Token *t = corrente();
    const char *lexema = t->lexema;
    No *n;

    /* EOL e TK_EOF não têm lexema a exibir */
    if (strcmp(t->nome, EOL) == 0 || strcmp(t->nome, TK_EOF) == 0) {
        lexema = "";
    }

    n = novo_no(t->nome, lexema, 1);
    avancar();
    return n;
}

/* Consome o terminal esperado e o anexa ao pai; se não for ele, diagnostica. */
static int consumir(No *pai, const char *categoria, const char *esperado) {
    if (eh(categoria)) {
        adicionar(pai, folha());
        return 1;
    }
    erro(esperado);
    return 0;
}

/* Categorias de instrução agrupadas pela forma dos operandos. */
static const char *OP_TRES[] = {"INS_ADD", "INS_ADDU", "INS_SUB", "INS_SUBU",
                                "INS_MUL", "INS_AND", "INS_OR", "INS_XOR",
                                "INS_NOR", "INS_SLT"};
static const char *OP_DOIS[] = {"INS_MOVE", "INS_DIV"};
static const char *OP_UM[] = {"INS_MFLO", "INS_MFHI", "INS_JR"};
static const char *OP_DESL[] = {"INS_SLL", "INS_SRL"};
static const char *OP_MEM[] = {"INS_LW", "INS_SW", "INS_LB", "INS_SB",
                               "INS_LH", "INS_SH"};
static const char *OP_DESV[] = {"INS_BEQ", "INS_BNE", "INS_BLT", "INS_BLE",
                                "INS_BGT", "INS_BGE"};
static const char *OP_SALTO[] = {"INS_J", "INS_JAL"};
static const char *OP_SEM[] = {"INS_SYSCALL", "INS_NOP"};

static int pertence(const char **lista, int quantidade) {
    int i;

    for (i = 0; i < quantidade; i++) {
        if (eh(lista[i])) {
            return 1;
        }
    }
    return 0;
}

static int inicia_diretiva_dados(void) {
    return eh("DIR_WORD") || eh("DIR_HALF") || eh("DIR_BYTE")
        || eh("DIR_ASCII") || eh("DIR_ASCIIZ")
        || eh("DIR_SPACE") || eh("DIR_ALIGN");
}

static int inicia_instrucao(void) {
    return strncmp(corrente()->nome, "INS_", 4) == 0;
}

/*
 A partir daqui há uma função para cada não terminal da gramática. Todas
 devolvem o nó construído, ou NULL quando encontram um erro. A produção
 correspondente está no comentário acima de cada uma.
*/
static No *secao(void);
static No *secao_dados(void);
static No *secao_codigo(void);
static No *linha_dados(void);
static No *linha_codigo(void);
static No *rotulo(void);
static No *diretiva_global(void);
static No *diretiva_dados(void);
static No *diretiva_inteiros(void);
static No *lista_inteiros(void);
static No *diretiva_string(void);
static No *instrucao(void);
static No *instrucao_tres_registradores(void);
static No *op_tres_registradores(void);
static No *instrucao_dois_registradores(void);
static No *op_dois_registradores(void);
static No *instrucao_um_registrador(void);
static No *op_um_registrador(void);
static No *instrucao_imediato(void);
static No *instrucao_deslocamento(void);
static No *op_deslocamento(void);
static No *instrucao_li(void);
static No *instrucao_la(void);
static No *instrucao_memoria(void);
static No *op_memoria(void);
static No *endereco(void);
static No *instrucao_desvio(void);
static No *op_desvio(void);
static No *instrucao_salto(void);
static No *op_salto(void);
static No *instrucao_sem_operandos(void);

#define ABERTURA_SECAO "DIR_DATA(\".data\") ou DIR_TEXT(\".text\")"

/* <programa> ::= <secao> { <secao> } TK_EOF */
static No *programa(void) {
    No *n = novo_no("<programa>", "", 0);
    No *s;
    int tem_secao = 0;

    while (!eh(TK_EOF)) {
        if (eh("DIR_DATA") || eh("DIR_TEXT")) {
            s = secao();
            if (s != NULL) {
                adicionar(n, s);
                tem_secao = 1;
            } else {
                recuperar();
            }
        } else {
            erro(ABERTURA_SECAO);
            recuperar();
        }
    }

    /* Arquivo vazio ou sem nenhuma seção. O teste evita repetir o
       diagnóstico quando o problema já foi apontado acima. */
    if (!tem_secao && total_erros == 0) {
        erro(ABERTURA_SECAO);
    }

    consumir(n, TK_EOF, TK_EOF);
    return n;
}

/* <secao> ::= <secao_dados> | <secao_codigo> */
static No *secao(void) {
    No *n = novo_no("<secao>", "", 0);
    No *s = eh("DIR_DATA") ? secao_dados() : secao_codigo();

    if (s == NULL) {
        return NULL;
    }
    adicionar(n, s);
    return n;
}

/* <secao_dados> ::= ".data" EOL { <linha_dados> } */
static No *secao_dados(void) {
    No *n = novo_no("<secao_dados>", "", 0);

    if (!consumir(n, "DIR_DATA", descricao("DIR_DATA"))) {
        return NULL;
    }
    if (!consumir(n, EOL, descricao(EOL))) {
        recuperar();
    }

    /* A repetição só termina em uma nova seção ou no fim do arquivo. Uma
       construção não permitida aqui é erro, e não fim da seção. */
    while (!eh("DIR_DATA") && !eh("DIR_TEXT") && !eh(TK_EOF)) {
        No *l = linha_dados();
        if (l != NULL) {
            adicionar(n, l);
        } else {
            recuperar();
        }
    }
    return n;
}

/* <secao_codigo> ::= ".text" EOL { <linha_codigo> } */
static No *secao_codigo(void) {
    No *n = novo_no("<secao_codigo>", "", 0);

    if (!consumir(n, "DIR_TEXT", descricao("DIR_TEXT"))) {
        return NULL;
    }
    if (!consumir(n, EOL, descricao(EOL))) {
        recuperar();
    }

    while (!eh("DIR_DATA") && !eh("DIR_TEXT") && !eh(TK_EOF)) {
        No *l = linha_codigo();
        if (l != NULL) {
            adicionar(n, l);
        } else {
            recuperar();
        }
    }
    return n;
}

/* <linha_dados> ::= <rotulo> [ <diretiva_dados> ] EOL
                   | <diretiva_dados> EOL
                   | <diretiva_global> EOL */
static No *linha_dados(void) {
    No *n = novo_no("<linha_dados>", "", 0);
    No *f;

    if (eh("ID")) {
        f = rotulo();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);

        if (inicia_diretiva_dados()) {
            f = diretiva_dados();
            if (f == NULL) {
                return NULL;
            }
            adicionar(n, f);
        }
    } else if (inicia_diretiva_dados()) {
        f = diretiva_dados();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);
    } else if (eh("DIR_GLOBL")) {
        f = diretiva_global();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);
    } else {
        erro("rótulo, diretiva de dados ou DIR_GLOBL(\".globl\")");
        return NULL;
    }

    if (!consumir(n, EOL, descricao(EOL))) {
        return NULL;
    }
    return n;
}

/* <linha_codigo> ::= <rotulo> [ <instrucao> ] EOL
                    | <instrucao> EOL
                    | <diretiva_global> EOL */
static No *linha_codigo(void) {
    No *n = novo_no("<linha_codigo>", "", 0);
    No *f;

    if (eh("ID")) {
        f = rotulo();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);

        if (inicia_instrucao()) {
            f = instrucao();
            if (f == NULL) {
                return NULL;
            }
            adicionar(n, f);
        }
    } else if (inicia_instrucao()) {
        f = instrucao();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);
    } else if (eh("DIR_GLOBL")) {
        f = diretiva_global();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);
    } else {
        erro("rótulo, instrução ou DIR_GLOBL(\".globl\")");
        return NULL;
    }

    if (!consumir(n, EOL, descricao(EOL))) {
        return NULL;
    }
    return n;
}

/* <rotulo> ::= ID ":" */
static No *rotulo(void) {
    No *n = novo_no("<rotulo>", "", 0);

    if (!consumir(n, "ID", "ID")) {
        return NULL;
    }
    if (!consumir(n, "SMB_COL", descricao("SMB_COL"))) {
        return NULL;
    }
    return n;
}

/* <diretiva_global> ::= ".globl" ID */
static No *diretiva_global(void) {
    No *n = novo_no("<diretiva_global>", "", 0);

    if (!consumir(n, "DIR_GLOBL", descricao("DIR_GLOBL"))) {
        return NULL;
    }
    if (!consumir(n, "ID", "ID")) {
        return NULL;
    }
    return n;
}

/* <diretiva_dados> ::= <diretiva_inteiros> <lista_inteiros>
                      | <diretiva_string> STRING
                      | ".space" NUM_INT
                      | ".align" NUM_INT */
static No *diretiva_dados(void) {
    No *n = novo_no("<diretiva_dados>", "", 0);
    No *f;

    if (eh("DIR_WORD") || eh("DIR_HALF") || eh("DIR_BYTE")) {
        f = diretiva_inteiros();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);

        f = lista_inteiros();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);
    } else if (eh("DIR_ASCII") || eh("DIR_ASCIIZ")) {
        f = diretiva_string();
        if (f == NULL) {
            return NULL;
        }
        adicionar(n, f);

        if (!consumir(n, "STRING", "STRING")) {
            return NULL;
        }
    } else if (eh("DIR_SPACE")) {
        consumir(n, "DIR_SPACE", descricao("DIR_SPACE"));
        if (!consumir(n, "NUM_INT", "NUM_INT")) {
            return NULL;
        }
    } else if (eh("DIR_ALIGN")) {
        consumir(n, "DIR_ALIGN", descricao("DIR_ALIGN"));
        if (!consumir(n, "NUM_INT", "NUM_INT")) {
            return NULL;
        }
    } else {
        erro("diretiva de dados");
        return NULL;
    }
    return n;
}

/* <diretiva_inteiros> ::= ".word" | ".half" | ".byte" */
static No *diretiva_inteiros(void) {
    No *n = novo_no("<diretiva_inteiros>", "", 0);

    if (!eh("DIR_WORD") && !eh("DIR_HALF") && !eh("DIR_BYTE")) {
        erro("DIR_WORD(\".word\"), DIR_HALF(\".half\") ou DIR_BYTE(\".byte\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <lista_inteiros> ::= NUM_INT { "," NUM_INT } */
static No *lista_inteiros(void) {
    No *n = novo_no("<lista_inteiros>", "", 0);

    if (!consumir(n, "NUM_INT", "NUM_INT")) {
        return NULL;
    }
    while (eh("SMB_COM")) {
        consumir(n, "SMB_COM", descricao("SMB_COM"));
        if (!consumir(n, "NUM_INT", "NUM_INT")) {
            return NULL;
        }
    }
    return n;
}

/* <diretiva_string> ::= ".ascii" | ".asciiz" */
static No *diretiva_string(void) {
    No *n = novo_no("<diretiva_string>", "", 0);

    if (!eh("DIR_ASCII") && !eh("DIR_ASCIIZ")) {
        erro("DIR_ASCII(\".ascii\") ou DIR_ASCIIZ(\".asciiz\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao> ::= <instrucao_tres_registradores> | ... | <instrucao_sem_operandos> */
static No *instrucao(void) {
    No *n = novo_no("<instrucao>", "", 0);
    No *f;

    if (pertence(OP_TRES, QTD(OP_TRES))) {
        f = instrucao_tres_registradores();
    } else if (pertence(OP_DOIS, QTD(OP_DOIS))) {
        f = instrucao_dois_registradores();
    } else if (pertence(OP_UM, QTD(OP_UM))) {
        f = instrucao_um_registrador();
    } else if (eh("INS_ADDI")) {
        f = instrucao_imediato();
    } else if (pertence(OP_DESL, QTD(OP_DESL))) {
        f = instrucao_deslocamento();
    } else if (eh("INS_LI")) {
        f = instrucao_li();
    } else if (eh("INS_LA")) {
        f = instrucao_la();
    } else if (pertence(OP_MEM, QTD(OP_MEM))) {
        f = instrucao_memoria();
    } else if (pertence(OP_DESV, QTD(OP_DESV))) {
        f = instrucao_desvio();
    } else if (pertence(OP_SALTO, QTD(OP_SALTO))) {
        f = instrucao_salto();
    } else if (pertence(OP_SEM, QTD(OP_SEM))) {
        f = instrucao_sem_operandos();
    } else {
        erro("instrução");
        return NULL;
    }

    if (f == NULL) {
        return NULL;
    }
    adicionar(n, f);
    return n;
}

/* <instrucao_tres_registradores> ::= <op_tres_registradores> REG "," REG "," REG */
static No *instrucao_tres_registradores(void) {
    No *n = novo_no("<instrucao_tres_registradores>", "", 0);
    No *op = op_tres_registradores();

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "REG", "REG")) {
        return NULL;
    }
    return n;
}

/* <op_tres_registradores> ::= "add" | "addu" | "sub" | "subu" | "mul"
                             | "and" | "or" | "xor" | "nor" | "slt" */
static No *op_tres_registradores(void) {
    No *n = novo_no("<op_tres_registradores>", "", 0);

    if (!pertence(OP_TRES, QTD(OP_TRES))) {
        erro("operação de três registradores");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao_dois_registradores> ::= <op_dois_registradores> REG "," REG */
static No *instrucao_dois_registradores(void) {
    No *n = novo_no("<instrucao_dois_registradores>", "", 0);
    No *op = op_dois_registradores();

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "REG", "REG")) {
        return NULL;
    }
    return n;
}

/* <op_dois_registradores> ::= "move" | "div" */
static No *op_dois_registradores(void) {
    No *n = novo_no("<op_dois_registradores>", "", 0);

    if (!pertence(OP_DOIS, QTD(OP_DOIS))) {
        erro("INS_MOVE(\"move\") ou INS_DIV(\"div\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao_um_registrador> ::= <op_um_registrador> REG */
static No *instrucao_um_registrador(void) {
    No *n = novo_no("<instrucao_um_registrador>", "", 0);
    No *op = op_um_registrador();

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "REG", "REG")) {
        return NULL;
    }
    return n;
}

/* <op_um_registrador> ::= "mflo" | "mfhi" | "jr" */
static No *op_um_registrador(void) {
    No *n = novo_no("<op_um_registrador>", "", 0);

    if (!pertence(OP_UM, QTD(OP_UM))) {
        erro("INS_MFLO(\"mflo\"), INS_MFHI(\"mfhi\") ou INS_JR(\"jr\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao_imediato> ::= "addi" REG "," REG "," NUM_INT */
static No *instrucao_imediato(void) {
    No *n = novo_no("<instrucao_imediato>", "", 0);

    if (!consumir(n, "INS_ADDI", "INS_ADDI(\"addi\")")
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "NUM_INT", "NUM_INT")) {
        return NULL;
    }
    return n;
}

/* <instrucao_deslocamento> ::= <op_deslocamento> REG "," REG "," NUM_INT */
static No *instrucao_deslocamento(void) {
    No *n = novo_no("<instrucao_deslocamento>", "", 0);
    No *op = op_deslocamento();

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "NUM_INT", "NUM_INT")) {
        return NULL;
    }
    return n;
}

/* <op_deslocamento> ::= "sll" | "srl" */
static No *op_deslocamento(void) {
    No *n = novo_no("<op_deslocamento>", "", 0);

    if (!pertence(OP_DESL, QTD(OP_DESL))) {
        erro("INS_SLL(\"sll\") ou INS_SRL(\"srl\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao_li> ::= "li" REG "," NUM_INT */
static No *instrucao_li(void) {
    No *n = novo_no("<instrucao_li>", "", 0);

    if (!consumir(n, "INS_LI", "INS_LI(\"li\")")
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "NUM_INT", "NUM_INT")) {
        return NULL;
    }
    return n;
}

/* <instrucao_la> ::= "la" REG "," ID */
static No *instrucao_la(void) {
    No *n = novo_no("<instrucao_la>", "", 0);

    if (!consumir(n, "INS_LA", "INS_LA(\"la\")")
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "ID", "ID")) {
        return NULL;
    }
    return n;
}

/* <instrucao_memoria> ::= <op_memoria> REG "," <endereco> */
static No *instrucao_memoria(void) {
    No *n = novo_no("<instrucao_memoria>", "", 0);
    No *op = op_memoria();
    No *e;

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))) {
        return NULL;
    }

    e = endereco();
    if (e == NULL) {
        return NULL;
    }
    adicionar(n, e);
    return n;
}

/* <op_memoria> ::= "lw" | "sw" | "lb" | "sb" | "lh" | "sh" */
static No *op_memoria(void) {
    No *n = novo_no("<op_memoria>", "", 0);

    if (!pertence(OP_MEM, QTD(OP_MEM))) {
        erro("instrução de acesso à memória");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <endereco> ::= ID | NUM_INT "(" REG ")" */
static No *endereco(void) {
    No *n = novo_no("<endereco>", "", 0);

    if (eh("ID")) {
        adicionar(n, folha());
        return n;
    }

    if (!consumir(n, "NUM_INT", "ID ou NUM_INT")
        || !consumir(n, "SMB_OPA", descricao("SMB_OPA"))
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_CPA", descricao("SMB_CPA"))) {
        return NULL;
    }
    return n;
}

/* <instrucao_desvio> ::= <op_desvio> REG "," REG "," ID */
static No *instrucao_desvio(void) {
    No *n = novo_no("<instrucao_desvio>", "", 0);
    No *op = op_desvio();

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "REG", "REG")
        || !consumir(n, "SMB_COM", descricao("SMB_COM"))
        || !consumir(n, "ID", "ID")) {
        return NULL;
    }
    return n;
}

/* <op_desvio> ::= "beq" | "bne" | "blt" | "ble" | "bgt" | "bge" */
static No *op_desvio(void) {
    No *n = novo_no("<op_desvio>", "", 0);

    if (!pertence(OP_DESV, QTD(OP_DESV))) {
        erro("instrução de desvio condicional");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao_salto> ::= <op_salto> ID */
static No *instrucao_salto(void) {
    No *n = novo_no("<instrucao_salto>", "", 0);
    No *op = op_salto();

    if (op == NULL) {
        return NULL;
    }
    adicionar(n, op);

    if (!consumir(n, "ID", "ID")) {
        return NULL;
    }
    return n;
}

/* <op_salto> ::= "j" | "jal" */
static No *op_salto(void) {
    No *n = novo_no("<op_salto>", "", 0);

    if (!pertence(OP_SALTO, QTD(OP_SALTO))) {
        erro("INS_J(\"j\") ou INS_JAL(\"jal\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/* <instrucao_sem_operandos> ::= "syscall" | "nop" */
static No *instrucao_sem_operandos(void) {
    No *n = novo_no("<instrucao_sem_operandos>", "", 0);

    if (!pertence(OP_SEM, QTD(OP_SEM))) {
        erro("INS_SYSCALL(\"syscall\") ou INS_NOP(\"nop\")");
        return NULL;
    }
    adicionar(n, folha());
    return n;
}

/*
 Gravação da árvore em DOT.

 São duas passagens sobre a árvore: a primeira numera os nós e declara cada
 um com seu rótulo, a segunda escreve as ligações. A numeração em pré-ordem
 garante identificadores exclusivos e deixa o arquivo na mesma ordem do
 exemplo do enunciado.
*/
static void escapar(FILE *saida, const char *texto) {
    int i;

    for (i = 0; texto[i] != '\0'; i++) {
        if (texto[i] == '"' || texto[i] == '\\') {
            fputc('\\', saida);
        }
        fputc(texto[i], saida);
    }
}

static void declarar(FILE *saida, No *n, int *contador) {
    int i;

    n->id = (*contador)++;
    fprintf(saida, "    n%d [label=\"", n->id);
    escapar(saida, n->nome);
    if (n->terminal && n->lexema[0] != '\0') {
        fprintf(saida, "\\n");
        escapar(saida, n->lexema);
    }
    fprintf(saida, "\"];\n");

    for (i = 0; i < n->total; i++) {
        declarar(saida, n->filhos[i], contador);
    }
}

static void ligar(FILE *saida, No *n) {
    int i;

    for (i = 0; i < n->total; i++) {
        fprintf(saida, "    n%d -> n%d;\n", n->id, n->filhos[i]->id);
    }
    for (i = 0; i < n->total; i++) {
        ligar(saida, n->filhos[i]);
    }
}

static void escrever_dot(FILE *saida, No *raiz) {
    int contador = 0;

    fprintf(saida, "digraph ArvoreSintatica {\n");
    fprintf(saida, "    graph [ordering=out];\n");
    fprintf(saida, "    node [shape=box, fontname=\"Arial\"];\n");
    fprintf(saida, "    edge [arrowhead=none];\n\n");
    declarar(saida, raiz, &contador);
    fprintf(saida, "\n");
    ligar(saida, raiz);
    fprintf(saida, "}\n");
}

int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros) {
    No *raiz;
    int resultado;

    saida_erros = erros;
    total_erros = 0;
    falha = 0;

    if (!carregar(tokens)) {
        liberar_tudo();
        return SIN_FALHA;
    }

    raiz = programa();

    if (falha || raiz == NULL) {
        liberar_tudo();
        return SIN_FALHA;
    }

    if (total_erros == 0) {
        fprintf(erros, "Nenhum erro sintático encontrado.\n");
        escrever_dot(arvore, raiz);
        resultado = SIN_VALIDO;
    } else {
        /* Programa inválido: o .dot fica vazio, sem árvore parcial. */
        resultado = SIN_INVALIDO;
    }

    if (ferror(erros) || ferror(arvore)) {
        resultado = SIN_FALHA;
    }

    liberar_tudo();
    return resultado;
}
