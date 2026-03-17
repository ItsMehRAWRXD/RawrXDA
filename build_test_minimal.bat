@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\rawrxd
ml64 /c /Zi RawrXD_Test_Minimal.asm
link /subsystem:console /entry:main /out:test_minimal.exe RawrXD_Test_Minimal.obj "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib" "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\user32.lib"
test_minimal.exe
