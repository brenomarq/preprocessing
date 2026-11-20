# Todas as diretivas de dados e todas as formas de operando

.data
numero:    .word 10
vetor:     .word 10, 20, -30, 0xFF
negativos: .word -1, -0x20
pequeno:   .byte 65
bytes:     .byte 1, 2, 3
valor:     .half 1000
metades:   .half 100, 200
mensagem:  .asciiz "Resultado: "
texto:     .ascii "ABC"
buffer:    .space 100
.align 2
.globl numero

.text
main:
    la $a0, mensagem
    li $v0, 4
    syscall
