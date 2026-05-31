@echo off
REM Developer Command Prompt for VS 2022 with proper environment
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64

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
