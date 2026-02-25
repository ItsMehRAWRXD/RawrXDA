@echo off
setlocal

:: Paths
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" (
    set "ML64_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
    set "LINK_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
) else (
    set "ML64_CMD=ml64.exe"
    set "LINK_CMD=link.exe"
)

set "SDK_LIB_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"

:: Assemble
"%ML64_CMD%" /c /Zi /arch:AVX512 Titan_InferenceCore.asm
if %errorlevel% neq 0 exit /b 1

"%ML64_CMD%" /c /Zi /arch:AVX512 Titan_Streaming_Orchestrator_Fixed.asm
if %errorlevel% neq 0 exit /b 1

"%ML64_CMD%" /c /Zi RawrXD_CLI.asm
if %errorlevel% neq 0 exit /b 1

:: Link
"%LINK_CMD%" /SUBSYSTEM:CONSOLE /ENTRY:main /LIBPATH:"%SDK_LIB_PATH%" /OUT:RawrXD-Agent.exe ^
    Titan_InferenceCore.obj ^
    Titan_Streaming_Orchestrator_Fixed.obj ^
    RawrXD_CLI.obj ^
    kernel32.lib
if %errorlevel% neq 0 exit /b 1

echo Build Success
