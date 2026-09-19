# Soma dois valores e imprime o resultado
.data
mensagem: .asciiz "Resultado: "
valor:    .word 10

.text
.globl main
main:
    la   $a0, mensagem
    li   $v0, 4
    syscall

    lw   $t0, valor
    li   $t1, 5
    add  $t2, $t0, $t1

    move $a0, $t2
    li   $v0, 1
    syscall

    li   $v0, 10
    syscall
