@echo off
REM ============================================
REM Visual Studio 2022 Repair & Launch
REM Path: D:\Microsoft Visual Studio 2022\Enterprise
REM ============================================

echo.
echo 🔧 Visual Studio 2022 Repair & Launch Script
echo ============================================
echo.

REM Set VS installation path
set VS_PATH=D:\Microsoft Visual Studio 2022\Enterprise
set DEVENV_EXE=%VS_PATH%\Common7\IDE\devenv.exe

REM Check if VS exists
if not exist "%DEVENV_EXE%" (
    echo ❌ Visual Studio not found at: %VS_PATH%
    echo.
    echo Please check that D:\Microsoft Visual Studio 2022\Enterprise exists
    pause
    exit /b 1
)

echo ✅ Found Visual Studio at: %VS_PATH%
echo.

REM Option 1: Repair VS
echo 🔄 Repair Options:
echo 1. Quick Repair (recommended)
echo 2. Full Repair
echo 3. Skip Repair and Launch
echo.

set /p CHOICE="Select option (1-3): "

if "%CHOICE%"=="1" (
    echo.
    echo ⏳ Running Quick Repair...
    "%DEVENV_EXE%" /repair
    if errorlevel 1 (
        echo ⚠️ Repair had warnings, continuing anyway...
    ) else (
        echo ✅ Quick Repair completed
    )
)

if "%CHOICE%"=="2" (
    echo.
    echo ⏳ Running Full Repair (this may take 10-15 minutes)...
    "%DEVENV_EXE%" /repair
    if errorlevel 1 (
        echo ⚠️ Repair had issues, continuing anyway...
    ) else (
        echo ✅ Full Repair completed
    )
)

REM Clear VS cache
echo.
echo 🧹 Clearing Visual Studio cache...
setlocal enabledelayedexpansion

REM Clear component model cache
if exist "%LOCALAPPDATA%\Microsoft\VisualStudio" (
    echo Clearing VS cache in %LOCALAPPDATA%\Microsoft\VisualStudio
    del /s /q "%LOCALAPPDATA%\Microsoft\VisualStudio\17.0_*\ComponentModelCache\*" 2>nul
    echo ✅ Cache cleared
)

REM Reset MEF cache
if exist "%TEMP%\VisualStudioComponentCache" (
    echo Clearing MEF cache...
    rmdir /s /q "%TEMP%\VisualStudioComponentCache" 2>nul
    mkdir "%TEMP%\VisualStudioComponentCache" 2>nul
    echo ✅ MEF cache reset
)

REM Launch Visual Studio
echo.
echo 🚀 Launching Visual Studio 2022...
echo.

"%DEVENV_EXE%"

echo.
echo ✅ Visual Studio closed
pause
