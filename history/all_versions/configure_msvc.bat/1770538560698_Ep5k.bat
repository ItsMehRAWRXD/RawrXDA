@echo off
REM Configure CMake with MSVC + ml64 via Ninja
REM Use SDK 10.0.22621.0 which has complete bin tools (rc.exe)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 10.0.22621.0 >nul 2>&1

REM Add SDK bin to PATH explicitly in case vcvars misses it
set "PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64;%PATH%"

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
  -DCMAKE_RC_COMPILER="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe" ^
  -DCMAKE_MT="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/mt.exe" ^
  -DCMAKE_SYSTEM_VERSION=10.0.22621.0 ^
  -B build -S .
