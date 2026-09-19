# Acesso a memoria no formato deslocamento(registrador)
.data
vetor: .word 1, 2, 3, 4
.text
main:
    la   $t0, vetor
    lw   $t1, 0($t0)
    lw   $t2, 4($t0)
    sw   $t2, 8($t0)
    lb   $t3, 1($t0)
    sb   $t3, 0($t0)
    lh   $t4, 2($t0)
    sh   $t4, 12($t0)
    lw   $t5, -4($sp)
    sw   $ra, 0($sp)
    lw   $8, 0($29)
    li   $v0, 10
    syscall
