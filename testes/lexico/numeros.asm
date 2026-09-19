# Numeros decimais, negativos e hexadecimais
.data
decimais:     .word 0, 15, 2025
negativos:    .word -1, -20, -32768
hexadecimais: .word 0x10, 0xFF, 0x10010000, 0XAbC
hex_negativo: .word -0x10, -0x20
.text
main:
    li   $t0, 25
    li   $t1, -4
    addi $t2, $t1, 0xFF
    li   $t3, -0x20
