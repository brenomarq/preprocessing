# Virgulas, dois-pontos e parenteses ausentes
.data
lista: .word 10,
outra: .word 1 2
.text
main
    lw   $t0, 4 $sp
    lw   $t1, 4($sp
    add  $t2 $t0, $t1
    nop
