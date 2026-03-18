@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\um\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.26100.0\um\x64
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared
set PATH=%PATH%;C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64
"C:\Program Files\CMake\bin\cmake.exe" -S d:\rawrxd -B d:\rawrxd\build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_RC_COMPILER="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe" -DCMAKE_MT="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/mt.exe"
