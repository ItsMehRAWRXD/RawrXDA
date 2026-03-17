@echo off
setlocal enabledelayedexpansion

REM Initialize Visual Studio 2022 environment
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Change to project directory
cd /d D:\temp\RawrXD-agentic-ide-production

REM Run qmake
echo Generating makefiles with qmake...
"C:\Qt\6.7.3\msvc2022_64\bin\qmake.exe" RawrXD-IDE.pro CONFIG+=release

if errorlevel 1 (
    echo ERROR: qmake failed
    exit /b 1
)

echo.
echo Makefiles generated successfully
echo Running nmake build...
echo.

REM Run nmake
nmake VERBOSE=1

if errorlevel 1 (
    echo ERROR: nmake failed
    exit /b 1
)

echo.
echo Build completed successfully!
