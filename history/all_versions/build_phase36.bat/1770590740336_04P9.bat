@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64

cd /d d:\rawrxd

if exist build rmdir /s /q build
mkdir build
cd build

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if errorlevel 1 (
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)

cmake --build . --config Release --target RawrXD-Win32IDE
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo BUILD SUCCEEDED
