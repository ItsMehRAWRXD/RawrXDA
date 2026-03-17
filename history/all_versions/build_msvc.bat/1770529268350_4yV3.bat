@echo off
REM ══════════════════════════════════════════════════════════════════
REM MSVC Build Script for RawrXD-Win32IDE
REM Uses VS2022 BuildTools + Windows SDK 10.0.22621.0
REM ══════════════════════════════════════════════════════════════════

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64 10.0.22621.0

if errorlevel 1 (
    echo [ERROR] vcvarsall.bat failed
    exit /b 1
)

echo.
echo ── MSVC Environment Ready ──
cl 2>&1 | findstr /C:"Version"
echo RC: 
where rc.exe
echo.

cd /d D:\rawrxd

if exist build_msvc rmdir /s /q build_msvc

echo ── CMake Configure ──
cmake -S . -B build_msvc -G Ninja ^
    -DCMAKE_C_COMPILER=cl ^
    -DCMAKE_CXX_COMPILER=cl ^
    -DCMAKE_RC_COMPILER="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_SYSTEM_VERSION=10.0.22621.0

if errorlevel 1 (
    echo [ERROR] CMake configure failed
    exit /b 1
)

echo.
echo ── CMake Build ──
cmake --build build_msvc --config Release --target RawrXD-Win32IDE 2>&1

echo.
echo ── Build Complete ──
if exist build_msvc\bin\RawrXD-Win32IDE.exe (
    echo [SUCCESS] build_msvc\bin\RawrXD-Win32IDE.exe
) else (
    echo [FAIL] Binary not found
)
