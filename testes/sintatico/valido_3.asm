# Alternancia de secoes, secao vazia, acesso a memoria e os demais grupos

.data

.text
.data
dados: .word 1, 2, 3

.text
main:
    la   $s0, dados
    lw   $t0, dados
    lw   $t1, 0($s0)
    sw   $t1, 4($s0)
    lb   $t2, 8($s0)
    sb   $t2, -4($sp)
    lh   $t3, 2($s0)
    sh   $t3, 0($sp)

    add  $t4, $t0, $t1
    addu $t5, $t0, $t1
    sub  $t6, $t0, $t1
    subu $t7, $t0, $t1
    mul  $t8, $t0, $t1
    and  $t9, $t0, $t1
    or   $s1, $t0, $t1
    xor  $s2, $t0, $t1
    nor  $s3, $t0, $t1
    slt  $s4, $t0, $t1

    move $s5, $t0
    div  $t0, $t1
    mflo $s6
    mfhi $s7

    addi $k0, $t0, 100
    sll  $k1, $t0, 2
    srl  $gp, $t0, 1
    li   $fp, 0x10010000
