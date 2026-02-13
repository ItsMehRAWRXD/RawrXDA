@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d d:\RawrXD\src\masm
ml64.exe /c RawrXD_NativeHttpServer.asm
lib.exe RawrXD_NativeHttpServer.obj /OUT:RawrXD_NativeHttpServer.lib
cl.exe /std:c++17 /EHsc /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt" /I"D:\RawrXD\include" /I"D:\RawrXD\src" test_http_server.cpp ..\..\Ship\NativeHttpServerStubs.cpp ..\backend\agentic_tools.cpp ..\backend\ollama_client.cpp ..\tools\file_ops.cpp ..\tools\git_client.cpp RawrXD_NativeHttpServer.lib /FeTestHttpServer.exe /link /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64" user32.lib httpapi.lib advapi32.lib shell32.lib winhttp.lib /LARGEADDRESSAWARE:NO
if %errorlevel% equ 0 (
    TestHttpServer.exe
)
