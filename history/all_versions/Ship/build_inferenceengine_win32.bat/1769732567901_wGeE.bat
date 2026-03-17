@echo off
setlocal enabledelayedexpansion

REM Set up Visual Studio environment
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"

cd /d D:\RawrXD\Ship

cl.exe ^
  /O2 /DNDEBUG /MD /EHsc /std:c++17 ^
  RawrXD_InferenceEngine_Win32.cpp ^
  /link ^
  /OUT:RawrXD_InferenceEngine_Win32.dll ^
  /DLL ^
  kernel32.lib

if %ERRORLEVEL% EQU 0 (
    dir RawrXD_InferenceEngine_Win32.dll
    echo Build successful
) else (
    echo Build failed with error %ERRORLEVEL%
)
