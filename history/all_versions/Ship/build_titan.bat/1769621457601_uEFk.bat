@echo off
echo [RawrXD-Titan] MetaReverse + CLI + GUI Build
echo =============================================

set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"

echo [1/4] Assembling MetaReverse (Infrastructure)...
%ML64% /c /Zi /O2 /W3 /D"METAREV=5" /arch:AVX512 RawrXD_Titan_MetaReverse.asm
if errorlevel 1 goto :fail

echo [2/4] Assembling CLI...
%ML64% /c /Zi /O2 RawrXD_CLI.asm
if errorlevel 1 goto :fail

echo [3/4] Assembling GUI...
%ML64% /c /Zi /O2 RawrXD_GUI.asm
if errorlevel 1 goto :fail

echo [4/4] Linking executables...

:: Link CLI
%LINK% /SUBSYSTEM:CONSOLE /OUT:RawrXD-Agent.exe /DEBUG /LARGEADDRESSAWARE ^
    RawrXD_CLI.obj ^
    RawrXD_Titan_MetaReverse.obj ^
    RawrXD_Streaming_Orchestrator.obj ^
    kernel32.lib user32.lib advapi32.lib ntdll.lib

:: Link GUI  
%LINK% /SUBSYSTEM:WINDOWS /OUT:RawrXD-TitanIDE.exe /DEBUG /LARGEADDRESSAWARE ^
    RawrXD_GUI.obj ^
    RawrXD_Titan_MetaReverse.obj ^
    RawrXD_Streaming_Orchestrator.obj ^
    kernel32.lib user32.lib gdi32.lib comctl32.lib advapi32.lib ntdll.lib

echo.
echo ============================================
echo BUILD SUCCESS
echo   RawrXD-Agent.exe      (Console)
echo   RawrXD-TitanIDE.exe   (Win32 IDE)
echo   MetaReverse.obj       (Infrastructure)
echo ============================================
echo.
echo Verify exports with: dumpbin /exports RawrXD-Agent.exe
goto :eof

:fail
echo BUILD FAILED
exit /b 1
