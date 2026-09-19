# Usa as demais diretivas e as operacoes aritmeticas, logicas e de desvio
.data
.align 2
vetor:  .word 3, 7, 12, 20
metade: .half 1000
letra:  .byte 65
buffer: .space 16
titulo: .ascii "Tabela"

.text
main:
    la   $s0, vetor
    lw   $t0, 0($s0)
    lw   $t1, 4($s0)
    sw   $t1, 8($s0)
    lb   $t2, 0($s0)
    sb   $t2, 1($s0)
    lh   $t3, 0($s0)
    sh   $t3, 2($s0)
    sub  $t2, $t0, $t1
    mul  $t3, $t0, $t1
    div  $t0, $t1
    mflo $t4
    mfhi $t5
    and  $t6, $t0, $t1
    or   $t7, $t0, $t1
    xor  $t8, $t0, $t1
    nor  $t9, $t0, $t1
    sll  $s1, $t0, 2
    srl  $s2, $t0, 1
    addu $s3, $s1, $s2
    subu $s4, $s1, $s2
    addi $s5, $s4, 1
    bne  $s3, $s4, pula
    nop
pula:
    ble  $s3, $s4, fim
    bgt  $s3, $zero, fim
    blt  $s3, $s4, fim
    bge  $s4, $zero, fim
    j    fim
fim:
    li   $v0, 10
    syscall
