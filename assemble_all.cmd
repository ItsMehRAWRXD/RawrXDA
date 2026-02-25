@echo off
setlocal enabledelayedexpansion

set "ML64=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set "OUTDIR=d:\rawrxd\build_genesis"
set "SRC1=d:\rawrxd\src\asm"
set "SRC2=d:\rawrxd\src"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set OK=0
set FAIL=0
set TOTAL=0

echo ========================================
echo  Assembling MASM sources with ml64.exe
echo ========================================

echo.
echo [Pass 1] src\asm\*.asm
for %%f in ("%SRC1%\*.asm") do (
    set /a TOTAL+=1
    "%ML64%" /c /nologo /W0 /Fo"%OUTDIR%\%%~nf.obj" "%%f" >nul 2>nul
    if !errorlevel! equ 0 (
        set /a OK+=1
    ) else (
        set /a FAIL+=1
        echo   FAIL: %%~nxf
    )
)

echo.
echo [Pass 2] src\*.asm
for %%f in ("%SRC2%\*.asm") do (
    set /a TOTAL+=1
    "%ML64%" /c /nologo /W0 /Fo"%OUTDIR%\%%~nf.obj" "%%f" >nul 2>nul
    if !errorlevel! equ 0 (
        set /a OK+=1
    ) else (
        set /a FAIL+=1
        echo   FAIL: %%~nxf
    )
)

echo.
echo ========================================
echo  Total: !TOTAL!  OK: !OK!  Failed: !FAIL!
echo ========================================

dir /b "%OUTDIR%\*.obj" 2>nul | find /c /v ""
echo objects in %OUTDIR%
