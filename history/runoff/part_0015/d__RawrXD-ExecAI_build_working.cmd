@echo off
cd /d D:\RawrXD-ExecAI
echo Assembling gguf_analyzer_working.asm...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" gguf_analyzer_working.asm /c /nologo /W3 /Fo build\Release\gguf_working.obj
if errorlevel 1 (
    echo Assembly failed
    exit /b 1
)
echo Linking...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" /nologo /subsystem:console /entry:main build\Release\gguf_working.obj /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" kernel32.lib /out:build\Release\gguf_analyzer_working.exe
if errorlevel 1 (
    echo Linking failed
    exit /b 1
)
echo Build successful!
echo.
echo Running analyzer...
build\Release\gguf_analyzer_working.exe
