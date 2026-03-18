@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\um\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.26100.0\um\x64
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared
set PATH=%PATH%;C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64
cd /d d:\rawrxd\build
cl /c /std:c++20 /EHsc /I"d:\rawrxd\src" /I"d:\rawrxd\src\agentic" /I"d:\rawrxd\include" /I"d:\rawrxd\src\core" /I"d:\rawrxd\extern" /I"d:\rawrxd\extern\nlohmann" /D_WIN32 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Fod:\rawrxd\test_compile.obj "d:\rawrxd\src\agentic\AgentOllamaClient.cpp" 2>&1
