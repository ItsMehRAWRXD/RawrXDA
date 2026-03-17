@echo off
setlocal

set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"

echo Compiling RawrXD_Titan_MetaReverse.asm...
%ML64% /c /Zi /D"METAREV=5" RawrXD_Titan_MetaReverse.asm
if %errorlevel% neq 0 exit /b 1

echo Linking RawrXD-Titan.exe...
%LINK% /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:PEBMain /OUT:RawrXD-Titan.exe RawrXD_Titan_MetaReverse.obj
if %errorlevel% neq 0 exit /b 1

echo Build Success
