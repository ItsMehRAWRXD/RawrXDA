@echo off
REM ============================================================================
REM FINAL_BUILD_AND_TEST.bat - Complete build and verification script
REM ============================================================================

setlocal enabledelayedexpansion
cls

echo.
echo ============================================================================
echo RawrXD Pure MASM64 IDE - Final Build and Test
echo ============================================================================
echo.

REM Clean previous artifacts
echo [STEP 1] Cleaning previous build artifacts...
del /q *.obj >nul 2>&1
if exist build\bin\Release\RawrXD.exe del /q build\bin\Release\RawrXD.exe

echo [STEP 2] Running BUILD.bat...
call BUILD.bat

if errorlevel 1 (
    echo.
    echo ============================================================================
    echo BUILD FAILED - See errors above
    echo ============================================================================
    exit /b 1
)

echo.
echo ============================================================================
echo Build successful! Verifying executable...
echo ============================================================================
echo.

if not exist build\bin\Release\RawrXD.exe (
    echo ERROR: RawrXD.exe not found in build\bin\Release\
    exit /b 1
)

for %%F in (build\bin\Release\RawrXD.exe) do (
    set "SIZE=%%~zF"
    echo Executable: build\bin\Release\RawrXD.exe
    echo Size: !SIZE! bytes
    if !SIZE! LSS 1500000 (
        echo WARNING: Executable smaller than expected (1.5 MB)
    ) else (
        echo ✓ Executable size is acceptable (^>= 1.5 MB)
    )
)

echo.
echo ============================================================================
echo To run the IDE:
echo ============================================================================
echo   build\bin\Release\RawrXD.exe
echo.
echo To test commands:
echo   - File ^> Open: Load a GGUF model
echo   - /terminal_run dir
echo   - /git_push_all
echo   - /scaffold_project
echo   - /highlight_code asm
echo.
echo ============================================================================
echo BUILD COMPLETE ✓
echo ============================================================================
echo.

exit /b 0
