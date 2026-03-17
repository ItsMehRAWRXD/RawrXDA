@echo off
echo ========================================
echo BigDaddyG Assembly IDE - Quick Install
echo ========================================
echo.

cd /d "%~dp0"

echo [1/3] Installing dependencies...
call npm install
if errorlevel 1 (
    echo ERROR: npm install failed
    pause
    exit /b 1
)

echo.
echo [2/3] Compiling extension...
call npm run compile
if errorlevel 1 (
    echo ERROR: compile failed
    pause
    exit /b 1
)

echo.
echo [3/3] Extension compiled successfully!
echo.
echo ========================================
echo SUCCESS! BigDaddyG Assembly IDE is ready!
echo ========================================
echo.
echo To use the extension:
echo.
echo METHOD 1 - Development Mode (Recommended for testing):
echo   1. Open this folder in VS Code: code .
echo   2. Press F5 to launch Extension Development Host
echo   3. Open any .asm file in the new window
echo   4. Press Ctrl+Shift+M to launch multi-agent!
echo.
echo METHOD 2 - Install as VSIX:
echo   1. Run: npm install -g @vscode/vsce
echo   2. Run: vsce package
echo   3. Install the .vsix file in VS Code
echo.
echo Make sure BigDaddyG server is running on port 11441!
echo   cd d:\MyCopilot-IDE\servers
echo   node bigdaddyg-model-server.js
echo.
pause
