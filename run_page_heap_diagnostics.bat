@echo off
REM ============================================================================
REM  Run IDE Under Page Heap to Catch First Bad Write
REM ============================================================================
REM  This script enables page heap for the IDE binary, then runs a smoke test
REM  that will fault IMMEDIATELY at the first invalid memory access.
REM ============================================================================

setlocal enabledelayedexpansion

set IDE_BINARY=D:\rawrxd\build_ninja3\RawrXD-Win32IDE.exe
set LOG_FILE=D:\rawrxd\page_heap_diag.log

echo [%date% %time%] Starting Page Heap diagnostic run >> "%LOG_FILE%"

REM ============================================================================
REM Step 1: Enable Page Heap for the IDE binary
REM ============================================================================
echo.
echo [STEP 1] Enabling Page Heap for IDE binary...
echo [%date% %time%] Enabling page heap >> "%LOG_FILE%"

REM Check if gflags exists
if not exist "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe" (
    echo ERROR: gflags not found. Install Windows Debugging Tools.
    echo       https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/
    exit /b 1
)

REM Enable full page heap
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe" /p /enable "%IDE_BINARY%" /full
echo Page heap enabled >> "%LOG_FILE%"

REM ============================================================================
REM Step 2: Run IDE smoke test (startup only)
REM ============================================================================
echo.
echo [STEP 2] Running IDE startup under page heap...
echo         (Will display any heap corruption immediately)
echo [%date% %time%] Starting IDE smoke test >> "%LOG_FILE%"

set RAWRXD_PHASE_DEBUG=1
set RAWRXD_HEAP_GUARD=1

REM Run IDE with timeout (30 sec should be enough to crash if corrupted)
timeout /t 2 /nobreak
start /wait /realtime "%IDE_BINARY%" --smoke-test-startup 2>> "%LOG_FILE%"
set EXIT_CODE=!errorlevel!

echo [%date% %time%] IDE exit code: !EXIT_CODE! >> "%LOG_FILE%"

REM ============================================================================
REM Step 3: Check for crash dump and diagnostics
REM ============================================================================
echo.
echo [STEP 3] Checking diagnostic outputs...

if exist "D:\rawrxd\uaf_log.txt" (
    echo.
    echo === UAF Detector Log ===
    type "D:\rawrxd\uaf_log.txt"
    echo. >> "%LOG_FILE%"
    type "D:\rawrxd\uaf_log.txt" >> "%LOG_FILE%"
) else (
    echo No UAF log generated (good sign - no detect corruption)
)

if exist "D:\rawrxd\crash_diag.txt" (
    echo.
    echo === Crash Diagnostics ===
    type "D:\rawrxd\crash_diag.txt"
    echo. >> "%LOG_FILE%"
    type "D:\rawrxd\crash_diag.txt" >> "%LOG_FILE%"
) else (
    echo No crash diagnostics (may indicate clean startup)
)

REM ============================================================================
REM Step 4: Disable page heap (cleanup)
REM ============================================================================
echo.
echo [STEP 4] Disabling page heap...
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe" /p /disable "%IDE_BINARY%"
echo Page heap disabled >> "%LOG_FILE%"

REM ============================================================================
REM Step 5: Summary
REM ============================================================================
echo.
echo [Summary]
echo - Page heap was enabled/disabled
echo - Startup run attempted (exit code: !EXIT_CODE!)
echo - Check logs in D:\rawrxd\uaf_log.txt for corruption details
echo - Full log: "%LOG_FILE%"
echo.

echo [%date% %time%] Diagnostic run complete >> "%LOG_FILE%"

endlocal
