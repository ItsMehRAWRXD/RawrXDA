@echo off
setlocal enabledelayedexpansion

echo [*] Building RawrXD PE Generator & Encoder (MASM x64)

if not exist "%~dp0bin" mkdir "%~dp0bin" >nul 2>&1

set MASM=ml64.exe
set LINKER=link.exe

where %MASM% >nul 2>&1
if errorlevel 1 (
    echo [!] ml64.exe not found in PATH. Run VsDevCmd.bat or Developer Command Prompt.
    exit /b 1
)

where %LINKER% >nul 2>&1
if errorlevel 1 (
    echo [!] link.exe not found in PATH. Run VsDevCmd.bat or Developer Command Prompt.
    exit /b 1
)

echo [+] Assembling rawrxd_pe_generator_encoder.asm
%MASM% /nologo /c /Fo:"%~dp0bin\rawrxd_pe_generator_encoder.obj" "%~dp0rawrxd_pe_generator_encoder.asm"
if errorlevel 1 goto :fail

echo [+] Assembling pe_generator_example.asm
%MASM% /nologo /c /Fo:"%~dp0bin\pe_generator_example.obj" "%~dp0pe_generator_example.asm"
if errorlevel 1 goto :fail

echo [+] Linking pe_generator_example.exe
%LINKER% /nologo /SUBSYSTEM:CONSOLE /ENTRY:Main ^
    /OUT:"%~dp0bin\pe_generator_example.exe" ^
    "%~dp0bin\rawrxd_pe_generator_encoder.obj" ^
    "%~dp0bin\pe_generator_example.obj" ^
    kernel32.lib user32.lib bcrypt.lib
if errorlevel 1 goto :fail

echo [✓] Build succeeded. Output in .\bin
exit /b 0

:fail
echo [x] Build failed.
exit /b 1
