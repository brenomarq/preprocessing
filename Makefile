# ==========================================================================
# Makefile - atalhos para compilar e testar (macOS / Linux)
#
#   make            compila o programa
#   make testes     compila e roda a bateria de testes
#   make exemplo    roda o exemplo do enunciado e mostra entrada x saida
#   make limpar     apaga os arquivos gerados
#
# No Windows, use diretamente:
#   gcc main.c preprocessador.c -o main.exe
# ==========================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic
ALVO    = main.exe
FONTES  = main.c preprocessador.c

$(ALVO): $(FONTES) preprocessador.h
	$(CC) $(CFLAGS) $(FONTES) -o $(ALVO)

testes: $(ALVO)
	sh testes/executar_testes.sh

exemplo: $(ALVO)
	@mkdir -p testes/saida
	@./$(ALVO) testes/06_exemplo_enunciado.asm testes/saida/exemplo.pre
	@echo ""
	@echo "----- ENTRADA -----"
	@cat testes/06_exemplo_enunciado.asm
	@echo "----- SAIDA -----"
	@cat testes/saida/exemplo.pre

limpar:
	rm -f $(ALVO)
	rm -rf testes/saida

.PHONY: testes exemplo limpar
