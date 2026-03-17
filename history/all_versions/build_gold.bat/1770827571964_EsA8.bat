@echo off
REM ============================================================================
REM RawrXD Gold Build — Standalone Deployment
REM All MASM modules linked, zero unresolved externals, static CRT
REM Uses existing pre-configured build directory (Ninja + MSVC)
REM ============================================================================

REM Set up minimal environment for link resolution
set "MSVC_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
set "PATH=%MSVC_ROOT%\bin\Hostx64\x64;%SDK_ROOT%\bin\10.0.22621.0\x64;%PATH%"
set "LIB=%MSVC_ROOT%\lib\x64;%SDK_ROOT%\Lib\10.0.22621.0\ucrt\x64;%SDK_ROOT%\Lib\10.0.26100.0\um\x64"
set "INCLUDE=%MSVC_ROOT%\include;%SDK_ROOT%\Include\10.0.22621.0\ucrt;%SDK_ROOT%\Include\10.0.26100.0\shared;%SDK_ROOT%\Include\10.0.26100.0\um"

cd /d %~dp0\build

echo [GOLD] Re-configuring CMake in existing build directory...
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [GOLD] CMake reconfigure FAILED
    exit /b 1
)

echo.
echo [GOLD] Building RawrXD_Gold target...
cmake --build . --target RawrXD_Gold 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [GOLD] Build FAILED — check errors above
    exit /b 1
)

echo.
echo ============================================================================
echo [GOLD] BUILD SUCCESS
echo ============================================================================
dir gold\RawrXD_Gold.exe 2>nul
