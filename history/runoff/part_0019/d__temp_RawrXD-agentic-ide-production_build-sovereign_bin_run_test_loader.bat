@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
cl.exe /nologo /I"..\..\src" test_loader.c /link /LIBPATH:"%~dp0" RawrXD-SovereignLoader.lib
