@echo off
REM ============================================================================
REM Build Script for RawrXD-IDE v1.0
REM Qt 6.7.3 + MSVC 2022 + MASM Integration
REM ============================================================================

setlocal enabledelayedexpansion

set QT_PATH=C:\Qt\6.7.3\msvc2022_64
set QT_MINGW_PATH=C:\Qt\6.7.3\llvm-mingw_64

REM Try MSVC path first, then MinGW fallback
if exist "%QT_PATH%\bin\qmake.exe" (
    set QMAKE_PATH=%QT_PATH%\bin\qmake.exe
    echo Using Qt MSVC build
) else if exist "%QT_MINGW_PATH%\bin\qmake.exe" (
    set QMAKE_PATH=%QT_MINGW_PATH%\bin\qmake.exe
    echo Using Qt MinGW build (MSVC not found)
) else (
    echo ERROR: Qt not found at %QT_PATH% or %QT_MINGW_PATH%
    exit /b 1
)

echo.
echo ============================================================================
echo RawrXD-IDE v1.0 Build Script
echo ============================================================================
echo Qt Path: %QMAKE_PATH%
echo Build Directory: %cd%
echo.

REM Clean previous build
if exist build (
    echo [1/4] Cleaning previous build...
    rmdir /s /q build >nul 2>&1
)

REM Create build directory
mkdir build >nul 2>&1

REM Run qmake
echo [2/4] Running qmake...
pushd build
"%QMAKE_PATH%" ..\RawrXD-IDE.pro CONFIG+=release
if errorlevel 1 (
    echo ERROR: qmake failed
    popd
    exit /b 1
)
popd

REM Run nmake
echo [3/4] Running nmake (this may take 1-2 minutes)...
pushd build
nmake VERBOSE=1 >build.log 2>&1
if errorlevel 1 (
    echo ERROR: nmake failed, check build.log
    type build.log | findstr /i "error"
    popd
    exit /b 1
)
popd

REM Check for output executable
echo [4/4] Verifying build output...
if exist build\release\RawrXD-IDE.exe (
    for /F "usebackq" %%A in ('dir /b build\release\RawrXD-IDE.exe ^| find "RawrXD-IDE"') do set EXE_SIZE=%%~zA
    echo.
    echo ============================================================================
    echo Build SUCCESSFUL!
    echo ============================================================================
    echo Executable: build\release\RawrXD-IDE.exe (!EXE_SIZE! bytes)
    echo Status: Ready for smoke testing
    echo.
    exit /b 0
) else (
    echo ERROR: RawrXD-IDE.exe not found after build
    type build\build.log | findstr /i "error"
    exit /b 1
)
