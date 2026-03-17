@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\RawrXD

set SDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0
set SDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0

cl.exe /EHsc /std:c++20 /I"src\masm" /I"%SDK_INC%\um" /I"%SDK_INC%\shared" /I"%SDK_INC%\ucrt" /I"%SDK_INC%\winrt" /I"%SDK_INC%\cppwinrt" test_http_chat_integration.cpp src\masm\RawrXD_HttpChatServer.lib /link /LIBPATH:"%SDK_LIB%\um\x64" /LIBPATH:"%SDK_LIB%\ucrt\x64" kernel32.lib user32.lib wininet.lib shlwapi.lib advapi32.lib shell32.lib /Fe:test_http_chat.exe
if errorlevel 1 (
    echo Compilation failed
    exit /b 1
)
echo.
echo Build successful! Running test...
echo.
test_http_chat.exe
