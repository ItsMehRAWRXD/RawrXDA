@echo off
cls
echo [+] OMEGA-POLYGLOT v4.0 PRO Build
echo.
C:\masm32\bin\ml /c /coff /Cp OmegaPolyglot_v4.asm
if errorlevel 1 (
    echo [-] Assembly failed
    pause
    exit /b 1
)
C:\masm32\bin\link /SUBSYSTEM:CONSOLE OmegaPolyglot_v4.obj
if errorlevel 1 (
    echo [-] Linking failed
    pause
    exit /b 1
)
echo [+] Build successful: OmegaPolyglot_v4.exe
echo.
pause