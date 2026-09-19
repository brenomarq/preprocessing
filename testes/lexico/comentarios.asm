# Comentario ocupando a linha inteira
.data # comentario depois de uma diretiva
msg: .asciiz "O # dentro da string nao e comentario" # mas este e
.text
main:   # comentario depois de um rotulo
    li $v0, 10  # carrega o codigo de saida
    syscall
# comentario na ultima linha
