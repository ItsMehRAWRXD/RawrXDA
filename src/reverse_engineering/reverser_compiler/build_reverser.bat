@echo off
REM =============================================================================
REM Reverser Compiler Suite — Build System
REM =============================================================================
REM
REM Builds the self-hosting "Reverser" language compiler written in pure assembly.
REM
REM Components:
REM   reverser_lexer.asm       — Tokenizer / lexical analyzer
REM   reverser_parser.asm      — Recursive descent parser (AST construction)
REM   reverser_ast.asm         — AST node definitions and manipulation
REM   reverser_bytecode_gen.asm — Bytecode generator for Reverser IR
REM   reverser_compiler.asm    — Main compiler driver
REM   reverser_runtime.asm     — Runtime library for compiled programs
REM   reverser_vtable.asm      — Virtual dispatch table implementation
REM   reverser_platform.asm    — Platform abstraction (Linux/Windows syscalls)
REM   reverser_syscalls.asm    — Raw syscall wrappers
REM
REM Requires: NASM (Netwide Assembler) in PATH or C:\nasm\
REM
REM Usage:
REM   build_reverser.bat [all|compiler|runtime|tests|clean]
REM
REM =============================================================================

setlocal enabledelayedexpansion

REM Try to find NASM
set NASM=nasm
where nasm >nul 2>&1
if errorlevel 1 (
    if exist "C:\nasm\nasm.exe" (
        set NASM=C:\nasm\nasm.exe
    ) else if exist "C:\nasm\nasm-2.16.03\nasm.exe" (
        set NASM=C:\nasm\nasm-2.16.03\nasm.exe
    ) else (
        echo [ERROR] NASM not found. Install from https://www.nasm.us/
        exit /b 1
    )
)

set OUTDIR=%~dp0..\..\..\..\bin\re_tools\reverser
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set TARGET=%~1
if "%TARGET%"=="" set TARGET=all

if "%TARGET%"=="clean" (
    echo [CLEAN] Removing build artifacts...
    del /q "%OUTDIR%\*.obj" 2>nul
    del /q "%OUTDIR%\*.exe" 2>nul
    echo [DONE]
    exit /b 0
)

if "%TARGET%"=="all" goto :build_all
if "%TARGET%"=="compiler" goto :build_compiler
if "%TARGET%"=="runtime" goto :build_runtime
if "%TARGET%"=="tests" goto :build_tests
echo [ERROR] Unknown target: %TARGET%
exit /b 1

:build_all
call :build_compiler
call :build_runtime
echo.
echo [DONE] Reverser compiler suite built in %OUTDIR%
exit /b 0

:build_compiler
echo.
echo [BUILD] Reverser Compiler — Core Components
echo =============================================
for %%F in (reverser_lexer reverser_parser reverser_ast reverser_bytecode_gen reverser_compiler reverser_vtable) do (
    echo   [ASM] %%F.asm
    "%NASM%" -f win64 -o "%OUTDIR%\%%F.obj" "%%F.asm"
    if errorlevel 1 (
        echo   [FAIL] %%F.asm
        exit /b 1
    )
)
echo [OK] Compiler objects built
exit /b 0

:build_runtime
echo.
echo [BUILD] Reverser Runtime Library
echo =================================
for %%F in (reverser_runtime reverser_platform reverser_syscalls) do (
    echo   [ASM] %%F.asm
    "%NASM%" -f win64 -o "%OUTDIR%\%%F.obj" "%%F.asm"
    if errorlevel 1 (
        echo   [FAIL] %%F.asm
        exit /b 1
    )
)
echo [OK] Runtime objects built
exit /b 0

:build_tests
echo.
echo [BUILD] Reverser Test Suite
echo ============================
for %%F in (tests\reverser_test_suite tests\test_reverser_lexer tests\test_reverser_parser tests\test_reverser_runtime tests\test_reverser_platform tests\test_reverser_vtable) do (
    echo   [ASM] %%F.asm
    "%NASM%" -f win64 -o "%OUTDIR%\%%~nF.obj" "%%F.asm"
    if errorlevel 1 (
        echo   [FAIL] %%F.asm
        exit /b 1
    )
)
echo [OK] Test objects built
exit /b 0
