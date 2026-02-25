@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64;%PATH%
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64

if exist "D:\rawrxd\build" rmdir /s /q "D:\rawrxd\build"
mkdir "D:\rawrxd\build"
cd /d "D:\rawrxd\build"

cmake -G "NMake Makefiles" ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_RC_COMPILER="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe" ^
  -DCMAKE_MT="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/mt.exe" ^
  -DCMAKE_BUILD_TYPE=Release ^
  ..

echo.
echo === CMake configure exit code: %ERRORLEVEL% ===
