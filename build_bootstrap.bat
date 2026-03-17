@echo off
set "VC_VARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
set "WIN_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
cd /d d:\rawrxd
"%VC_VARS%\ml64.exe" /c /Zi d:\rawrxd\src\agentic\main_bootstrap.asm d:\rawrxd\src\agentic\RawrXD_Monolithic_PE_Emitter.asm
"%VC_VARS%\link.exe" /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main main_bootstrap.obj RawrXD_Monolithic_PE_Emitter.obj /LIBPATH:"%WIN_LIB%" kernel32.lib /OUT:d:\rawrxd\bin\bootstrap_compiler.exe
