@echo off
cd /d d:\rawrxd\src
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\ml64.exe" /c rawrxd_compiler_masm64.asm
if %errorlevel% neq 0 (
    echo Build failed with error %errorlevel%
    pause
) else (
    echo Build successful
)