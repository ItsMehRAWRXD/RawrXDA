@echo off
REM Universal Compiler - Portable Wrapper
setlocal enabledelayedexpansion
set "args="
:loop
if "%~1"=="" goto endloop
set "args=!args! '%~1'"
shift
goto loop
:endloop
pwsh.exe -ExecutionPolicy Bypass -NoProfile -Command "& '%~dp0ucc.ps1' !args!"
