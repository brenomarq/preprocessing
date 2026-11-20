# Erros em linhas distintas: as construcoes seguintes continuam reconhecidas
.text
main:
    li   $t0,
    li   $t1, 20
    add  $t2, $t0
    add  $t3, $t0, $t1
    beq  $t0 $t1, fim
    bne  $t0, $t1, fim
fim:
    syscall
