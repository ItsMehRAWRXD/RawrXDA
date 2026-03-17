@echo off
REM ================================================================
REM Complete MASM Port Integration - Batch Wrapper
REM ================================================================
REM This script executes the PowerShell integration script and
REM performs complete setup of all MASM components
REM ================================================================

setlocal enabledelayedexpansion

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║   MASM Component Integration - Complete Setup              ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

REM Set project root
set PROJECT_ROOT=D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader
set SCRIPT_PATH=%PROJECT_ROOT%\complete_masm_integration.ps1

REM Verify script exists
if not exist "%SCRIPT_PATH%" (
    echo ERROR: Integration script not found at %SCRIPT_PATH%
    exit /b 1
)

REM Run PowerShell script with execution policy override
echo Running integration script...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_PATH%"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Integration script failed with exit code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║   Integration Complete - Ready for Final Deployment        ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

endlocal
exit /b 0
