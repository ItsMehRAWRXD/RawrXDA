@echo off
REM Final working link command
cd /d "D:\RawrXD-production-lazy-init\src\masm"

REM Use the linker with full path in quotes
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" /OUT:RawrXD_IDE_core.exe /SUBSYSTEM:CONSOLE /MACHINE:X64 /DEBUG agentic_kernel.obj language_scaffolders.obj kernel32.lib user32.lib shell32.lib advapi32.lib

if %errorlevel% equ 0 (
    echo SUCCESS: RawrXD_IDE_core.exe created
    dir RawrXD_IDE_core.exe
) else (
    echo FAILED with error code %errorlevel%
)
