# Atalhos para compilar e testar no macOS/Linux.
# No Windows: gcc main.c preprocessador.c -o main.exe

CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra
ALVO   = main.exe
FONTES = main.c preprocessador.c

$(ALVO): $(FONTES) preprocessador.h
	$(CC) $(CFLAGS) $(FONTES) -o $(ALVO)

testes: $(ALVO)
	sh testes/executar_testes.sh

limpar:
	rm -f $(ALVO)
	rm -rf testes/saida

.PHONY: testes limpar
