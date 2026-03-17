@echo off
REM Wrapper to run build with VS environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\temp\RawrXD-agentic-ide-production
call build_sovereign.bat
