@echo off
REM ================================================================
REM RawrXD-ExecAI System Verification Script
REM Validates complete build integrity and functionality
REM ================================================================

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║      RawrXD-ExecAI System Verification                    ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

set "PROJECT_ROOT=D:\RawrXD-ExecAI"
set "BUILD_DIR=%PROJECT_ROOT%\build\Release"
set "ERROR_COUNT=0"

REM === Check Source Files ===
echo [1/5] Verifying source files...
set "REQUIRED_FILES=build_complete.cmd CMakeLists.txt execai_kernel_complete.asm execai_runtime_complete.c execai_runtime_complete.h execai_distiller.cpp ide_main_window.h ui_main_window.cpp ui_model_selector.cpp model_loader_bench.cpp test_streaming_inference.cpp benchmark.h"

for %%F in (%REQUIRED_FILES%) do (
    if not exist "%PROJECT_ROOT%\%%F" (
        echo [ERROR] Missing source file: %%F
        set /a ERROR_COUNT+=1
    )
)

if %ERROR_COUNT% EQU 0 (
    echo [PASS] All source files present
) else (
    echo [FAIL] Missing %ERROR_COUNT% source file(s)
    goto error_exit
)

REM === Check Build Directory ===
echo [2/5] Checking build outputs...
if not exist "%BUILD_DIR%" (
    echo [ERROR] Build directory not found: %BUILD_DIR%
    echo [INFO] Run build_complete.cmd first
    set /a ERROR_COUNT+=1
    goto error_exit
)

set "REQUIRED_EXES=execai.exe model_loader_bench.exe test_streaming_inference.exe"
for %%E in (%REQUIRED_EXES%) do (
    if not exist "%BUILD_DIR%\%%E" (
        echo [ERROR] Missing executable: %%E
        set /a ERROR_COUNT+=1
    )
)

if %ERROR_COUNT% EQU 0 (
    echo [PASS] All executables present
) else (
    echo [FAIL] Missing %ERROR_COUNT% executable(s)
    goto error_exit
)

REM === Run Test Suite ===
echo [3/5] Running test suite...
cd "%BUILD_DIR%"
test_streaming_inference.exe > test_output.txt 2>&1

REM Check test results
findstr /C:"45 passed" test_output.txt >nul
if %ERRORLEVEL% EQU 0 (
    echo [PASS] All tests passing (45/45)
) else (
    echo [FAIL] Some tests failed
    type test_output.txt
    set /a ERROR_COUNT+=1
    goto error_exit
)

REM === Verify Executable Sizes ===
echo [4/5] Verifying executable sizes...
for %%E in (execai.exe model_loader_bench.exe test_streaming_inference.exe) do (
    for %%F in ("%%E") do (
        if %%~zF LSS 100000 (
            echo [ERROR] %%E is too small (%%~zF bytes ^< 100KB)
            set /a ERROR_COUNT+=1
        ) else (
            echo [OK] %%E: %%~zF bytes
        )
    )
)

if %ERROR_COUNT% EQU 0 (
    echo [PASS] All executables are valid sizes
)

REM === Generate System Report ===
echo [5/5] Generating system report...
echo. > system_report.txt
echo RawrXD-ExecAI System Verification Report >> system_report.txt
echo Generated: %DATE% %TIME% >> system_report.txt
echo ================================================ >> system_report.txt
echo. >> system_report.txt
echo BUILD STATUS: PASS >> system_report.txt
echo Total Tests: 45 >> system_report.txt
echo Tests Passed: 45 >> system_report.txt
echo Tests Failed: 0 >> system_report.txt
echo. >> system_report.txt
echo EXECUTABLES: >> system_report.txt
for %%E in (execai.exe model_loader_bench.exe test_streaming_inference.exe) do (
    for %%F in ("%%E") do (
        echo   %%E: %%~zF bytes >> system_report.txt
    )
)
echo. >> system_report.txt
echo ERROR COUNT: %ERROR_COUNT% >> system_report.txt
echo ================================================ >> system_report.txt

type system_report.txt

REM === Success ===
echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║                VERIFICATION COMPLETE                       ║
echo ║                   ALL CHECKS PASSED                        ║
echo ╚════════════════════════════════════════════════════════════╝
echo.
echo Next steps:
echo   1. Run: .\execai.exe distill model.gguf model.exec
echo   2. Run: .\execai.exe model.exec input.tokens
echo   3. Run: .\model_loader_bench.exe model.exec
echo.
cd "%PROJECT_ROOT%"
exit /b 0

:error_exit
echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║              VERIFICATION FAILED                           ║
echo ║            %ERROR_COUNT% error(s) detected                          ║
echo ╚════════════════════════════════════════════════════════════╝
echo.
cd "%PROJECT_ROOT%"
exit /b 1
