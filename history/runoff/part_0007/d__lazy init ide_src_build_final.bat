@echo off
cls
echo [+] Building OMEGA-POLYGLOT MAXIMUM v3.0 PRO...
echo [+] Professional Reverse Engineering Suite
echo.

C:\masm32\bin\ml /c /coff /Cp omega_final.asm
if errorlevel 1 goto error

C:\masm32\bin\link /SUBSYSTEM:CONSOLE omega_final.obj
if errorlevel 1 goto error

echo.
echo [+] SUCCESS: omega_final.exe created
echo [+] Professional PE Analysis Tool Ready
echo [+] Features: PE32/PE32+ Support, Import Analysis, Section Analysis, String Extraction
goto end

:error
echo.
echo [-] Build failed - check for errors
pause

:end