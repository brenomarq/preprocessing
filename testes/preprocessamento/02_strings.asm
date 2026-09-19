.data
espacos:  .asciiz "A     B"
pontuacao: .asciiz "Resultado: , ; : ( ) # 1"
vazia:    .asciiz ""
aspas:    .asciiz "ele disse \"ola\" e saiu"
tab_dentro: .asciiz "antes	depois"
duas:     .asciiz "primeira"   # e um comentario, nao uma segunda string
.text
main:
    li   $v0, 4
