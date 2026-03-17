@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64

set WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\
set WindowsSDKVersion=10.0.22621.0\
set WindowsSdkBinPath=C:\Program Files (x86)\Windows Kits\10\bin\
set WindowsSdkVerBinPath=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\

set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\winrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\cppwinrt
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64
set PATH=%PATH%;C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64

cmake -S D:\rawrxd -B D:\rawrxd\build_test_parse -G Ninja -DRAWRXD_BUILD_WIN32IDE=ON -DRAWRXD_BUILD_TESTS=OFF -DCMAKE_RC_COMPILER="C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe"
