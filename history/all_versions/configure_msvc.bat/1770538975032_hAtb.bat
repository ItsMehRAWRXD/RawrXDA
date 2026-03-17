@echo off
REM Configure + Build RawrXD with MSVC cl.exe + ml64.exe via Ninja
REM Uses SDK 10.0.22621.0 which has complete bin tools

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

REM Manually fix up SDK paths if winsdk.bat failed to add them
REM The SDK 10.0.22621.0 include/lib/bin dirs exist but vcvars validation is buggy
set "WINKITS=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.22621.0"

REM Add missing SDK include paths (um, shared, winrt, cppwinrt)
set "INCLUDE=%INCLUDE%;%WINKITS%\include\%SDKVER%\um;%WINKITS%\include\%SDKVER%\shared;%WINKITS%\include\%SDKVER%\winrt;%WINKITS%\include\%SDKVER%\cppwinrt"

REM Add SDK lib paths
set "LIB=%LIB%;%WINKITS%\Lib\%SDKVER%\um\x64;%WINKITS%\Lib\%SDKVER%\ucrt\x64"

REM Add SDK bin to PATH for rc.exe / mt.exe
set "PATH=%WINKITS%\bin\%SDKVER%\x64;%PATH%"

REM Verify env is set
echo LIB=%LIB%
echo INCLUDE=%INCLUDE%

REM Verify tools
echo === Tool verification ===
where cl.exe
where ml64.exe
where rc.exe
where link.exe
echo === End verification ===

REM Remove stale build
if exist build rmdir /s /q build

REM Configure
cmake -G Ninja ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_ASM_MASM_COMPILER=ml64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -B build -S .
if errorlevel 1 (
    echo FATAL: CMake configure failed
    exit /b 1
)

echo.
echo === Configuration successful. Now building... ===
echo.

REM Build
cmake --build build --config Release --target RawrXD-Win32IDE
if errorlevel 1 (
    echo WARNING: Build had errors
    exit /b 1
)

echo.
echo === BUILD SUCCESSFUL ===
