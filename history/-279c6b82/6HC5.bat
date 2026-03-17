@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

rd /s /q D:\rawrxd\build_msvc 2>nul
cd /d D:\rawrxd

cmake -S . -B build_msvc -G Ninja ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_ASM_MASM_COMPILER=ml64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_RC_COMPILER=rc ^
  -DCMAKE_MT=mt

if errorlevel 1 (
    echo === CONFIGURE FAILED ===
    exit /b 1
)

cmake --build build_msvc --config Release --target RawrXD-Win32IDE -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo === BUILD FAILED ===
    exit /b 1
)

echo === BUILD SUCCEEDED ===
