@echo off
setlocal enabledelayedexpansion

cd /d "D:\lazy init ide"
mkdir build_digestion 2>nul
cd build_digestion

:: Configure with AVX-512 support
:: Note: Adjust the Qt path if necessary to match your environment
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_CXX_FLAGS="/arch:AVX512 /O2 /DNDEBUG" ^
    -DQt6_DIR="C:\Qt\6.7.3\msvc2022_64\lib\cmake\Qt6"

:: Build
cmake --build . --config Release --target RawrXDDigestion -j16

:: Copy to output (optional, depends on project structure)
:: copy Release\RawrXDDigestion.lib ..\lib\
:: copy Release\RawrXDDigestion.dll ..\bin\

echo Digestion Engine build complete.
pause
