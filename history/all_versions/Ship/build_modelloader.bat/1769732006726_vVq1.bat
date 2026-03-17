@echo off
setlocal enabledelayedexpansion
cd /d "D:\RawrXD\Ship"
set "MSVC=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10"
set "PATH=!MSVC!\bin\Hostx64\x64;!PATH!"
cl.exe /I"!WINSDK!\Include\10.0.22621.0\um" /I"!WINSDK!\Include\10.0.22621.0\shared" /I"!WINSDK!\Include\10.0.22621.0\ucrt" /I"!MSVC!\include" /O2 /DNDEBUG /MD /EHsc RawrXD_ModelLoader.cpp /link /LIBPATH:"!MSVC!\lib\onecore\x64" /LIBPATH:"!WINSDK!\Lib\10.0.22621.0\um\x64" /LIBPATH:"!WINSDK!\Lib\10.0.22621.0\ucrt\x64" kernel32.lib user32.lib gdi32.lib shell32.lib /DLL /OUT:RawrXD_ModelLoader.dll
