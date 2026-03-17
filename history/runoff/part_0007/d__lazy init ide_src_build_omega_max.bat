@echo off
cls
echo ========================================
echo OMEGA-POLYGLOT MAX v3.1 Professional
echo AI-Augmented Reverse Engineering Suite
echo ========================================
echo.

if not exist "\masm32\bin\ml.exe" (
    echo [-] MASM32 not found at \masm32
    echo Please install MASM32 first
    exit /b 1
)

echo [+] Assembling omega_max_v31.asm...
\masm32\bin\ml /c /coff /Cp /nologo omega_max_v31.asm
if errorlevel 1 goto fail

echo [+] Linking omega_max_v31.obj...
\masm32\bin\link /SUBSYSTEM:CONSOLE /nologo omega_max_v31.obj
if errorlevel 1 goto fail

echo.
echo [+] Build successful: omega_max_v31.exe
echo [+] Ready for professional reverse engineering
echo.
echo Usage: omega_max_v31.exe
echo.
goto end

:fail
echo [-] Build failed
exit /b 1

:end
pause
