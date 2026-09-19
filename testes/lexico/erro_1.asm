# Caractere invalido e registradores fora do conjunto aceito
.text
main:
    li   $t0, 10
    add  $t1, $t0, @
    li   $t10, 5
    move $32, $x1
    li   $v0, 10
    syscall
