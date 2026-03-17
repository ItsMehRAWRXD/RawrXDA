@echo off
title RawrZ Security Platform Professional v2.0.0
color 0A

echo.
echo ================================================================
echo  RawrZ Security Platform - Professional Edition
echo  67-Engine Security Suite - Console Launcher
echo ================================================================
echo.
echo [INIT] Starting RawrZ Professional Console...
echo [INFO] Build: Professional Standalone Edition
echo [INFO] Engines: 67 security and compilation engines loaded
echo.

cd /d "%~dp0"

if not exist "node.exe" (
    echo [WARNING] node.exe not found in current directory
    echo [INFO] Using system Node.js installation
    node rawrz-console.js
) else (
    echo [INFO] Using portable Node.js
    node.exe rawrz-console.js
)

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] RawrZ Platform encountered an error
    echo [INFO] Check Node.js installation and try again
    pause
)

echo.
echo [EXIT] RawrZ Platform terminated
pause