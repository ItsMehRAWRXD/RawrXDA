@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\winrt
cl.exe /c /std:c++20 /EHsc /W3 /DRAWRXD_LINK_REVERSE_ENGINEERED_ASM=1 /I d:\rawrxd\include /I d:\rawrxd\src /I d:\rawrxd\src\core /I d:\rawrxd\src\gpu /I d:\rawrxd\src\server d:\rawrxd\src\complete_server.cpp /Fo:d:\build_verification\complete_server_test.obj
echo EXIT_CODE: %ERRORLEVEL%
