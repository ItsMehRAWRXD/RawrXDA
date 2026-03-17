@echo off
setlocal
REM ═══════════════════════════════════════════════════════════════════════════
REM Build the native PE32+ emitter tool (MASM x64)
REM Produces pe_emitter.exe which generates output.exe when run.
REM ═══════════════════════════════════════════════════════════════════════════

set ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe
set LINK=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe
set UCRT=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
set UM=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64

echo ═══════════════════════════════════════════════════
echo   RawrXD PE Emitter Tool — MASM x64 Build
echo ═══════════════════════════════════════════════════

echo [1/2] Assembling pe_emitter.asm...
"%ML64%" /c /nologo pe_emitter.asm
if errorlevel 1 (
    echo FAIL: ml64 assembly error
    exit /b 1
)

echo [2/2] Linking pe_emitter.obj...
"%LINK%" /nologo /subsystem:console /entry:main pe_emitter.obj kernel32.lib /LIBPATH:"%UCRT%" /LIBPATH:"%UM%"
if errorlevel 1 (
    echo FAIL: linker error
    exit /b 1
)

echo.
echo ═══════════════════════════════════════════════════
echo   SUCCESS: pe_emitter.exe built
echo   Run: pe_emitter.exe  (produces output.exe)
echo   Then: output.exe     (exits cleanly, code 0)
echo ═══════════════════════════════════════════════════
