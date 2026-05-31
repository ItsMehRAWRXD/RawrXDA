@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
cd /d d:\rawrxd\src\titan
"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe" /O2 /LD /FeRawrXD_Titan.dll RawrXD_Titan.cpp /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt" /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" winhttp.lib
