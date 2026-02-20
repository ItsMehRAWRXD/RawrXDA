@echo off
cls
echo ========================================
echo OMEGA-POLYGLOT PRO v3.0 Builder
echo ========================================
echo.

echo [1/3] Cleaning...
del omega_pro_v3.obj 2>nul
del omega_pro_v3.exe 2>nul

echo [2/3] Assembling...
C:\masm32\bin\ml.exe /c /coff omega_pro_v3.asm
if errorlevel 1 (
    echo [!] Assembly failed
    pause
    exit /b 1
)

echo [3/3] Linking...
C:\masm32\bin\link.exe /SUBSYSTEM:CONSOLE /OUT:omega_pro_v3.exe omega_pro_v3.obj C:\masm32\lib\kernel32.lib C:\masm32\lib\user32.lib C:\masm32\lib\msvcrt.lib
if errorlevel 1 (
    echo [!] Linking failed
    pause
    exit /b 1
)

del omega_pro_v3.obj 2>nul

echo.
echo ========================================
echo [+] BUILD COMPLETE: omega_pro_v3.exe
echo ========================================
echo.
echo Professional Reverse Engineering Features:
echo   - Full PE32 Parsing (DOS/NT/Optional Headers)
echo   - Accurate RVA-to-FileOffset Translation
echo   - Import Table Reconstruction (IAT/ILT)
echo   - Export Table Enumeration
echo   - Section Analysis with Characteristics
echo   - Shannon Entropy Calculation
echo   - Professional Hex Dump with ASCII
echo   - Basic Disassembly Engine
echo   - Source Reconstruction Framework
echo   - 50 Language Detection Constants
echo   - 10 Packer Detection Signatures
echo.
echo Usage: omega_pro_v3.exe
echo.
pause
