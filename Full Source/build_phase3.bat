@echo off
echo Building RawrXD Phase-3 Agent Kernel...

REM Set paths for Visual Studio tools
set "VSTOOLS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

REM Add to PATH
set "PATH=%VSTOOLS%;%PATH%"

REM Assemble
echo Assembling Phase3_Agent_Kernel_Complete.asm...
ml64.exe /c /Zi /Fo src\agentic\Phase3_Agent_Kernel_Complete.obj src\agentic\Phase3_Agent_Kernel_Complete.asm
if errorlevel 1 (
    echo Assembly failed!
    pause
    exit /b 1
)

REM Link DLL
echo Linking DLL...
link.exe /DLL /OUT:bin\Phase3_Agent_Kernel.dll /SUBSYSTEM:WINDOWS /LARGEADDRESSAWARE ^
    src\agentic\Phase3_Agent_Kernel_Complete.obj ^
    kernel32.lib user32.lib comdlg32.lib shell32.lib ole32.lib ntdll.lib ^
    /LIBPATH:"%WINSDK%"

if errorlevel 1 (
    echo Linking failed!
    pause
    exit /b 1
)

echo Build successful! DLL created at bin\Phase3_Agent_Kernel.dll
pause