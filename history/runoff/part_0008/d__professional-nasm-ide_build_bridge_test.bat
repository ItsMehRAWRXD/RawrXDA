@echo off
cd /d "%~dp0"

nasm -f win64 test_bridge_init.asm -o build\test_bridge_init.obj
if errorlevel 1 goto error

"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe" /NOLOGO /ENTRY:main /SUBSYSTEM:WINDOWS /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" /OUT:bin\test_bridge_init.exe build\test_bridge_init.obj kernel32.lib user32.lib
if errorlevel 1 goto error

echo Build successful!
echo Running bridge_init test...
bin\test_bridge_init.exe
goto end

:error
echo Build failed!
pause

:end