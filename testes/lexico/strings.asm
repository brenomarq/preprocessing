# Cadeias com espacos, simbolos e todas as sequencias de escape aceitas
.data
simples:   .asciiz "Resultado: "
espacos:   .asciiz "A     B"
simbolos:  .asciiz "virgula, dois-pontos: (parenteses) #1"
quebra:    .asciiz "linha 1\nlinha 2"
tabulacao: .asciiz "coluna\tcoluna"
aspas:     .asciiz "ele disse \"ola\""
barra:     .asciiz "C:\\pasta"
nulo:      .ascii  "fim\0"
vazia:     .asciiz ""
