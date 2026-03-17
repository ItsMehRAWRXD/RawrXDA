@echo off
REM Simple test link with minimal configuration

cd /d C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\src\masm\final-ide

set LINK="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

echo Attempting simple link test...

REM Try linking with /ENTRY:main_masm to avoid entry point issues
%LINK% /NOLOGO /SUBSYSTEM:WINDOWS /MACHINE:X64 /OUT:bin\test.exe ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" ^
    kernel32.lib user32.lib ^
    obj\asm_memory.obj obj\malloc_wrapper.obj 2>&1

echo.
if exist bin\test.exe (
    echo SUCCESS - basic linking works
    del bin\test.exe
) else (
    echo FAILED - basic linking failed
    exit /b 1
)
