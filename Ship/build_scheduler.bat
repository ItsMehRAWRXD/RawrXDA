@echo off
setlocal enabledelayedexpansion
cd /d "D:\RawrXD\Ship"
set "M=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "W=C:\Program Files (x86)\Windows Kits\10"
set "PATH=!M!\bin\Hostx64\x64;!PATH!"
cl /I"!W!\Include\10.0.22621.0\um" /I"!W!\Include\10.0.22621.0\shared" /I"!W!\Include\10.0.22621.0\ucrt" /I"!M!\include" /O2 /DNDEBUG /MD /EHsc RawrXD_TaskScheduler.cpp /link /LIBPATH:"!M!\lib\onecore\x64" /LIBPATH:"!W!\Lib\10.0.22621.0\um\x64" /LIBPATH:"!W!\Lib\10.0.22621.0\ucrt\x64" kernel32.lib user32.lib /DLL /OUT:RawrXD_TaskScheduler.dll && dir RawrXD_TaskScheduler.dll
