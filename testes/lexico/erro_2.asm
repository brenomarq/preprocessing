# Problemas em cadeias de caracteres
.data
ok:      .asciiz "Esta cadeia esta correta"
aberta:  .asciiz "Mensagem nao encerrada
escape:  .asciiz "Sequencia \q invalida"
.text
main:
    li $v0, 10
    syscall
