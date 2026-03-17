REM Win32 IDE Quick Build Script
REM Version: 1.0.0
REM Date: 2025-12-18

@echo off
setlocal enabledelayedexpansion

echo ====================================
echo  RawrXD Win32 IDE - Quick Build
echo ====================================
echo.

REM Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found in PATH
    echo Please install CMake from: https://cmake.org/download/
    pause
    exit /b 1
)

REM Check for Visual Studio
where msbuild >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] MSBuild not found in PATH
    echo Please install Visual Studio 2022 or Build Tools
    pause
    exit /b 1
)

REM Set build directory
set BUILD_DIR=build-win32-only
set CONFIG=Release

echo [1/4] Configuring CMake...
cmake -S win32_only -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed
    pause
    exit /b 1
)
echo [OK] CMake configured successfully
echo.

echo [2/4] Building Release configuration...
cmake --build %BUILD_DIR% --config %CONFIG% --target AgenticIDEWin
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed
    pause
    exit /b 1
)
echo [OK] Build completed successfully
echo.

echo [3/4] Verifying binary...
if not exist "%BUILD_DIR%\bin\%CONFIG%\AgenticIDEWin.exe" (
    echo [ERROR] Binary not found at expected location
    pause
    exit /b 1
)
echo [OK] Binary found: %BUILD_DIR%\bin\%CONFIG%\AgenticIDEWin.exe
echo.

echo [4/4] Checking file size...
for %%A in ("%BUILD_DIR%\bin\%CONFIG%\AgenticIDEWin.exe") do set SIZE=%%~zA
set /a SIZE_MB=!SIZE! / 1048576
echo Binary size: !SIZE_MB! MB
echo.

echo ====================================
echo  BUILD COMPLETE
echo ====================================
echo.
echo Binary location: %BUILD_DIR%\bin\%CONFIG%\AgenticIDEWin.exe
echo.
echo To run the IDE:
echo   cd %BUILD_DIR%\bin\%CONFIG%
echo   AgenticIDEWin.exe
echo.
echo Or simply double-click: %BUILD_DIR%\bin\%CONFIG%\AgenticIDEWin.exe
echo.

pause
exit /b 0
