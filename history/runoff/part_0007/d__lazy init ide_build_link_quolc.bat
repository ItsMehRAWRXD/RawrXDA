@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Base path is the folder of this script (build\)
set "BASE=%~dp0"

REM Build response file
set "RSP=%BASE%obj\quolc.rsp"
del /f /q "%RSP%" 2>nul

echo /DEBUG>"%RSP%"
echo /SUBSYSTEM:CONSOLE>>"%RSP%"
echo /ENTRY:RawrXD_Main>>"%RSP%"
echo /OUT:"%BASE%bin\quolc.exe">>"%RSP%"

echo "%BASE%obj\RawrXD_Singularity_Engine.obj">>"%RSP%"
if exist "%BASE%obj\agentic_core.obj" echo "%BASE%obj\agentic_core.obj">>"%RSP%"
if exist "%BASE%obj\singularity_engine_integration.obj" echo "%BASE%obj\singularity_engine_integration.obj">>"%RSP%"
if exist "%BASE%obj\runtime.obj" echo "%BASE%obj\runtime.obj">>"%RSP%"

echo kernel32.lib>>"%RSP%"
echo ntdll.lib>>"%RSP%"

REM Try MSVC link, then fallback to LLVM lld-link
link @"%RSP%"
if errorlevel 1 lld-link @"%RSP%"
exit /b %ERRORLEVEL%
