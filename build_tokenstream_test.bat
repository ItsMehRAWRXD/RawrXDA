@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe" /EHsc /std:c++17 /O2 /W4 d:\rawrxd\AI_TokenStream_Test.cpp /Fe:d:\rawrxd\AI_TokenStream_Test.exe /link winhttp.lib
