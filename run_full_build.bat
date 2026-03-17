@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\rawrxd
if exist build rmdir /s /q build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -S . -B build 1>build\cmake_config.log 2>&1
if errorlevel 1 (echo CMAKE_FAILED & type build\cmake_config.log & exit /b 1)
echo CMAKE_OK
cd /d D:\rawrxd\build
nmake RawrXD-Win32IDE 1>build_stdout.log 2>build_stderr.log
echo EXIT=%ERRORLEVEL% >>build_stdout.log
