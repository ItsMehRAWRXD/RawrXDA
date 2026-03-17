@echo off
REM Pure MASM IDE Build - Full 11-file compilation and linking

setlocal enabledelayedexpansion

cd /d "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\src\masm\final-ide"

set ML64="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set LINK="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
set LIB="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

if not exist obj mkdir obj
if not exist bin mkdir bin

echo ===================================================================
echo Pure MASM IDE Build - Complete 11-file compilation
echo ===================================================================
echo.

set FAILED=0

REM Compile all 11 files
for %%F in (asm_memory.asm malloc_wrapper.asm asm_string.asm asm_log.asm asm_events.asm qt6_foundation.asm qt6_main_window.asm qt6_statusbar.asm qt6_text_editor.asm qt6_syntax_highlighter.asm main_masm.asm) do (
    set FILE=%%F
    set OBJFILE=!FILE:.asm=.obj!
    echo Compiling !FILE!...
    %ML64% /c /Cp /nologo /Zi /Fo "obj\!OBJFILE!" "!FILE!" 2>&1 | findstr error && set FAILED=1
)

if %FAILED% equ 1 (
    echo.
    echo ===================================================================
    echo Build FAILED - compilation errors
    echo ===================================================================
    exit /b 1
)

echo.
echo All files compiled successfully.
echo.
echo Linking RawrXD-Pure-MASM-IDE.exe...
echo.

REM List all object files
dir obj\*.obj /B > obj_list.txt

REM Link with proper settings
%LINK% /NOLOGO /SUBSYSTEM:WINDOWS /MACHINE:X64 /OUT:bin\RawrXD-Pure-MASM-IDE.exe ^
    /LIBPATH:%LIB% ^
    kernel32.lib user32.lib gdi32.lib ^
    obj\asm_memory.obj obj\malloc_wrapper.obj obj\asm_string.obj obj\asm_log.obj obj\asm_events.obj ^
    obj\qt6_foundation.obj obj\qt6_main_window.obj obj\qt6_statusbar.obj obj\qt6_text_editor.obj ^
    obj\qt6_syntax_highlighter.obj obj\main_masm.obj 2>&1

if errorlevel 1 (
    echo.
    echo ===================================================================
    echo Linking FAILED - see errors above
    echo ===================================================================
    exit /b 1
)

if exist bin\RawrXD-Pure-MASM-IDE.exe (
    for %%F in (bin\RawrXD-Pure-MASM-IDE.exe) do set SIZE=%%~zF
    echo.
    echo ===================================================================
    echo Build SUCCESS!
    echo Executable: bin\RawrXD-Pure-MASM-IDE.exe (!SIZE! bytes^)
    echo ===================================================================
    echo.
    dir bin\RawrXD-Pure-MASM-IDE.exe
) else (
    echo.
    echo ===================================================================
    echo ERROR: Executable not created despite successful link
    echo ===================================================================
    exit /b 1
)
