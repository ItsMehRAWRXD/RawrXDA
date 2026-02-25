@echo off
echo Building Marketplace Installer Test Suite...

REM Set paths
set "VSTOOLS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
set "PATH=%VSTOOLS%;%PATH%"

REM Create test directories
if not exist "tests\fixtures" mkdir tests\fixtures
if not exist "tests\output" mkdir tests\output

REM Assemble test suite
echo Assembling test suite...
ml64.exe /c /Zi /I "D:\RawrXD\include" /Fo tests\test_marketplace_installer.obj tests\test_marketplace_installer.asm
if errorlevel 1 (
    echo Test assembly failed!
    pause
    exit /b 1
)

REM Link test executable
echo Linking test executable...
link.exe /OUT:tests\test_marketplace_installer.exe /SUBSYSTEM:CONSOLE ^
    tests\test_marketplace_installer.obj ^
    src\agentic\marketplace_installer.obj ^
    kernel32.lib user32.lib msvcrt.lib ^
    /LIBPATH:"%WINSDK%"

if errorlevel 1 (
    echo Test linking failed!
    pause
    exit /b 1
)

echo Build successful!
echo.
echo Running tests...
tests\test_marketplace_installer.exe

pause
