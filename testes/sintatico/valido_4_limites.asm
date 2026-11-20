# Rotulo inexistente e rotulo declarado duas vezes.
# Sao validos nesta etapa: a verificacao pertence a analise semantica.
.text
main:
    j    nao_existe
    beq  $t0, $t1, tambem_nao_existe
repetido:
    nop
repetido:
    nop
