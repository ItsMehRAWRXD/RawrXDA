@echo off
REM Manually set SDK paths to known good values
set INCLUDE=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include;C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\um;C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\shared
set LIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\um\x64
set PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64;%PATH%

REM Clean old build
del /q D:\rawrxd\build\bin\sovereign_streamer_fingerprint_test_v2.exe >nul 2>&1
del /q D:\rawrxd\tests\streaming\sovereign_streamer_fingerprint_test.obj >nul 2>&1

REM Compile the test
cd /d D:\rawrxd\tests\streaming
cl.exe /std:c++17 /O2 /EHsc /W0 sovereign_streamer_fingerprint_test.cpp /link Psapi.lib /OUT:D:\rawrxd\build\bin\sovereign_streamer_fingerprint_test_v2.exe

if %ERRORLEVEL% EQU 0 (
    echo Build succeeded
    exit /b 0
) else (
    echo Build failed with error %ERRORLEVEL%
    exit /b 1
)
