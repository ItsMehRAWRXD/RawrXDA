@echo off
REM ============================================================================
REM RawrXD Gold Build — Standalone Deployment
REM All MASM modules linked, zero unresolved externals, static CRT
REM ============================================================================

REM Set up minimal environment for link resolution
set "MSVC_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
set "PATH=%MSVC_ROOT%\bin\Hostx64\x64;%SDK_ROOT%\bin\10.0.22621.0\x64;%PATH%"
set "LIB=%MSVC_ROOT%\lib\x64;%SDK_ROOT%\Lib\10.0.22621.0\ucrt\x64;%SDK_ROOT%\Lib\10.0.26100.0\um\x64"
set "INCLUDE=%MSVC_ROOT%\include;%SDK_ROOT%\Include\10.0.22621.0\ucrt;%SDK_ROOT%\Include\10.0.26100.0\shared;%SDK_ROOT%\Include\10.0.26100.0\um"

cd /d %~dp0

if not exist build_gold mkdir build_gold
cd build_gold

echo [GOLD] Configuring CMake (fresh build)...
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER="%MSVC_ROOT%\bin\Hostx64\x64\cl.exe" ^
  -DCMAKE_CXX_COMPILER="%MSVC_ROOT%\bin\Hostx64\x64\cl.exe" ^
  -DCMAKE_LINKER="%MSVC_ROOT%\bin\Hostx64\x64\link.exe" ^
  -DCMAKE_RC_COMPILER="%SDK_ROOT%\bin\10.0.22621.0\x64\rc.exe" ^
  -DCMAKE_MT="%SDK_ROOT%\bin\10.0.22621.0\x64\mt.exe" ^
  -DCMAKE_ASM_MASM_COMPILER="%MSVC_ROOT%\bin\Hostx64\x64\ml64.exe" ^
  2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [GOLD] CMake configure FAILED
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
