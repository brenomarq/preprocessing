@echo off
REM ==========================================================================
REM executar_testes.bat - bateria de testes do pre-processador (Windows)
REM
REM Uso: na pasta do projeto, execute   testes\executar_testes.bat
REM ==========================================================================
setlocal enabledelayedexpansion

cd /d "%~dp0.."

set TOTAL=0
set PASSOU=0

echo.
echo ==========================================
echo  Compilando
echo ==========================================
gcc -Wall -Wextra -std=c99 main.c preprocessador.c -o main.exe
if errorlevel 1 (
    echo ERRO DE COMPILACAO
    exit /b 1
)
echo Compilado: main.exe

if not exist "testes\saida" mkdir "testes\saida"

echo.
echo ==========================================
echo  Casos de pre-processamento
echo ==========================================

for %%A in (testes\*.asm) do (
    set /a TOTAL+=1
    set NOME=%%~nA
    main.exe "%%A" "testes\saida\!NOME!.pre" >nul 2>nul

    REM /A compara texto ignorando diferencas de quebra de linha
    fc /A "testes\!NOME!.esperado" "testes\saida\!NOME!.pre" >nul 2>nul
    if errorlevel 1 (
        echo FALHOU  !NOME!
        fc "testes\!NOME!.esperado" "testes\saida\!NOME!.pre"
    ) else (
        echo OK      !NOME!
        set /a PASSOU+=1
    )
)

echo.
echo ==========================================
echo  Casos de erro ^(devem terminar com falha^)
echo ==========================================

call :VerificaErro "sem argumentos"
call :VerificaErro "apenas um argumento" testes\05_rotulos.asm
call :VerificaErro "argumentos demais" a.asm b.pre c.txt
call :VerificaErro "entrada inexistente" nao_existe.asm testes\saida\x.pre
call :VerificaErro "entrada igual a saida" testes\05_rotulos.asm testes\05_rotulos.asm

echo.
echo ==========================================
echo  Resultado: %PASSOU% de %TOTAL% testes passaram
echo ==========================================
echo.
exit /b 0

:VerificaErro
set /a TOTAL+=1
main.exe %2 %3 %4 >nul 2>nul
if errorlevel 1 (
    echo OK      %~1
    set /a PASSOU+=1
) else (
    echo FALHOU  %~1 ^(deveria ter retornado erro^)
)
exit /b 0
