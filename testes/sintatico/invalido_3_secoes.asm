# Instrucao em .data, diretiva de dados em .text e linha fora de secao
li $t0, 1
.data
valor: .word 1
syscall
.text
main:
    .word 10
    nop
