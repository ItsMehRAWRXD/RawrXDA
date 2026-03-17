@echo off
setlocal

REM ===========================================================================
REM  build_all.bat -- Master build for RawrXD MASM x64 modules
REM  kernel32.lib ONLY. No CRT. No .inc files.
REM
REM  Usage:  build_all.bat         -- build only
REM          build_all.bat run     -- build + run each
REM          build_all.bat clean   -- delete .obj/.exe
REM ===========================================================================

set "ML=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set "LN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
set "LP1=/LIBPATH:C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
set "LP2=/LIBPATH:C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
set "LP3=/LIBPATH:C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64"

cd /d D:\rawrxd

if "%1"=="clean" goto :clean

echo.
echo ========================================
echo   RawrXD MASM x64 -- Master Build
echo   kernel32.lib ONLY / No CRT / x64
echo ========================================
echo.

set PASS=0
set FAIL=0

call :build proof %1
call :build lsp_jsonrpc %1
call :build mmap_loader %1
call :build dequant_simd %1
call :build kv_cache_mgr %1

echo ========================================
echo   Results: %PASS% passed, %FAIL% failed
echo ========================================
if %FAIL% GTR 0 (
    echo BUILD INCOMPLETE
    exit /b 1
)
echo ALL MODULES BUILT SUCCESSFULLY
exit /b 0

REM --- Build subroutine ---
:build
echo --- [%1] Assembling ---
"%ML%" /c /nologo /W3 /Fo %1.obj %1.asm
if errorlevel 1 (
    echo [FAIL] %1.asm - assembly error
    set /a FAIL+=1
    echo.
    goto :eof
)

echo --- [%1] Linking ---
"%LN%" /nologo /subsystem:console /entry:main /out:%1.exe %1.obj "%LP1%" "%LP2%" "%LP3%" kernel32.lib
if errorlevel 1 (
    echo [FAIL] %1 - link error
    set /a FAIL+=1
    echo.
    goto :eof
)

echo [PASS] %1.exe built OK
set /a PASS+=1

if "%2"=="run" (
    echo --- [%1] Running ---
    %1.exe
    echo --- [%1] Done ---
    echo.
)
echo.
goto :eof

:clean
echo Cleaning...
del /q proof.obj proof.exe 2>nul
del /q lsp_jsonrpc.obj lsp_jsonrpc.exe 2>nul
del /q mmap_loader.obj mmap_loader.exe 2>nul
del /q dequant_simd.obj dequant_simd.exe 2>nul
del /q kv_cache_mgr.obj kv_cache_mgr.exe 2>nul
echo Done.
exit /b 0
