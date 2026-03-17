@echo off
REM ═══════════════════════════════════════════════════════════════════
REM Build script for PE Writer & Machine Code Emitter (x64 MASM)
REM ═══════════════════════════════════════════════════════════════════

SET ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe
SET SDK=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64
SET CRT=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64

echo.
echo ════════════════════════════════════════════════════════════════════
echo   Building PE Writer - Zero-Dependency PE32+ Generator
echo ════════════════════════════════════════════════════════════════════
echo.

"%ML64%" /nologo /c /Fo pe_writer_full.obj pe_writer_full.asm
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Assembly failed
    exit /b 1
)

echo [OK] Assembly successful
echo.
echo Linking...

link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main ^
     /LIBPATH:"%SDK%" /LIBPATH:"%CRT%" ^
     pe_writer_full.obj ^
     kernel32.lib ^
     /OUT:pe_generator.exe ^
     /NODEFAULTLIB:libcmt

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [OK] Link successful
echo.
echo ════════════════════════════════════════════════════════════════════
echo   Build complete: pe_generator.exe
echo   Run it to generate a minimal PE executable: generated.exe
echo ════════════════════════════════════════════════════════════════════
echo.
