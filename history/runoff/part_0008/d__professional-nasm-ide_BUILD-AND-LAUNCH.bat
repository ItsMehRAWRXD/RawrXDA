@echo off
cd /d "%~dp0"
echo Building NASM IDE...

REM Create directories if they don't exist
if not exist build mkdir build
if not exist bin mkdir bin
if not exist lib mkdir lib

echo Preparing core DLL (build-asm-core.bat)...
call build-asm-core.bat >nul 2>nul
if errorlevel 1 (
	echo [WARN] Core DLL build failed or skipped. Continuing without DLL integration.
)

nasm -f win64 src\dx_ide_main.asm -o build\dx_ide_main.obj
if errorlevel 1 goto error

"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe" /NOLOGO /ENTRY:main /SUBSYSTEM:WINDOWS /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" /OUT:bin\nasm_ide_dx.exe build\dx_ide_main.obj build\file_system.obj build\chat_pane.obj kernel32.lib user32.lib gdi32.lib comdlg32.lib
if errorlevel 1 goto error

echo.
echo Build successful! Launching IDE...
echo.
echo If Windows SmartScreen appears, click "More info" then "Run anyway"
echo.
timeout /t 2 /nobreak >nul
start "" "bin\nasm_ide_dx.exe"
goto end

:error
echo.
echo BUILD FAILED!
pause
goto end

:end
