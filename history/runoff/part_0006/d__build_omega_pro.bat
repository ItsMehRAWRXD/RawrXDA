@echo off
cls
echo [+] Building RawrXD Solo Compiler...

REM Set up MSVC environment with absolute paths
set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set CL="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
set INCLUDE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include
set LIB=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64

echo [+] Assembling MASM frontend (masm_nasm_universal.asm)...
%ML64% /c /Fo"d:\RawrXD-Compilers\masm_nasm_universal.obj" "d:\RawrXD-Compilers\masm_nasm_universal.asm"
if errorlevel 1 (
    echo [-] MASM assembly failed.
    pause
    exit /b 1
)

echo [+] Compiling C++ backend (RawrXD_SoloCompiler_Backend.cpp)...
%CL% /c /Fo"d:\RawrXD-Compilers\RawrXD_SoloCompiler_Backend.obj" "d:\RawrXD-Compilers\RawrXD_SoloCompiler_Backend.cpp"
if errorlevel 1 (
    echo [-] C++ compilation failed.
    pause
    exit /b 1
)

echo [+] Linking frontend and backend...
%LINK% /SUBSYSTEM:CONSOLE /OUT:"d:\RawrXD-Compilers\masm_nasm_universal.exe" "d:\RawrXD-Compilers\masm_nasm_universal.obj" "d:\RawrXD-Compilers\RawrXD_SoloCompiler_Backend.obj" kernel32.lib
if errorlevel 1 (
    echo [-] Linking failed.
    pause
    exit /b 1
)

echo [+] Build Complete: d:\RawrXD-Compilers\masm_nasm_universal.exe
pause
