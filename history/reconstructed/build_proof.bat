@echo off
setlocal

set ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe
set LINK=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe
set SDKLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64
set UCRTLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
set VCLIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64

cd /d D:\rawrxd

echo ===== STEP 1: Assemble =====
"%ML64%" /c /nologo /W3 /Fo proof.obj proof.asm
if errorlevel 1 (
    echo ASSEMBLE FAILED
    goto :done
)
echo ASSEMBLE OK

echo ===== STEP 2: Link =====
"%LINK%" /nologo /subsystem:console /entry:main /out:proof.exe proof.obj /LIBPATH:"%SDKLIB%" /LIBPATH:"%UCRTLIB%" /LIBPATH:"%VCLIB%" kernel32.lib
if errorlevel 1 (
    echo LINK FAILED
    goto :done
)
echo LINK OK

echo ===== STEP 3: Run =====
proof.exe
echo Exit code: %ERRORLEVEL%

:done
echo ===== BUILD COMPLETE =====
endlocal
