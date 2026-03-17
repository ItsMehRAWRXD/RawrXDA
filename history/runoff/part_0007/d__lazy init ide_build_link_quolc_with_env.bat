@echo off
setlocal EnableExtensions EnableDelayedExpansion
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%~dp0link_quolc.bat"
exit /b %ERRORLEVEL%
