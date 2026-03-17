@echo off
setlocal enabledelayedexpansion

REM ===========================================================================
REM  build_all.bat — Master build script for RawrXD MASM x64 modules
REM
REM  Assembles, links, and optionally runs all standalone modules.
REM  kernel32.lib ONLY. No CRT. No .inc files.
REM
REM  Usage:
REM    build_all.bat          — build all
REM    build_all.bat run      — build all + run each
REM    build_all.bat clean    — delete .obj/.exe
REM ===========================================================================

set "ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set "LINK=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
set "SDKLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
set "UCRTLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
set "VCLIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64"

cd /d D:\rawrxd

REM --- Modules to build ---
set MODULES=proof lsp_jsonrpc mmap_loader dequant_simd kv_cache_mgr

set TOTAL=0
set PASS=0
set FAIL=0

if "%1"=="clean" goto :clean

echo.
echo ========================================
echo   RawrXD MASM x64 — Master Build
echo   kernel32.lib ONLY / No CRT / x64
echo ========================================
echo.

for %%M in (%MODULES%) do (
    set /a TOTAL+=1
    echo --- [%%M] Assembling ---
    "!ML64!" /c /nologo /W3 /Fo %%M.obj %%M.asm
    if errorlevel 1 (
        echo [FAIL] %%M.asm — assembly error
        set /a FAIL+=1
    ) else (
        echo --- [%%M] Linking ---
        "!LINK!" /nologo /subsystem:console /entry:main /out:%%M.exe %%M.obj "/LIBPATH:!SDKLIB!" "/LIBPATH:!UCRTLIB!" "/LIBPATH:!VCLIB!" kernel32.lib
        if errorlevel 1 (
            echo [FAIL] %%M — link error
            set /a FAIL+=1
        ) else (
            echo [PASS] %%M.exe built OK
            set /a PASS+=1

            if "%1"=="run" (
                echo --- [%%M] Running ---
                %%M.exe
                echo.
                echo --- [%%M] Exit code: !ERRORLEVEL! ---
                echo.
            )
        )
    )
    echo.
)

echo ========================================
echo   Results: !PASS!/!TOTAL! passed, !FAIL! failed
echo ========================================

if !FAIL! GTR 0 (
    echo BUILD INCOMPLETE — fix errors above
    exit /b 1
) else (
    echo ALL MODULES BUILT SUCCESSFULLY
    exit /b 0
)

:clean
echo Cleaning .obj and .exe files...
for %%M in (%MODULES%) do (
    if exist %%M.obj del %%M.obj
    if exist %%M.exe del %%M.exe
)
echo Done.
exit /b 0
