# Secao .text direta, rotulos e todos os desvios e saltos

.text
.globl main
main:
    li   $t0, 0
    li   $t1, 10

loop:
    addi $t0, $t0, 1
    beq  $t0, $t1, fim
    bne  $t0, $t1, loop
    blt  $t0, $t1, fim
    ble  $t0, $t1, fim
    bgt  $t0, $t1, fim
    bge  $t0, $t1, fim
    j    chamada

chamada:
    jal  rotina
    nop
    j    fim

rotina:
    jr   $ra

fim:
    li   $v0, 10
    syscall
