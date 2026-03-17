@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\RawrXD
cl.exe /EHsc /std:c++20 /I"src\masm" test_http_chat_integration.cpp src\masm\RawrXD_HttpChatServer.lib kernel32.lib user32.lib wininet.lib shlwapi.lib advapi32.lib shell32.lib /Fe:test_http_chat.exe
if errorlevel 1 (
    echo Compilation failed
    exit /b 1
)
echo.
echo Build successful! Running test...
echo.
test_http_chat.exe
