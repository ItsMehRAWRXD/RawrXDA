@echo off
setlocal EnableExtensions EnableDelayedExpansion
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
ml64 /c /Cx /Zi /Fo "%~dp0obj\RawrXD_Singularity_Engine.obj" "%~dp0..\singularity_min.asm"
exit /b %ERRORLEVEL%
