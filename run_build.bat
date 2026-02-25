@echo off
set "PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64;C:\Program Files\CMake\bin;%PATH%"
set "INCLUDE=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um"
set "LIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
cd /d D:\rawrxd
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -S . -B build >build_config.log 2>&1
if errorlevel 1 (echo CONFIG_FAILED & type build_config.log & exit /b 1)
echo CONFIG_OK
cd /d D:\rawrxd\build
nmake RawrXD-Win32IDE >build_stdout.log 2>build_stderr.log
echo EXIT_CODE=%ERRORLEVEL%>>build_stdout.log
