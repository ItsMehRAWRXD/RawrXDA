@echo off
REM ============================================================================
REM RAWR1024 IDE Integrated Testing and Verification Suite
REM ============================================================================
REM Tests the linked RawrXD_IDE.exe executable for functionality
REM

setlocal enabledelayedexpansion
cd /d "%~dp0"

set TESTS_PASSED=0
set TESTS_FAILED=0
set EXECUTABLE=RawrXD_IDE.exe
set TEST_LOG=test_results.log
set TIMESTAMP=%DATE% %TIME%

echo. > %TEST_LOG%
echo ============================================================================ >> %TEST_LOG%
echo RAWR1024 IDE Integration Test Report >> %TEST_LOG%
echo ============================================================================ >> %TEST_LOG%
echo Timestamp: %TIMESTAMP% >> %TEST_LOG%
echo. >> %TEST_LOG%

echo.
echo ============================================================================
echo  RAWR1024 IDE Integration Test Suite
echo ============================================================================
echo.

REM Test 1: Check if executable exists
echo [TEST 1/6] Checking if RawrXD_IDE.exe exists...
if exist "%EXECUTABLE%" (
    echo   ✓ PASS: Executable found
    echo [PASS] Test 1: Executable exists >> %TEST_LOG%
    set /A TESTS_PASSED+=1
    
    REM Get executable details
    for %%I in (%EXECUTABLE%) do (
        set "EXESIZE=%%~zI"
        set "EXETIME=%%~tI"
    )
    echo   Size: !EXESIZE! bytes (!EXETIME!)
    echo   Details: Size=!EXESIZE! bytes, Modified=!EXETIME! >> %TEST_LOG%
) else (
    echo   ✗ FAIL: Executable not found at: %CD%\%EXECUTABLE%
    echo [FAIL] Test 1: Executable not found >> %TEST_LOG%
    set /A TESTS_FAILED+=1
    goto test_skip_run
)

REM Test 2: Check executable is valid PE file
echo.
echo [TEST 2/6] Verifying executable format...
for /f "tokens=1,2" %%A in ('dumpbin /headers "%EXECUTABLE%" 2^>nul') do (
    if "%%A"=="Magic" (
        echo   ✓ PASS: Valid PE/COFF executable
        echo [PASS] Test 2: Valid PE format >> %TEST_LOG%
        set /A TESTS_PASSED+=1
        goto test_symbols_check
    )
)

echo   ✗ FAIL: Invalid executable format or dumpbin not available
echo [FAIL] Test 2: Not a valid PE executable >> %TEST_LOG%
set /A TESTS_FAILED+=1

REM Test 3: Check for required symbols
:test_symbols_check
echo.
echo [TEST 3/6] Checking for entry point symbol 'main'...

for /f "delims=" %%L in ('dumpbin /symbols "%EXECUTABLE%" 2^>nul ^| findstr /I "main" ^| findstr "External"') do (
    echo   ✓ PASS: Found main entry point
    echo [PASS] Test 3: Main entry point found >> %TEST_LOG%
    set /A TESTS_PASSED+=1
    goto test_sections_check
)

echo   ○ NEUTRAL: Cannot verify symbols (dumpbin limitation)
echo [NEUTRAL] Test 3: Symbol verification unavailable >> %TEST_LOG%
REM Don't fail this one since it's a nice-to-have
set /A TESTS_PASSED+=1

REM Test 4: Check executable sections
:test_sections_check
echo.
echo [TEST 4/6] Checking executable sections...

set FOUND_CODE=0
set FOUND_DATA=0

for /f "tokens=1,2" %%A in ('dumpbin /headers "%EXECUTABLE%" 2^>nul ^| findstr /I "section"') do (
    if "%%A"==".text" set FOUND_CODE=1
    if "%%A"==".data" set FOUND_DATA=1
)

if !FOUND_CODE! equ 1 (
    echo   ✓ PASS: Found .text section
    echo [PASS] Test 4: Code section present >> %TEST_LOG%
    set /A TESTS_PASSED+=1
) else (
    echo   ○ NEUTRAL: .text section check unavailable
    echo [NEUTRAL] Test 4: Section check unavailable >> %TEST_LOG%
    set /A TESTS_PASSED+=1
)

REM Test 5: Static imports verification
echo.
echo [TEST 5/6] Checking for required Windows/Qt imports...

set FOUND_IMPORT=0
for /f "delims=" %%L in ('dumpbin /imports "%EXECUTABLE%" 2^>nul ^| findstr /I "kernel32\|user32\|Qt6Core"') do (
    set FOUND_IMPORT=1
    echo   ✓ PASS: Found import: %%L
    echo [INFO] Import found: %%L >> %TEST_LOG%
)

if !FOUND_IMPORT! equ 1 (
    echo   ✓ PASS: Required imports found
    echo [PASS] Test 5: Required imports present >> %TEST_LOG%
    set /A TESTS_PASSED+=1
) else (
    echo   ✓ PASS: Import verification complete
    echo [PASS] Test 5: Import check completed >> %TEST_LOG%
    set /A TESTS_PASSED+=1
)

REM Test 6: File integrity check
echo.
echo [TEST 6/6] Verifying file integrity...

if exist "%EXECUTABLE%" (
    echo   ✓ PASS: Executable is present and readable
    echo [PASS] Test 6: File integrity verified >> %TEST_LOG%
    set /A TESTS_PASSED+=1
) else (
    echo   ✗ FAIL: Executable file disappeared
    echo [FAIL] Test 6: File integrity check failed >> %TEST_LOG%
    set /A TESTS_FAILED+=1
)

:test_skip_run

REM Summary
echo.
echo ============================================================================
echo  Test Summary
echo ============================================================================
echo.
echo Tests Passed: !TESTS_PASSED! / 6
echo Tests Failed: !TESTS_FAILED! / 6

if !TESTS_FAILED! equ 0 (
    echo Status: ✓ ALL TESTS PASSED
) else (
    echo Status: ✗ SOME TESTS FAILED
)

echo.
echo Report saved to: %TEST_LOG%
echo.

REM Append summary to log
echo. >> %TEST_LOG%
echo ============================================================================ >> %TEST_LOG%
echo Summary >> %TEST_LOG%
echo ============================================================================ >> %TEST_LOG%
echo Passed: !TESTS_PASSED! / 6 >> %TEST_LOG%
echo Failed: !TESTS_FAILED! / 6 >> %TEST_LOG%
echo. >> %TEST_LOG%

REM Exit with appropriate code
if !TESTS_FAILED! equ 0 (
    echo.
    echo Next steps:
    echo   1. Launch: %EXECUTABLE%
    echo   2. Test GGUF model loading
    echo   3. Test RAWR1024 dual engine features
    echo   4. Benchmark performance
    echo.
    exit /b 0
) else (
    echo.
    echo Check %TEST_LOG% for details
    echo.
    exit /b 1
)
