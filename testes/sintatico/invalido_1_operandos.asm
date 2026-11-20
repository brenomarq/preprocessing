# Operandos ausentes, excedentes, fora de ordem e de categoria inadequada
.data
mensagem: .asciiz "texto"
.text
main:
    add  $t0, $t1
    li   $t0, mensagem
    li   $t0, 10, 20
    add  $t0, 5, $t1
    mflo
    syscall $t0
    li   $v0, 10
    syscall
