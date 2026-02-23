@echo off
REM Standalone MASM x64/x86 compiler/linker launcher.
REM Usage: build_masm_standalone.bat [x64|x86] [path\to\source.asm]
REM   Or:  build_masm_standalone.bat x64
REM        (builds from current dir or toolchain\masm\samples)
setlocal
set "ARCH=x64"
set "SRC="
if "%~1"=="x86" set "ARCH=x86" & shift
if "%~1"=="x64" set "ARCH=x64" & shift
if not "%~1"=="" set "SRC=%~1"
cd /d "%~dp0"
set "PWSH=powershell.exe"
where pwsh.exe >nul 2>&1 && set "PWSH=pwsh.exe"
if "%SRC%"=="" set "SRC=%~dp0samples\hello_masm.asm"
if not exist "%SRC%" (
    echo No source specified and samples\hello_masm.asm not found.
    echo Usage: %~nx0 [x64^|x86] [path\to\source.asm]
    exit /b 1
)
"%PWSH%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Unified-PowerShell-Compiler-RawrXD.ps1" -Source "%SRC%" -Tool masm -Architecture %ARCH% -SubSystem console -Entry main
exit /b %ERRORLEVEL%
