@echo off
REM RawrXD Standalone Web Bridge Build Script (Windows)
REM Qt-free version that bypasses browser TCP limitations

echo =========================================
echo  RawrXD Standalone Web Bridge Builder
echo =========================================

REM Configuration
set BUILD_DIR=build-standalone
set CMAKE_FILE=CMakeLists-Standalone.txt

REM Check prerequisites
echo [INFO] Checking prerequisites...

where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found. Please install CMake 3.16 or later.
    pause
    exit /b 1
)

echo [INFO] Prerequisites check passed.

REM Create build directory
echo [INFO] Setting up build directory...

if exist "%BUILD_DIR%" (
    echo [WARN] Build directory '%BUILD_DIR%' already exists. Cleaning...
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

echo [INFO] Build directory ready.

REM Configure with CMake
echo [INFO] Configuring build with CMake...

if not exist "..\%CMAKE_FILE%" (
    echo [ERROR] CMake file '%CMAKE_FILE%' not found in parent directory.
    pause
    exit /b 1
)

cmake .. -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo [INFO] CMake configuration complete.

REM Build the project
echo [INFO] Building project...

cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [INFO] Build complete.

REM Copy web files
echo [INFO] Copying web interface files...

if not exist "web" mkdir web
copy "..\%CMAKE_FILE%" "web\" >nul 2>nul
copy "..\standalone_interface.html" "web\" >nul

echo [INFO] Web files copied.

REM Success message
echo.
echo [INFO] Build successful! ^🎉
echo.
echo To run the server:
echo   cd %BUILD_DIR%
echo   rawrxd-standalone.exe
echo.
echo Then open http://localhost:8080 in your browser
echo.
echo =========================================

pause