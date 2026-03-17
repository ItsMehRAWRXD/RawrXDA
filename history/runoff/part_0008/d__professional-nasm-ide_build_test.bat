@echo off
cd /d "%~dp0"

nasm -f win64 test_extensions.asm -o build\test_extensions.obj
if errorlevel 1 goto error

"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe" /NOLOGO /ENTRY:main /SUBSYSTEM:CONSOLE /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" /OUT:bin\test_extensions.exe build\test_extensions.obj kernel32.lib user32.lib
if errorlevel 1 goto error

echo Build successful!
echo Running test...
bin\test_extensions.exe
goto end

:error
echo Build failed!
pause

:end