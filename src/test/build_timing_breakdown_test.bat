@echo off
REM ============================================================================
REM build_timing_breakdown_test.bat — Three-Clock Instrumentation Test Build
REM ============================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0"

echo ============================================================================
echo Building Three-Clock Timing Breakdown Test
echo ============================================================================

REM Compiler configuration
REM Include parent src directory so #include "telemetry/..." resolves correctly.
set CL_FLAGS=/std:c++17 /O2 /EHsc /arch:AVX2 /W4 /I. /I..
set SDK_LIB_UM=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64
set SDK_LIB_UCRT=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64

REM Compile test
echo.
echo [1/2] Compiling test_timing_breakdown_validation.cpp...
cl %CL_FLAGS% /Fe:test_timing_breakdown_validation.exe test_timing_breakdown_validation.cpp /link /LIBPATH:"%SDK_LIB_UM%" /LIBPATH:"%SDK_LIB_UCRT%"
if errorlevel 1 (
    echo ERROR: Compilation failed
    exit /b 1
)

echo.
echo [2/2] Running test...
echo.
test_timing_breakdown_validation.exe

if errorlevel 1 (
    echo ERROR: Test execution failed
    exit /b 1
)

echo.
echo ============================================================================
echo Build and test completed successfully!
echo ============================================================================

endlocal
