@echo off
cd /d D:\rawrxd

if "%1"=="clean" goto clean

echo.
echo ========================================
echo   RawrXD MASM x64 -- Master Build
echo   kernel32.lib ONLY / No CRT / x64
echo ========================================
echo.

set PASS=0
set FAIL=0

call :build proof
call :build lsp_jsonrpc
call :build mmap_loader
call :build dequant_simd
call :build kv_cache_mgr

echo ========================================
echo   Results: %PASS% passed, %FAIL% failed
echo ========================================
goto end

:build
echo --- [%1] Assemble ---
C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe /c /nologo /W3 /Fo %1.obj %1.asm
if errorlevel 1 (
    echo [FAIL] %1 - assemble
    set /a FAIL+=1
    goto :eof
)
echo --- [%1] Link ---
C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe /nologo /subsystem:console /entry:main /out:%1.exe %1.obj "/LIBPATH:C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" "/LIBPATH:C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" "/LIBPATH:C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64" kernel32.lib
if errorlevel 1 (
    echo [FAIL] %1 - link
    set /a FAIL+=1
    goto :eof
)
echo [PASS] %1.exe
set /a PASS+=1
if "%2"=="run" (
    %1.exe
    echo.
)
goto :eof

:clean
del /q *.obj *.exe 2>nul
echo Cleaned.
goto end

:end