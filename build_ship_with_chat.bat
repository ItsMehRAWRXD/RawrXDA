@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\RawrXD\Ship
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "NMake Makefiles"
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)
nmake RawrXD_Agent_GUI
if errorlevel 1 (
    echo Build failed
    exit /b 1
)
echo Build complete!
dir RawrXD_Agent_GUI.exe
