@echo off
REM build_rbtree.bat — Assemble RawrXD_RBTree.asm for x64
REM Usage: build_rbtree.bat [Debug|Release]

setlocal EnableDelayedExpansion

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

echo [RawrXD] Building MASM x64 Red-Black Tree (%CONFIG%)...

set ML64="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set SRC=src\win32app\RawrXD_RBTree.asm
set OUTDIR=build-ninja\%CONFIG%

if not exist %OUTDIR% mkdir %OUTDIR%

set OBJ=%OUTDIR%\RawrXD_RBTree.obj

%ML64% /c /Zi /Fo%OBJ% %SRC%
if errorlevel 1 (
    echo [RawrXD] FAILED: MASM assembly error
    exit /b 1
)

echo [RawrXD] SUCCESS: %OBJ%
endlocal
