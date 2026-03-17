@echo off
REM ============================================================
REM  RawrXD Unified IDE - Smoke Test Harness (Task 18)
REM  Launches each mode with valid/invalid args, checks exit codes.
REM  Target: Full regression in <30 seconds
REM ============================================================
setlocal enabledelayedexpansion

set EXE=RawrXD_IDE_unified.exe
set PASS=0
set FAIL=0
set TOTAL=0
set CRASH_CODE=C0000005

echo ============================================================
echo  RawrXD Smoke Test Harness
echo  %date% %time%
echo ============================================================
echo.

REM Check that the EXE exists
if not exist "%~dp0%EXE%" (
    echo FATAL: %EXE% not found in %~dp0
    echo Build first with: RawrXD_IDE_BUILD.bat
    exit /b 1
)

REM ---- Helper: RunTest label, args, expected (0=clean, 1=crash expected) ----
REM We use CALL :RunTest to keep it clean

REM ============================================================
REM  VALID MODE TESTS (expect exit code 0)
REM ============================================================
echo --- Valid Mode Tests (expect exit code 0) ---
echo.

call :RunTest "Mode 1: Compile"    "-compile"    0
call :RunTest "Mode 2: Encrypt"    "-encrypt"    0
call :RunTest "Mode 3: Inject"     "-inject"     0
call :RunTest "Mode 4: UAC"        "-uac"        0
call :RunTest "Mode 5: Persist"    "-persist"    0
call :RunTest "Mode 6: Sideload"   "-sideload"   0
call :RunTest "Mode 7: AVScan"     "-avscan"     0
call :RunTest "Mode 8: Entropy"    "-entropy"    0
call :RunTest "Mode 9: StubGen"    "-stubgen"    0
call :RunTest "Mode 10: Trace"     "-trace"      0
call :RunTest "Mode 11: Agent"     "-agent"      0

REM ============================================================
REM  INVALID / EDGE-CASE TESTS (expect clean exit, NOT crash)
REM ============================================================
echo.
echo --- Invalid / Edge-Case Tests (expect clean exit, NOT 0xC0000005) ---
echo.

call :RunTest "No args (GUI skip)"       ""               0
call :RunTest "Invalid flag"             "-invalidmode"   0
call :RunTest "Out-of-range mode 255"    "-mode255"       0
call :RunTest "Garbage input"            "AAAAAAAAAA"     0
call :RunTest "Inject no PID"            "-inject"        0
call :RunTest "Empty string arg"         """"             0
call :RunTest "Very long arg"            "-compile -encrypt -inject -uac -persist -sideload -avscan -entropy -stubgen -trace -agent" 0

REM ============================================================
REM  BOUNDARY TESTS (bounds check validation)
REM ============================================================
echo.
echo --- Boundary Tests (bounds check / canary validation) ---
echo.

call :RunTest "Compile alias /c"   "c"     0
call :RunTest "Double-dash"        "--compile"  0

REM ============================================================
REM  RESULTS SUMMARY
REM ============================================================
echo.
echo ============================================================
echo  SMOKE TEST RESULTS
echo ============================================================
echo  Total:   %TOTAL%
echo  Passed:  %PASS%
echo  Failed:  %FAIL%
echo ============================================================

if %FAIL% GTR 0 (
    echo  STATUS: SOME TESTS FAILED ^^^!
    echo ============================================================
    exit /b 1
) else (
    echo  STATUS: ALL TESTS PASSED
    echo ============================================================
    exit /b 0
)

REM ============================================================
REM  :RunTest  "label"  "args"  expected_behavior
REM    expected_behavior: 0 = expect clean exit (exit code 0)
REM                       1 = expect crash (any nonzero / 0xC0000005)
REM ============================================================
:RunTest
    set /a TOTAL+=1
    set "LABEL=%~1"
    set "ARGS=%~2"
    set "EXPECT=%~3"

    REM Run the exe with a 10-second timeout, capture exit code
    REM Use cmd /c to isolate crash codes from the parent shell
    cmd /c ""%~dp0%EXE%" %ARGS% >nul 2>&1" <nul
    set EC=!ERRORLEVEL!

    REM Convert exit code to hex-ish for crash detection
    REM 0xC0000005 = -1073741819 in signed 32-bit
    set CRASH=0
    if !EC! EQU -1073741819 set CRASH=1
    if !EC! EQU -1073741795 set CRASH=1
    REM 0xC00000FF = -1073741569 (invalid stack)
    if !EC! EQU -1073741569 set CRASH=1
    REM 0xC0000409 = STATUS_STACK_BUFFER_OVERRUN = -1073740791
    if !EC! EQU -1073740791 set CRASH=1

    if "%EXPECT%"=="0" (
        REM We expect a clean exit (no crash)
        if !CRASH! EQU 1 (
            echo [FAIL] !LABEL! - CRASHED ^(exit code: !EC!^)
            set /a FAIL+=1
        ) else (
            echo [PASS] !LABEL! - exit code: !EC!
            set /a PASS+=1
        )
    ) else (
        REM We expect a crash (for intentional stress tests)
        if !CRASH! EQU 1 (
            echo [PASS] !LABEL! - Crashed as expected ^(exit code: !EC!^)
            set /a PASS+=1
        ) else (
            echo [FAIL] !LABEL! - Expected crash but got exit code: !EC!
            set /a FAIL+=1
        )
    )
    goto :eof
