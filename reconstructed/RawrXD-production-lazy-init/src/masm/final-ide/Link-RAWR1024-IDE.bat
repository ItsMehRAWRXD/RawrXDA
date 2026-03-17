@echo off
REM Link-RAWR1024-IDE.bat - Batch wrapper for PowerShell linking script
REM Run from: D:\RawrXD-production-lazy-init\src\masm\final-ide\

setlocal enabledelayedexpansion
cd /d "%~dp0"

echo.
echo ====================================================================
echo  RawrXD IDE Linking Script
echo ====================================================================
echo.

REM Check if PowerShell is available
where powershell >nul 2>&1
if errorlevel 1 (
    echo ERROR: PowerShell is required but not found in PATH
    exit /b 1
)

REM Display usage if requested
if "%1"=="help" (
    echo Usage: Link-RAWR1024-IDE.bat [action]
    echo.
    echo Actions:
    echo   (none)    - Full link and verify
    echo   verify    - Check object files only
    echo   link      - Link objects to executable
    echo   clean     - Remove executable
    echo   help      - Show this help message
    echo.
    exit /b 0
)

REM Default action is 'full'
if "%1"=="" (
    set ACTION=full
) else (
    set ACTION=%1
)

echo Action: !ACTION!
echo.

REM Execute PowerShell script
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "& '.\Link-RAWR1024-IDE.ps1' -Action !ACTION! -Verbose"

if errorlevel 1 (
    echo.
    echo Linking failed - see errors above
    exit /b 1
) else (
    echo.
    echo ====================================================================
    echo  ✓ Linking completed successfully
    echo ====================================================================
    exit /b 0
)
