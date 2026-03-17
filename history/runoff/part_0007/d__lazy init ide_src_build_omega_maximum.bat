@echo off
cls
echo [+] OMEGA-POLYGLOT MAXIMUM v3.0 PRO...
echo [+] Claude/Moonshot/DeepSeek Professional Reverse Engineering...
echo.

REM Assemble the source file
C:\masm32\bin\ml /c /coff /Cp omega_pro_maximum.asm

REM Check if assembly was successful
if errorlevel 1 goto error

REM Link the object file
C:\masm32\bin\link /SUBSYSTEM:CONSOLE /FIXED:NO /DYNAMICBASE:NO omega_pro_maximum.obj

REM Check if linking was successful  
if errorlevel 1 goto error

echo.
echo [+] Production Ready: omega_pro_maximum.exe
echo [+] Professional PE Analysis Suite Complete
echo [+] Features: PE32/PE32+ Analysis, Shannon Entropy, Import/Export Reconstruction
echo [+] Ready for: Claude, Moonshot, DeepSeek, Codex Integration
goto end

:error
echo.
echo [-] Build Failed - Check source code for errors
pause
goto end

:end