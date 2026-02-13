@echo off
cls
echo ========================================
echo OMEGA-POLYGLOT PRO v4.0 Build Script
echo Professional Reverse Engineering Suite
echo ========================================
echo.

REM Check for MASM32
if not exist "\masm32\bin\ml.exe" (
    echo [-] ERROR: MASM32 not found at \masm32
    echo [-] Please install MASM32 first
    exit /b 1
)

echo [+] Assembling omega_pro_v4.asm...
\masm32\bin\ml /c /coff /Cp /Zd /Zi omega_pro_v4.asm
if errorlevel 1 (
    echo [-] Assembly failed
    exit /b 1
)

echo [+] Linking omega_pro_v4.obj...
\masm32\bin\link /SUBSYSTEM:CONSOLE /DEBUG omega_pro_v4.obj
if errorlevel 1 (
    echo [-] Linking failed
    exit /b 1
)

echo.
echo [+] Build successful!
echo [+] Executable: omega_pro_v4.exe
echo.
echo [+] Running OMEGA-POLYGLOT PRO v4.0...
echo.
omega_pro_v4.exe

pause
