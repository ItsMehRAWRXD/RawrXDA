@echo off
REM ============================================================================
REM RawrXD Gold Build — Standalone Deployment
REM All MASM modules linked, zero unresolved externals, static CRT
REM ============================================================================
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64

REM Add missing SDK um/x64 libs (partial SDK install workaround)
set "LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"

REM Configure
if not exist build_gold mkdir build_gold
cd build_gold

echo [GOLD] Configuring CMake...
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [GOLD] CMake configure FAILED
    exit /b 1
)

echo [GOLD] Building RawrXD_Gold target...
cmake --build . --target RawrXD_Gold -- -j%NUMBER_OF_PROCESSORS% 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [GOLD] Build FAILED — check errors above
    exit /b 1
)

echo.
echo ============================================================================
echo [GOLD] BUILD SUCCESS: gold\RawrXD_Gold.exe
echo ============================================================================
dir gold\RawrXD_Gold.exe 2>nul
