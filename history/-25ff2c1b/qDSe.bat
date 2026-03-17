@echo off
REM RawrXD Deobfuscator Build Script
REM Builds both Omega and MetaReverse engines

echo ==========================================
echo  RawrXD Omega Deobfuscator Build System
echo ==========================================
echo.

set ML64=ml64.exe
set LINK=link.exe
set OUTDIR=..\..\build_deobf

if not exist %OUTDIR% mkdir %OUTDIR%

echo [1/4] Assembling OmegaDeobfuscator...
%ML64% /c /Fo%OUTDIR%\OmegaDeobf.obj /W3 /nologo /Zi /Zd RawrXD_OmegaDeobfuscator.asm
if errorlevel 1 goto error

echo [2/4] Assembling MetaReverse...
%ML64% /c /Fo%OUTDIR%\MetaReverse.obj /W3 /nologo /Zi /Zd RawrXD_MetaReverse.asm
if errorlevel 1 goto error

echo [3/4] Creating static library...
lib /OUT:%OUTDIR%\RawrXD_Deobfuscator.lib %OUTDIR%\OmegaDeobf.obj %OUTDIR%\MetaReverse.obj
if errorlevel 1 goto error

echo [4/4] Building test executable...
cl /Fe%OUTDIR%\DeobfTest.exe /O2 /Zi test_deobf.cpp %OUTDIR%\RawrXD_Deobfuscator.lib
if errorlevel 1 goto error

echo.
echo ==========================================
echo  BUILD SUCCESSFUL
echo ==========================================
echo Output: %OUTDIR%\RawrXD_Deobfuscator.lib
echo.
goto done

:error
echo.
echo ==========================================
echo  BUILD FAILED
echo ==========================================
exit /b 1

:done
