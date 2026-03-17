@echo off
REM =============================================================================
REM OMEGA-POLYGLOT PE Analyzer Suite — Build System
REM =============================================================================
REM
REM Builds all versions of the OMEGA reverse engineering tools.
REM Requires: MASM32 at C:\masm32\
REM
REM Targets:
REM   omega_pro_v3.exe   — OMEGA-POLYGLOT v3.0P (Professional Reverse Edition)
REM   omega_v4.exe       — OMEGA-POLYGLOT v4.0 (Multi-Language Polyglot)
REM   omega_max.exe      — OMEGA-POLYGLOT MAX v4.0 PRO
REM   omega_simple.exe   — OMEGA-POLYGLOT SIMPLE v1.0 (Basic PE Analysis)
REM
REM Usage:
REM   build_omega_suite.bat [target]
REM   build_omega_suite.bat all
REM   build_omega_suite.bat v3
REM   build_omega_suite.bat clean
REM
REM =============================================================================

setlocal enabledelayedexpansion
set MASM32=C:\masm32
set ML=%MASM32%\bin\ml.exe
set LINK=%MASM32%\bin\link.exe
set INCLUDE=%MASM32%\include
set LIB=%MASM32%\lib
set OUTDIR=%~dp0..\..\..\..\bin\re_tools

if not exist "%ML%" (
    echo [ERROR] MASM32 not found at %MASM32%
    echo         Install from https://www.masm32.com/
    exit /b 1
)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set TARGET=%~1
if "%TARGET%"=="" set TARGET=all

REM --- Clean ---
if "%TARGET%"=="clean" (
    echo [CLEAN] Removing build artifacts...
    del /q "%OUTDIR%\omega_*.exe" 2>nul
    del /q "*.obj" 2>nul
    echo [DONE]
    exit /b 0
)

REM --- Build v3 ---
if "%TARGET%"=="v3" goto :build_v3
if "%TARGET%"=="all" goto :build_all
goto :build_%TARGET%

:build_all
call :build_v3
call :build_v4
call :build_simple
echo.
echo [DONE] All omega suite tools built in %OUTDIR%
exit /b 0

:build_v3
echo.
echo [BUILD] OMEGA-POLYGLOT v3.0P — Professional Reverse Edition
echo ============================================================
del /q omega_pro_v3.obj 2>nul
"%ML%" /c /coff /I"%INCLUDE%" v3\omega_pro_v3.asm
if errorlevel 1 (
    echo [FAIL] Assembly failed for omega_pro_v3.asm
    exit /b 1
)
"%LINK%" /SUBSYSTEM:CONSOLE /OUT:"%OUTDIR%\omega_pro_v3.exe" omega_pro_v3.obj "%LIB%\kernel32.lib" "%LIB%\user32.lib" "%LIB%\masm32.lib"
if errorlevel 1 (
    echo [FAIL] Link failed for omega_pro_v3.exe
    exit /b 1
)
del /q omega_pro_v3.obj 2>nul
echo [OK] Built: %OUTDIR%\omega_pro_v3.exe
exit /b 0

:build_v4
echo.
echo [BUILD] OMEGA-POLYGLOT v4.0 — Multi-Language Polyglot
echo ======================================================
del /q OmegaPolyglot_v4.obj 2>nul
"%ML%" /c /coff /I"%INCLUDE%" v4\OmegaPolyglot_v4.asm
if errorlevel 1 (
    echo [FAIL] Assembly failed for OmegaPolyglot_v4.asm
    exit /b 1
)
"%LINK%" /SUBSYSTEM:CONSOLE /OUT:"%OUTDIR%\omega_v4.exe" OmegaPolyglot_v4.obj "%LIB%\kernel32.lib" "%LIB%\user32.lib" "%LIB%\masm32.lib"
if errorlevel 1 (
    echo [FAIL] Link failed for omega_v4.exe
    exit /b 1
)
del /q OmegaPolyglot_v4.obj 2>nul
echo [OK] Built: %OUTDIR%\omega_v4.exe
exit /b 0

:build_simple
echo.
echo [BUILD] OMEGA-POLYGLOT SIMPLE v1.0 — Basic PE Analysis
echo =======================================================
del /q omega_simple.obj 2>nul
"%ML%" /c /coff /I"%INCLUDE%" v3\omega_simple.asm
if errorlevel 1 (
    echo [FAIL] Assembly failed for omega_simple.asm
    exit /b 1
)
"%LINK%" /SUBSYSTEM:CONSOLE /OUT:"%OUTDIR%\omega_simple.exe" omega_simple.obj "%LIB%\kernel32.lib" "%LIB%\user32.lib" "%LIB%\masm32.lib"
if errorlevel 1 (
    echo [FAIL] Link failed for omega_simple.exe
    exit /b 1
)
del /q omega_simple.obj 2>nul
echo [OK] Built: %OUTDIR%\omega_simple.exe
exit /b 0
