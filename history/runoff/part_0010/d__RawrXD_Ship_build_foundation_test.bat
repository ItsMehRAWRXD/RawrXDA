@echo off
setlocal enabledelayedexpansion
cd /d "D:\RawrXD\Ship"
set "M=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "W=C:\Program Files (x86)\Windows Kits\10"
set "PATH=!M!\bin\Hostx64\x64;!PATH!"

echo Compiling RawrXD_FoundationTest.exe...
echo.

cl /I"!W!\Include\10.0.22621.0\um" /I"!W!\Include\10.0.22621.0\shared" /I"!W!\Include\10.0.22621.0\ucrt" /I"!M!\include" /O2 /DNDEBUG /MD /EHsc /std:c++17 RawrXD_FoundationTest.cpp /link /LIBPATH:"!M!\lib\onecore\x64" /LIBPATH:"!W!\Lib\10.0.22621.0\um\x64" /LIBPATH:"!W!\Lib\10.0.22621.0\ucrt\x64" kernel32.lib user32.lib psapi.lib dbghelp.lib /SUBSYSTEM:CONSOLE /OUT:RawrXD_FoundationTest.exe

if %ERRORLEVEL% equ 0 (
    echo.
    echo ✓ Test executable built successfully!
    echo.
    dir RawrXD_FoundationTest.exe
    echo.
    echo Running Foundation Integration Test...
    echo.
    RawrXD_FoundationTest.exe
) else (
    echo.
    echo ✗ Build failed with error %ERRORLEVEL%
)
