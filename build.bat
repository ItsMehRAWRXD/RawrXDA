@echo off
setlocal enabledelayedexpansion

set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
set SOURCE=D:\RawrXD\src\RawrXD_NativeModelBridge_Complete.asm
set OBJ=D:\RawrXD\bin\RawrXD_NativeModelBridge.obj
set DLL=D:\RawrXD\bin\RawrXD_NativeModelBridge.dll
set LIB=D:\RawrXD\bin\RawrXD_NativeModelBridge.lib

echo Assembling...
%ML64% /c /W3 /Fo%OBJ% %SOURCE%

if !errorlevel! neq 0 (
    echo Assembly failed
    exit /b !errorlevel!
)

echo Linking...
%LINK% /DLL /SUBSYSTEM:WINDOWS /MACHINE:x64 /OUT:%DLL% /IMPLIB:%LIB% %OBJ% kernel32.lib ntdll.lib user32.lib msvcrt.lib

if !errorlevel! neq 0 (
    echo Linking failed
    exit /b !errorlevel!
)

echo Build successful!
echo DLL: %DLL%
