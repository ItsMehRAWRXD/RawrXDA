@echo off
setlocal
echo ═══════════════════════════════════════════════
echo   PE Backend Validation — Build ^& Run
echo ═══════════════════════════════════════════════

set VCDIR=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717
set WKDIR=C:\Program Files (x86)\Windows Kits\10
set SDKVER=10.0.22621.0
set PATH=%VCDIR%\bin\Hostx64\x64;%PATH%
set INCLUDE=%VCDIR%\include;%WKDIR%\Include\%SDKVER%\ucrt;%WKDIR%\Include\%SDKVER%\um;%WKDIR%\Include\%SDKVER%\shared
set LIB=%VCDIR%\lib\x64;%WKDIR%\Lib\%SDKVER%\ucrt\x64;%WKDIR%\Lib\%SDKVER%\um\x64

if not exist out mkdir out

echo.
echo [1/2] Compiling pe_validation.exe ...
cl.exe /O2 /W4 /D_CRT_SECURE_NO_WARNINGS /I.. pe_validation.c /Fe:pe_validation.exe /link ..\lib\pe_emitter.lib
if %ERRORLEVEL% neq 0 (
    echo [!] Compilation failed.
    exit /b 1
)

echo.
echo [2/2] Running validation suite ...
echo.
pe_validation.exe
set RESULT=%ERRORLEVEL%

echo.
if %RESULT% equ 0 (
    echo [+] ALL TESTS PASSED
) else (
    echo [!] SOME TESTS FAILED (exit code %RESULT%)
)

echo.
echo Generated artifacts in .\out\:
dir /b out\*.exe out\*.dll 2>nul

exit /b %RESULT%
