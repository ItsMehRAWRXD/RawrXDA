@echo off
REM Manual VS2022 + Windows SDK environment setup for build
REM The vcvarsall.bat is not properly configuring INCLUDE paths

set "VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\"
set "VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\"
set "VCToolsInstallDir=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\"
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\"
set "WindowsSDKVersion=10.0.22621.0"
set "UCRTVersion=10.0.22621.0"

REM MSVC paths
set "MSVC_TOOLS=%VCToolsInstallDir%"
set "MSVC_BIN=%MSVC_TOOLS%bin\Hostx64\x64"

REM Set INCLUDE
set "INCLUDE=%MSVC_TOOLS%include;%WindowsSdkDir%Include\%WindowsSDKVersion%\ucrt;%WindowsSdkDir%Include\%WindowsSDKVersion%\um;%WindowsSdkDir%Include\%WindowsSDKVersion%\shared;%WindowsSdkDir%Include\%WindowsSDKVersion%\winrt;%WindowsSdkDir%Include\%WindowsSDKVersion%\cppwinrt"

REM Set LIB
set "LIB=%MSVC_TOOLS%lib\x64;%WindowsSdkDir%Lib\%WindowsSDKVersion%\ucrt\x64;%WindowsSdkDir%Lib\%WindowsSDKVersion%\um\x64"

REM Set PATH
set "PATH=%MSVC_BIN%;%WindowsSdkDir%bin\%WindowsSDKVersion%\x64;%WindowsSdkDir%bin\x64;C:\Program Files\CMake\bin;%PATH%"

echo === Environment Check ===
where cl
where rc
echo === End Check ===

cd /d d:\rawrxd

if exist build\CMakeCache.txt del /f build\CMakeCache.txt
if not exist build mkdir build
cd build

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if errorlevel 1 (
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)

cmake --build . --config Release --target RawrXD-Win32IDE 2>&1
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo BUILD SUCCEEDED
