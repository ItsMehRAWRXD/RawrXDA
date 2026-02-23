@echo off
setlocal
echo Building RawrXD Phase-3 Agent Kernel...

REM Prefer VS developer environment so ml64/link and SDK are found automatically
set "VSWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "InstallDir="
if exist "%VSWhere%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "InstallDir=%%i"
)
if defined InstallDir (
    set "VCVARS=%InstallDir%\VC\Auxiliary\Build\vcvars64.bat"
    if exist "%VCVARS%" (
        echo Using VS at: %InstallDir%
        call "%VCVARS%" >nul 2>&1
    )
)

REM Fallback: hardcoded paths if vcvars wasn't used (e.g. vswhere not found)
where ml64 >nul 2>&1
if errorlevel 1 (
    set "VSTOOLS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64"
    set "WINSDK=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
    if exist "%VSTOOLS%\ml64.exe" (
        set "PATH=%VSTOOLS%;%PATH%"
        echo Using fallback toolchain: %VSTOOLS%
    ) else (
        echo ERROR: Visual Studio 2022 x64 tools not found.
        echo Run this from "Developer Command Prompt for VS 2022" or install Build Tools.
        pause
        exit /b 1
    )
)

REM Ensure bin exists
if not exist "bin" mkdir bin

REM Assemble
echo Assembling Phase3_Agent_Kernel_Complete.asm...
ml64.exe /c /Zi /Fo src\agentic\Phase3_Agent_Kernel_Complete.obj src\agentic\Phase3_Agent_Kernel_Complete.asm
if errorlevel 1 (
    echo Assembly failed!
    pause
    exit /b 1
)

REM Link DLL (LIBPATH only needed when not using vcvars)
set "LINK_EXTRA="
if defined WINSDK set "LINK_EXTRA=/LIBPATH:%WINSDK%"
echo Linking DLL...
link.exe /DLL /OUT:bin\Phase3_Agent_Kernel.dll /SUBSYSTEM:WINDOWS /LARGEADDRESSAWARE ^
    src\agentic\Phase3_Agent_Kernel_Complete.obj ^
    kernel32.lib user32.lib comdlg32.lib shell32.lib ole32.lib ntdll.lib ^
    %LINK_EXTRA%
if errorlevel 1 (
    echo Linking failed!
    pause
    exit /b 1
)

echo Build successful! DLL created at bin\Phase3_Agent_Kernel.dll
pause
endlocal
