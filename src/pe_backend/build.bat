@echo off
setlocal
set VCDIR=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717
set PATH=%VCDIR%\bin\Hostx64\x64;%PATH%
set INCLUDE=%VCDIR%\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt
set LIB=%VCDIR%\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64

echo [*] Building pe_emitter.lib (static library) ...

if not exist "lib" mkdir lib

cl.exe /O2 /W4 /c /Fo pe_emitter.obj pe_emitter.c
if errorlevel 1 (echo [!] Compile failed & exit /b 1)

lib.exe /NOLOGO /OUT:lib\pe_emitter.lib pe_emitter.obj
if errorlevel 1 (echo [!] Lib failed & exit /b 1)

del pe_emitter.obj 2>nul

echo [+] Built lib\pe_emitter.lib
echo [*] Link into your project:
echo     cl.exe /O2 your_tool.c /Fe:your_tool.exe /link lib\pe_emitter.lib
