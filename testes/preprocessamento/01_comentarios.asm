# Comentario ocupando a linha inteira
    # comentario indentado
.data
msg: .asciiz "Resultado # obtido"   # o # do texto acima deve continuar
outra: .asciiz "# comeca com cerquilha"
.text
main:
    add $t0, $t1, $t2     # realiza a soma
    li $t3, 10#comentario colado na instrucao
    sub $t4, $t0, $t3
#
