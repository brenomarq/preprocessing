# Numeros, diretivas e identificadores malformados
.dados
valor: .word 0xG1
outro: .word 0x
real:  .word 12.5
.text
main:
    li   $t0, 2valor
    j    soma-total
    j    inicio@
    li   $t1, - 5
