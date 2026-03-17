@echo off
setlocal
cd /d D:\RawrXD-ExecAI

REM Set up paths
set MASM_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64
set LINK_EXE=%MASM_PATH%\link.exe

REM Move obj file if needed
if exist gguf_analyzer_working.obj move gguf_analyzer_working.obj build\Release\

REM Link
echo Linking...
"%LINK_EXE%" /nologo /subsystem:console /entry:main build\Release\gguf_analyzer_working.obj /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" kernel32.lib /out:build\Release\gguf_analyzer_working.exe

if errorlevel 1 (
    echo Linking failed
    exit /b 1
)

echo Build successful!
echo.
echo Running analyzer...
build\Release\gguf_analyzer_working.exe
