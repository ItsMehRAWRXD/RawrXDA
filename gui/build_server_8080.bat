@echo off
:: Build RawrXD Backend Server (MASM x64) — Node parity on port 8080
:: Requires: ml64 and link in PATH (VS x64 Native Tools Command Prompt or vcvars64.bat)
setlocal
cd /d "%~dp0"

set ASM=server_8080.asm
set OBJ=server_8080.obj
set EXE=server_8080.exe

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
ml64 /c /nologo /Zi /Fo"%OBJ%" "%ASM%"
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
echo [OK] %EXE% built. Run tool_server (or Win32 IDE) on 11435, then server_8080.exe
echo      Serves http://localhost:8080 — GUI static; /api/* proxied to Win32 backend :11435
exit /b 0
