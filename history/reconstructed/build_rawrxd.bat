@echo off
REM Build script for RawrXD with CMake and Visual Studio 2022

setlocal enabledelayedexpansion

echo.
echo ═══════════════════════════════════════════════════════════════
echo  RawrXD CMake Build
echo ═══════════════════════════════════════════════════════════════
echo.

REM Initialize Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 (
    echo [ERROR] Failed to initialize Visual Studio environment
    exit /b 1
)

echo [OK] Visual Studio environment initialized

REM Set CMake flags to work around SDK issues
set "VSCMD_SKIP_SENDTELEMETRY=1"

REM Remove old build directory
if exist build_qt_free (
    echo [INFO] Removing old build directory...
    rmdir /s /q build_qt_free
)

mkdir build_qt_free
cd build_qt_free

echo [INFO] Running CMake configuration...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo [OK] CMake configuration succeeded

echo [INFO] Building project...
cmake --build . --config Release --parallel 4

if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo ═══════════════════════════════════════════════════════════════
echo  BUILD SUCCESSFUL
echo ═══════════════════════════════════════════════════════════════
echo.

endlocal
