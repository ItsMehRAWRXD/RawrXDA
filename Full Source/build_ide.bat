@echo off
REM Build script for RawrXD IDE using VS2022 Developer Environment

echo Initializing VS2022 Build Tools environment...
REM Auto-detect: try D: first, then C:/VS2022Enterprise, then C:/BuildTools
set "_VCVARS="
if exist "D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "_VCVARS=D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined _VCVARS if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "_VCVARS=C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined _VCVARS if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if defined _VCVARS (
    call "%_VCVARS%"
) else (
    echo [WARN] vcvars64.bat not found on D: or C: — relying on PATH
)

echo Cleaning previous build...
if exist build rd /s /q build
mkdir build
cd build

echo Configuring CMake with NMake...
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..

if errorlevel 1 (
    echo CMake configuration failed!
    cd ..
    exit /b 1
)

echo Building RawrEngine executable...
nmake

if errorlevel 1 (
    echo Build failed!
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo Executable: build\bin\RawrEngine.exe
echo ========================================
cd ..
