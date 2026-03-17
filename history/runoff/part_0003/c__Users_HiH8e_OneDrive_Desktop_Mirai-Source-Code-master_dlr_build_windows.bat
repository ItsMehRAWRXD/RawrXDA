@echo off
REM Windows build script for DLR component
REM Usage: build_windows.bat [Debug|Release] [x86|x64]

setlocal enabledelayedexpansion

set BUILD_TYPE=%1
set TARGET_ARCH=%2

REM Default values
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
if "%TARGET_ARCH%"=="" set TARGET_ARCH=x64

REM Validate build type
if /i not "%BUILD_TYPE%"=="Debug" if /i not "%BUILD_TYPE%"=="Release" (
    echo Error: BUILD_TYPE must be Debug or Release
    exit /b 1
)

REM Validate architecture
if /i not "%TARGET_ARCH%"=="x86" if /i not "%TARGET_ARCH%"=="x64" (
    echo Error: TARGET_ARCH must be x86 or x64
    exit /b 1
)

echo Building DLR for Windows %TARGET_ARCH% - %BUILD_TYPE%

REM Clean previous build
if exist build (
    echo Cleaning previous build...
    rmdir /s /q build
)

REM Create build directory
mkdir build
cd build

REM Configure CMake
if /i "%TARGET_ARCH%"=="x86" (
    cmake .. -A Win32 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
) else (
    cmake .. -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
)

if errorlevel 1 (
    echo CMake configuration failed
    cd ..
    exit /b 1
)

REM Build
cmake --build . --config %BUILD_TYPE%

if errorlevel 1 (
    echo Build failed
    cd ..
    exit /b 1
)

cd ..

REM Check if binary was created
if exist "build\%BUILD_TYPE%\dlr_win.exe" (
    echo Build successful: build\%BUILD_TYPE%\dlr_win.exe
    
    REM Copy to release directory
    if not exist release mkdir release
    copy "build\%BUILD_TYPE%\dlr_win.exe" "release\dlr.%TARGET_ARCH%.exe"
    
    echo Binary copied to release\dlr.%TARGET_ARCH%.exe
) else (
    echo Build failed - binary not found
    exit /b 1
)

endlocal
echo DLR Windows build completed successfully