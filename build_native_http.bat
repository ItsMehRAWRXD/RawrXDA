@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d D:\RawrXD\src\masm

echo Building RawrXD_NativeHttpServer.obj...
ml64.exe /c /Cp /FoRawrXD_NativeHttpServer.obj RawrXD_NativeHttpServer.asm
if errorlevel 1 (
    echo FAILED: ml64.exe assembly error
    exit /b 1
)

echo Checking for .obj file...
if not exist RawrXD_NativeHttpServer.obj (
    echo FAILED: RawrXD_NativeHttpServer.obj not found
    dir RawrXD_Native*.obj 2>nul || echo No .obj files found
    exit /b 1
)

echo Building RawrXD_NativeHttpServer.lib...
lib.exe /OUT:RawrXD_NativeHttpServer.lib RawrXD_NativeHttpServer.obj /MACHINE:X64
if errorlevel 1 (
    echo FAILED: lib.exe linking error
    exit /b 1
)

echo.
echo SUCCESS: Native HTTP Server library created
dir /B RawrXD_NativeHttpServer.*
