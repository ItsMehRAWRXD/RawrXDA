@echo off
cls
echo ========================================================
echo   OMEGA-POLYGLOT MAXIMUM v3.0 PRO - Professional Build
echo ========================================================
echo [+] Claude/Moonshot/DeepSeek Professional Reverse Engineering Suite
echo [+] Building production-ready PE analysis tool...
echo.

REM Clean previous builds
if exist omega_final_working.obj del omega_final_working.obj
if exist omega_final_working.exe del omega_final_working.exe

REM Assemble source code
echo [*] Assembling source code...
C:\masm32\bin\ml /c /coff /Cp omega_final_working.asm

REM Check assembly result
if not exist omega_final_working.obj goto assembly_failed
if %errorlevel% neq 0 goto assembly_failed

echo [+] Assembly successful

REM Link object file  
echo [*] Linking executable...
C:\masm32\bin\link /SUBSYSTEM:CONSOLE /FIXED:NO /DYNAMICBASE:NO omega_final_working.obj

REM Check linking result
if not exist omega_final_working.exe goto linking_failed
if %errorlevel% neq 0 goto linking_failed

echo [+] Linking successful
echo.
echo ========================================================
echo [+] OMEGA-POLYGLOT MAXIMUM v3.0 PRO BUILD COMPLETE
echo ========================================================
echo [+] Executable: omega_final_working.exe
echo [+] Features:   PE32/PE32+ Analysis
echo [+]             Import/Export Reconstruction  
echo [+]             Section Analysis
echo [+]             String Extraction
echo [+]             Professional UI
echo [+] AI Ready:   Claude | Moonshot | DeepSeek | Codex
echo ========================================================
goto end

:assembly_failed
echo.
echo [-] ASSEMBLY FAILED
echo [-] Check source code for syntax errors
echo [-] Common issues: undefined symbols, syntax errors
pause
goto end

:linking_failed
echo.
echo [-] LINKING FAILED  
echo [-] Check for unresolved external symbols
echo [-] Ensure all required libraries are available
pause
goto end

:end