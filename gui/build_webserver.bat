@echo off
:: Build RawrXD MASM Web Server (webserver.exe) from gui\webserver.asm
:: Requires: ml64 and link in PATH (run from VS x64 Native Tools Command Prompt or after vcvars64.bat)
setlocal
cd /d "%~dp0"

set ASM=webserver.asm
set OBJ=webserver.obj
set EXE=webserver.exe

where ml64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64 not in PATH. Open "x64 Native Tools Command Prompt for VS" or run vcvars64.bat
    exit /b 1
)

if not exist "%ASM%" (
    echo [ERROR] %ASM% not found in %CD%
    exit /b 1
)

echo Building %EXE% from %ASM%...
ml64 /c /nologo /Fo"%OBJ%" "%ASM%"
if errorlevel 1 (
    echo [ERROR] Assembly failed.
    exit /b 1
)

link /nologo /subsystem:console /entry:start "%OBJ%" kernel32.lib ws2_32.lib /out:"%EXE%"
if errorlevel 1 (
    echo [ERROR] Link failed.
    del "%OBJ%" 2>nul
    exit /b 1
)

del "%OBJ%" 2>nul
echo [OK] %EXE% built. Run it to serve GUI at http://localhost:3000 (HTTPS via reverse proxy).
exit /b 0
