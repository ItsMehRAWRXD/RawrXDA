@echo off
REM RawrXD Win32 IDE Launcher
REM Version 1.0.0

setlocal enabledelayedexpansion

echo ====================================
echo  RawrXD Win32 IDE
echo  Version 1.0.0
echo ====================================
echo.

REM Set deployment root
set DEPLOY_ROOT=%~dp0

REM Check for binary
if not exist "%DEPLOY_ROOT%bin\AgenticIDEWin.exe" (
    echo [ERROR] AgenticIDEWin.exe not found in bin directory
    pause
    exit /b 1
)

REM Check for config
if not exist "%APPDATA%\RawrXD\config.json" (
    echo [INFO] First run detected - creating config directory...
    mkdir "%APPDATA%\RawrXD" 2>nul
    if exist "%DEPLOY_ROOT%config\config.json" (
        copy "%DEPLOY_ROOT%config\config.json" "%APPDATA%\RawrXD\config.json" >nul
        echo [OK] Configuration file created
    )
)

REM Check for logs directory
if not exist "%LOCALAPPDATA%\RawrXD\logs" (
    echo [INFO] Creating logs directory...
    mkdir "%LOCALAPPDATA%\RawrXD\logs" 2>nul
)

REM Check for required environment variables
if "%OPENAI_API_KEY%"=="" (
    echo.
    echo [WARNING] OPENAI_API_KEY environment variable not set
    echo AI features will not be available
    echo To enable AI features, set the environment variable:
    echo   setx OPENAI_API_KEY "your-api-key-here"
    echo.
)

REM Check for Node.js
where node >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [WARNING] Node.js not found in PATH
    echo AI orchestration features may not work
    echo Please install Node.js from: https://nodejs.org/
    echo.
)

REM Launch IDE
echo [INFO] Launching RawrXD Win32 IDE...
echo.
start "" "%DEPLOY_ROOT%bin\AgenticIDEWin.exe" %*

REM Wait a moment to check if it started
timeout /t 2 /nobreak >nul

tasklist /FI "IMAGENAME eq AgenticIDEWin.exe" 2>nul | find /I /N "AgenticIDEWin.exe" >nul
if %ERRORLEVEL%==0 (
    echo [OK] IDE launched successfully
) else (
    echo [ERROR] IDE failed to start
    echo Check logs at: %LOCALAPPDATA%\RawrXD\logs\
    pause
)

exit /b 0
