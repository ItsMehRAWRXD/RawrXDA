@echo off
REM Build script for Agentic Tools - Pure MASM Implementation
REM Copyright (c) 2025 RawrXD Project

echo ===============================================
echo  Agentic Tools - Pure MASM Build Script
echo ===============================================
echo.

REM Check if ML.exe is in PATH
where ml.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: ML.exe not found in PATH
    echo Please run this from Visual Studio Developer Command Prompt
    echo.
    echo To open Developer Command Prompt:
    echo   1. Open Start Menu
    echo   2. Search for "Developer Command Prompt"
    echo   3. Select your Visual Studio version
    echo.
    pause
    exit /b 1
)

echo [1/3] Assembling agentic_tools.asm...
ml /c /coff /Zi agentic_tools.asm
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Assembly failed!
    pause
    exit /b 1
)
echo       Success!
echo.

echo [2/3] Linking agentic_tools.obj...
link /subsystem:console /entry:main /debug agentic_tools.obj kernel32.lib user32.lib
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Linking failed!
    pause
    exit /b 1
)
echo       Success!
echo.

echo [3/3] Build complete!
echo.
echo ===============================================
echo  Output: agentic_tools.exe
echo  Size:   
dir /b agentic_tools.exe 2>nul && for %%A in (agentic_tools.exe) do echo          %%~zA bytes
echo ===============================================
echo.

REM Ask user if they want to run the demo
set /p RUNTEST="Run demonstration? (Y/N): "
if /i "%RUNTEST%"=="Y" (
    echo.
    echo Running agentic_tools.exe...
    echo.
    agentic_tools.exe
)

echo.
pause
