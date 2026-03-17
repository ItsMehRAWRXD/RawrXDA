@echo off
REM Configure CMake with MSVC + ml64 via Ninja
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Verify tools
where cl.exe
where ml64.exe
where rc.exe
where link.exe

REM Remove stale build
if exist build rmdir /s /q build

REM Configure
cmake -G Ninja ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_ASM_MASM_COMPILER=ml64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_RC_COMPILER=rc ^
  -B build -S .

REM Build
cmake --build build --config Release --target RawrXD-Win32IDE
