# Atalhos para o trabalho com Makefile.
# No Windows, compilar direto com: gcc main.c preprocessador.c lexico.c -o main.exe

CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra
ALVO   = main.exe
FONTES = main.c preprocessador.c lexico.c

# make          compila
# make testes   roda o programa em todos os arquivos de teste
# make limpar   apaga o executável e as saídas geradas

$(ALVO): $(FONTES) preprocessador.h lexico.h
	$(CC) $(CFLAGS) $(FONTES) -o $(ALVO)

compilar: $(ALVO)

testes: $(ALVO)
	@mkdir -p testes/saida
	@for arquivo in testes/preprocessamento/*.asm testes/lexico/*.asm; do \
		nome=`basename $$arquivo .asm`; \
		echo "=== $$nome ==="; \
		./$(ALVO) $$arquivo testes/saida/$$nome.lex; \
		echo ""; \
	done
	@echo "Arquivos gerados em testes/saida/ (.pre, .lex, .ts e .err)"

limpar:
	rm -f $(ALVO)
	rm -rf testes/saida

.PHONY: compilar testes limpar
