@echo off
echo.
echo ========================================
echo   Mirai C^&C Server - Quick Start
echo ========================================
echo.

cd /d "%~dp0Server"

echo [1/3] Restoring packages...
dotnet restore
if %errorlevel% neq 0 (
    echo ERROR: Failed to restore packages
    pause
    exit /b 1
)

echo [2/3] Building server...
dotnet build -c Release
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo [3/3] Starting C^&C server...
echo.
echo ========================================
echo   Server will start on:
echo   - Port 23  : Bot connections
echo   - Port 101 : Admin interface  
echo   - Port 8080: API for GUI
echo ========================================
echo.

dotnet run -c Release

pause
