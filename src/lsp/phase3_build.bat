@echo off
REM ============================================================================
REM phase3_build.bat — Days 10-12 Build and Validation Script
REM ============================================================================
REM Compiles Phase 3 LSP components and runs comprehensive validation
REM
REM Requirements:
REM   - MSVC 2022 (cl.exe)
REM   - C++20 support
REM   - Windows 10+
REM
REM Usage:
REM   .\phase3_build.bat              (build + test)
REM   .\phase3_build.bat clean        (clean artifacts)
REM   .\phase3_build.bat test-only    (skip build, run tests)
REM
REM ============================================================================

setlocal enabledelayedexpansion
set LSP_SRC=D:\rawrxd\src\lsp
set OUTPUT_DIR=%LSP_SRC%\build
set TEST_EXE=%OUTPUT_DIR%\phase3_tests.exe

if "%1"=="clean" (
    echo Cleaning build artifacts...
    if exist %OUTPUT_DIR% (
        rmdir /s /q %OUTPUT_DIR%
        echo Cleaned.
    )
    exit /b 0
)

REM Create output directory
if not exist %OUTPUT_DIR% mkdir %OUTPUT_DIR%

REM Detailed compiler flags
set CFLAGS=/std:c++latest /EHsc /W4 /O2 /Oi /GL
set CFLAGS=%CFLAGS% /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN
set CFLAGS=%CFLAGS% /I%LSP_SRC% /I"C:\Program Files\nlohmann\include"

if not "%1"=="test-only" (
    echo.
    echo ============================================================================
    echo PHASE 3 BUILD: Days 10-12 LSP Implementation
    echo ============================================================================
    echo.
    
    echo [1/5] Compiling workspace_symbol_index.cpp...
    cl %CFLAGS% /c %LSP_SRC%\workspace_symbol_index.cpp /Fo%OUTPUT_DIR%\workspace_symbol_index.obj
    if errorlevel 1 goto build_error
    
    echo [2/5] Compiling crossfile_rename_engine.cpp...
    cl %CFLAGS% /c %LSP_SRC%\crossfile_rename_engine.cpp /Fo%OUTPUT_DIR%\crossfile_rename_engine.obj
    if errorlevel 1 goto build_error
    
    echo [3/5] Compiling intellisense_completion.cpp...
    cl %CFLAGS% /c %LSP_SRC%\intellisense_completion.cpp /Fo%OUTPUT_DIR%\intellisense_completion.obj
    if errorlevel 1 goto build_error
    
    echo [4/5] Compiling phase3_lsp_test_suite.cpp...
    cl %CFLAGS% /c %LSP_SRC%\phase3_lsp_test_suite.cpp /Fo%OUTPUT_DIR%\phase3_lsp_test_suite.obj
    if errorlevel 1 goto build_error
    
    echo [5/5] Linking test executable...
    link /OUT:%TEST_EXE% %OUTPUT_DIR%\workspace_symbol_index.obj ^
                         %OUTPUT_DIR%\crossfile_rename_engine.obj ^
                         %OUTPUT_DIR%\intellisense_completion.obj ^
                         %OUTPUT_DIR%\phase3_lsp_test_suite.obj
    if errorlevel 1 goto build_error
    
    echo.
    echo Build successful! Test executable: %TEST_EXE%
    echo.
)

REM Run tests
if exist %TEST_EXE% (
    echo.
    echo ============================================================================
    echo RUNNING PHASE 3 VALIDATION TESTS
    echo ============================================================================
    echo.
    
    %TEST_EXE%
    if errorlevel 1 (
        echo.
        echo ============================================================================
        echo TEST FAILURES DETECTED
        echo ============================================================================
        exit /b 1
    )
    
    echo.
    echo ============================================================================
    echo ALL TESTS PASSED - PHASE 3 PRODUCTION READY
    echo ============================================================================
    
) else (
    echo Error: Test executable not found: %TEST_EXE%
    exit /b 1
)

exit /b 0

:build_error
echo.
echo ============================================================================
echo BUILD ERROR - Please check errors above
echo ============================================================================
exit /b 1
