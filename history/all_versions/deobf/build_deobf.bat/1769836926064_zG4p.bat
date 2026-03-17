@echo off
REM RawrXD Deobfuscator Build Script
REM Complete build with ALL implementations

echo ==========================================
echo  RawrXD Omega Deobfuscator Build System
echo ==========================================
echo.

if "%MASM64%"=="" set MASM64=C:\masm64
set ML64=%MASM64%\bin\ml64.exe
set LINK=link.exe
set OUTDIR=build_deobf

if not exist %OUTDIR% mkdir %OUTDIR%

echo [1/5] Assembling OmegaDeobfuscator...
%ML64% /c /Fo%OUTDIR%\OmegaDeobf.obj /W3 /nologo /Zi /Zd RawrXD_OmegaDeobfuscator.asm
if errorlevel 1 goto error

echo [2/5] Assembling MetaReverse...
%ML64% /c /Fo%OUTDIR%\MetaReverse.obj /W3 /nologo /Zi /Zd RawrXD_MetaReverse.asm
if errorlevel 1 goto error

echo [3/5] Creating static library...
lib /OUT:%OUTDIR%\RawrXD_Deobfuscator.lib %OUTDIR%\OmegaDeobf.obj %OUTDIR%\MetaReverse.obj /NOLOGO
if errorlevel 1 goto error

echo [4/5] Assembling unified API...
%ML64% /c /Fo%OUTDIR%\Unified.obj /W3 /nologo /Zi /Zd RawrXD_Deobfuscator.inc
if errorlevel 1 goto error

echo [5/5] Building test executable...
cl /Fe%OUTDIR%\DeobfTest.exe /O2 /Zi test_deobf.cpp %OUTDIR%\RawrXD_Deobfuscator.lib %OUTDIR%\Unified.obj /link /SUBSYSTEM:CONSOLE
if errorlevel 1 goto error

echo.
echo ==========================================
echo  BUILD SUCCESSFUL
echo ==========================================
echo.
echo Files created:
echo   - %OUTDIR%\RawrXD_Deobfuscator.lib
echo   - %OUTDIR%\DeobfTest.exe
echo.
echo Usage: DeobfTest.exe target.exe
echo.
goto done

:error
echo.
echo ==========================================
echo  BUILD FAILED
echo ==========================================
exit /b 1

:done
