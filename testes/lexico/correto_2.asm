# Calcula o maior entre dois numeros usando uma sub-rotina
.text
.globl main
main:
    li   $a0, 17
    li   $a1, 42
    jal  maior
    move $s0, $v0

    li   $v0, 10
    syscall

maior:
    slt  $t0, $a0, $a1
    beq  $t0, $zero, primeiro
    move $v0, $a1
    jr   $ra
primeiro:
    move $v0, $a0
    jr   $ra
