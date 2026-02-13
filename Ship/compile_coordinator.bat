@echo off
setlocal enabledelayedexpansion

REM Set VS2022 paths
set "VSPATH=C:\VS2022Enterprise"
set "MSVCVER=14.50.35717"

REM Set environment variables
set "PATH=!VSPATH!\VC\Tools\MSVC\!MSVCVER!\bin\Hostx64\x64;!VSPATH!\Common7\IDE;!PATH!"
set "INCLUDE=!VSPATH!\VC\Tools\MSVC\!MSVCVER!\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared"
set "LIB=!VSPATH!\VC\Tools\MSVC\!MSVCVER!\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

REM Compile
cl.exe /O2 /DNDEBUG /EHsc /std:c++17 /W3 RawrXD_AgentCoordinator.cpp /link kernel32.lib user32.lib gdi32.lib shell32.lib /DLL /OUT:RawrXD_AgentCoordinator.dll

if errorlevel 1 (
    echo Compilation failed
    exit /b 1
) else (
    echo Compilation succeeded
    dir RawrXD_AgentCoordinator.dll
)
