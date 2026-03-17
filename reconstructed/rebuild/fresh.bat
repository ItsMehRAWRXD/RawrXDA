@echo off
REM Force fresh build of RawrXD-AgenticIDE
setlocal enabledelayedexpansion

cd /d "D:\RawrXD-production-lazy-init"

echo ==========================================
echo DESTROYING OLD BUILD ARTIFACTS
echo ==========================================
if exist build (
    rmdir /s /q build
    echo ✓ Build dir removed
)

if exist build-msvc (
    rmdir /s /q build-msvc
    echo ✓ Build-msvc removed
)

echo.
echo ==========================================
echo CREATING FRESH BUILD DIRECTORY
echo ==========================================
mkdir build
cd /d "D:\RawrXD-production-lazy-init\build"
echo ✓ Build directory ready at: %cd%

echo.
echo ==========================================
echo CONFIGURING WITH CMAKE
echo ==========================================

cmake -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DCMAKE_PREFIX_PATH="C:\Qt\6.7.3\msvc2022_64" ^
  .. 

if errorlevel 1 (
    echo ✗ CMAKE CONFIGURATION FAILED
    exit /b 1
)

echo ✓ CMake configuration complete

echo.
echo ==========================================
echo BUILDING RELEASE
echo ==========================================

cmake --build . --config Release -j4

if errorlevel 1 (
    echo ✗ BUILD FAILED
    exit /b 1
)

echo ✓ Build completed successfully

echo.
echo ==========================================
echo BUILD COMPLETE
echo ==========================================
echo Executable location: D:\RawrXD-production-lazy-init\build\bin\Release\RawrXD-AgenticIDE.exe

endlocal
