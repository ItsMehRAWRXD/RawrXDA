@echo off
REM Pure MASM IDE Build - Simplified compilation of 11 core files

setlocal enabledelayedexpansion

cd /d "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\src\masm\final-ide"

set ML64="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set LINK="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

if not exist obj mkdir obj
if not exist bin mkdir bin

echo ===================================================================
echo Pure MASM IDE Build - Compiling 11 files
echo ===================================================================
echo.

set FAILED=0
set COMPILED=0

echo [1/11] asm_memory.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\asm_memory.obj asm_memory.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [2/11] malloc_wrapper.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\malloc_wrapper.obj malloc_wrapper.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [3/11] asm_string.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\asm_string.obj asm_string.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [4/11] asm_log.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\asm_log.obj asm_log.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [5/11] asm_events.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\asm_events.obj asm_events.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [6/11] qt6_foundation.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\qt6_foundation.obj qt6_foundation.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [7/11] qt6_main_window.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\qt6_main_window.obj qt6_main_window.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [8/11] qt6_statusbar.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\qt6_statusbar.obj qt6_statusbar.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [9/11] qt6_text_editor.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\qt6_text_editor.obj qt6_text_editor.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [10/11] qt6_syntax_highlighter.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\qt6_syntax_highlighter.obj qt6_syntax_highlighter.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo [11/11] main_masm.asm...
%ML64% /c /Cp /nologo /Zi /Fo obj\main_masm.obj main_masm.asm >nul 2>&1
if errorlevel 1 (echo FAIL & set FAILED=1) else (echo PASS & set /a COMPILED=COMPILED+1)

echo.
echo ===================================================================
echo Compilation Summary: %COMPILED%/11 files compiled
echo ===================================================================
echo.

if %FAILED% equ 0 (
    echo Linking RawrXD-Pure-MASM-IDE.exe...
    %LINK% /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:_start /OUT:bin\RawrXD-Pure-MASM-IDE.exe obj\*.obj >nul 2>&1
    if errorlevel 0 (
        if exist bin\RawrXD-Pure-MASM-IDE.exe (
            for %%F in (bin\RawrXD-Pure-MASM-IDE.exe) do set SIZE=%%~zF
            echo Build SUCCESS: bin\RawrXD-Pure-MASM-IDE.exe created (!SIZE! bytes^)
        ) else (
            echo Link completed but executable not found
        )
    ) else (
        echo Link FAILED
    )
) else (
    echo BUILD FAILED - see errors above
)
