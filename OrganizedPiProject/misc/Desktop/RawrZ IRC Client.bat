@echo off
cd /d "C:\Users\Garre\AppData\Roaming\RawrZ\IRCClient"
echo.
echo ========================================
echo   RawrZ IRC Client - Free mIRC Alternative
echo ========================================
echo.
echo Choose your preferred interface:
echo 1. Console Client (Recommended - Fast & Lightweight)
echo 2. GUI Client (requires Electron installation)
echo.
set /p choice="Enter your choice (1 or 2, default=1): "
if "%choice%"=="2" (
    echo Starting GUI client...
    node gui.js
) else (
    echo Starting console client...
    node console-client.js
)
pause