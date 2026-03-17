@echo off
REM =====================================================================
REM build_masm_hotpatch.bat - Pure MASM x64 Hotpatch Build Script
REM NO C/C++ DEPENDENCIES - Windows MSVC 2022 Required
REM =====================================================================

setlocal enabledelayedexpansion

echo ========================================
echo Pure MASM x64 Hotpatch Build System
echo ========================================
echo.

REM Check for build type argument
set BUILD_TYPE=Release
if not "%1"=="" set BUILD_TYPE=%1

echo Build type: %BUILD_TYPE%
echo.

REM Find Visual Studio 2022
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo ERROR: Visual Studio 2022 not found!
    echo Please install MSVC 2022 with MASM support
    exit /b 1
)

echo.
echo ========================================
echo Step 1: Create build directory
echo ========================================

if not exist "build_masm" mkdir build_masm
cd build_masm

echo.
echo ========================================
echo Step 2: Configure CMake
echo ========================================

cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DBUILD_MASM_TESTS=ON ^
    -DINTEGRATE_WITH_QTSHELL=OFF ^
    ..

if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Step 3: Build MASM components
echo ========================================

cmake --build . --config %BUILD_TYPE% --target masm_hotpatch_unified

if errorlevel 1 (
    echo ERROR: MASM build failed!
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Step 4: Build pure MASM test harness
echo ========================================

cmake --build . --config %BUILD_TYPE% --target masm_hotpatch_test

if errorlevel 1 (
    echo ERROR: Test build failed!
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Step 5: Run tests
echo ========================================

if exist "bin\tests\%BUILD_TYPE%\masm_hotpatch_test.exe" (
    echo Running pure MASM test suite...
    bin\tests\%BUILD_TYPE%\masm_hotpatch_test.exe
    
    if errorlevel 1 (
        echo.
        echo WARNING: Some tests failed! Exit code: !errorlevel!
    ) else (
        echo.
        echo SUCCESS: All tests passed!
    )
) else (
    echo WARNING: Test executable not found
)

cd ..

echo.
echo ========================================
echo Build Summary
echo ========================================
echo.

if exist "build_masm\lib\%BUILD_TYPE%\masm_hotpatch_unified.lib" (
    echo [OK] masm_hotpatch_unified.lib
    dir /b build_masm\lib\%BUILD_TYPE%\*.lib
) else (
    echo [FAIL] Libraries not built
)

echo.

if exist "build_masm\bin\tests\%BUILD_TYPE%\masm_hotpatch_test.exe" (
    echo [OK] masm_hotpatch_test.exe
) else (
    echo [FAIL] Test executable not built
)

echo.
echo ========================================
echo Build complete!
echo ========================================
echo.
echo Output directory: build_masm\lib\%BUILD_TYPE%
echo Test executable: build_masm\bin\tests\%BUILD_TYPE%\masm_hotpatch_test.exe
echo.

endlocal
