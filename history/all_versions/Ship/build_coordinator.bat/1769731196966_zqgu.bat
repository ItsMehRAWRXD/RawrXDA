@echo off
setlocal enabledelayedexpansion

cd /d "D:\RawrXD\Ship"

REM Direct MSVC paths
set "MSVC=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10"

REM Set full environment
set "PATH=!MSVC!\bin\Hostx64\x64;!PATH!"
set "INCLUDE=!MSVC!\include;!WINSDK!\Include\10.0.22621.0\ucrt;!WINSDK!\Include\10.0.22621.0\um;!WINSDK!\Include\10.0.22621.0\shared"
set "LIB=!MSVC!\lib\x64;!WINSDK!\Lib\10.0.22621.0\ucrt\x64;!WINSDK!\Lib\10.0.22621.0\um\x64"

REM Compile - explicit Windows header includes
REM Use /NODEFAULTLIB and add minimal CRT libs
cl.exe /I"!WINSDK!\Include\10.0.22621.0\um" /I"!WINSDK!\Include\10.0.22621.0\ucrt" /I"!MSVC!\include" /O2 /DNDEBUG /EHsc RawrXD_AgentCoordinator_Minimal.cpp /link /NODEFAULTLIB /LIBPATH:"!WINSDK!\Lib\10.0.22621.0\um\x64" /LIBPATH:"!WINSDK!\Lib\10.0.22621.0\ucrt\x64" /LIBPATH:"!MSVC!\lib\x64" kernel32.lib user32.lib gdi32.lib shell32.lib ucrt.lib /DLL /OUT:RawrXD_AgentCoordinator.dll

if errorlevel 1 (
    echo Compilation FAILED
    dir RawrXD_AgentCoordinator.dll 2>&1 | find "cannot"
    exit /b 1
)

echo Compilation SUCCEEDED
dir RawrXD_AgentCoordinator.dll
