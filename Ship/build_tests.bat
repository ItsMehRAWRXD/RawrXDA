@echo off
setlocal enabledelayedexpansion

REM ═════════════════════════════════════════════════════════════════════════════
REM build_tests.bat - Automated Test Suite Build Script
REM ═════════════════════════════════════════════════════════════════════════════

if not exist build mkdir build
cd /d build

REM Set compiler flags
set CFLAGS=/std:c++20 /EHsc /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX

REM Set include/lib paths
set INCLUDES=/I"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35723\include"
set INCLUDES=!INCLUDES! /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um"
set INCLUDES=!INCLUDES! /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared"

set LIBPATHS=/LIBPATH:"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35723\lib\x64"
set LIBPATHS=!LIBPATHS! /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
set LIBPATHS=!LIBPATHS! /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║     RawrXD Integration Test Suite - Build Script               ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

echo [1/3] Cleaning old artifacts...
if exist RawrXD_TestRunner.exe del RawrXD_TestRunner.exe
if exist RawrXD_TestRunner.obj del RawrXD_TestRunner.obj
if exist test_results.txt del test_results.txt
if exist test_results.xml del test_results.xml

echo [2/3] Compiling test runner...
cl %CFLAGS% %INCLUDES% /Fe:RawrXD_TestRunner.exe ..\RawrXD_TestRunner.cpp ^
    /link %LIBPATHS% kernel32.lib user32.lib psapi.lib dbghelp.lib shlwapi.lib /SUBSYSTEM:CONSOLE
    
if errorlevel 1 (
    echo.
    echo ✗ Compilation failed
    cd ..
    exit /b 1
)

echo [3/3] Test executable ready
echo.
echo ✓ Build successful
echo   Output: build\RawrXD_TestRunner.exe
echo.
echo Test Execution:
echo   Run all tests:        RawrXD_TestRunner.exe
echo   List all tests:       RawrXD_TestRunner.exe --list
echo   Run category:         RawrXD_TestRunner.exe --category CoreInfra
echo   Generate XML report:  RawrXD_TestRunner.exe --output report.xml --xml
echo   Stop on failure:      RawrXD_TestRunner.exe --stop-on-failure
echo   Repeat 5 times:       RawrXD_TestRunner.exe --repeat 5
echo.

REM Optional: Run tests immediately
REM echo Running tests...
REM RawrXD_TestRunner.exe --output ..\test_results.txt
REM RawrXD_TestRunner.exe --output ..\test_results.xml --xml

cd ..
endlocal
exit /b 0
