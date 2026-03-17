@echo off
REM Titan AVX-512 Build Script

set ML64=ml64.exe
set CL=cl.exe
set LINK=link.exe
set OUTDIR=build\titan

if not exist %OUTDIR% mkdir %OUTDIR%

echo [Building Titan AVX-512 Core...]
%ML64% /c /Fo%OUTDIR%\RawrXD_Titan.obj /W3 /nologo src\RawrXD_Titan.asm
if errorlevel 1 goto error

echo [Building Titan C++ Integration...]
REM Assuming a cpp file that uses it, here we just check syntax or build a dummy dll
REM For now we just link the object into a DLL to verify exports

%LINK% /DLL /OUT:%OUTDIR%\TitanMath.dll /NOLOGO %OUTDIR%\RawrXD_Titan.obj /NOENTRY kernel32.lib
if errorlevel 1 goto error

echo [Success] TitanMath.dll created.
goto done

:error
echo [Error] Build failed.
exit /b 1

:done
exit /b 0
