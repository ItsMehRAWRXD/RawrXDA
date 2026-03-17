@echo off
echo ============================================================
echo Starting Multi-AV Scanner API Server
echo ============================================================
echo.

cd /d "%~dp0"

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please run INSTALL-SCANNER.bat first
    pause
    exit /b 1
)

echo [*] Starting API server on http://localhost:5000
echo.
echo Available endpoints:
echo   GET  /api/v1/get/avlist          - Get available AV engines
echo   POST /api/v1/token/generate      - Generate API token
echo   POST /api/v1/file/scan           - Scan file
echo   GET  /api/v1/file/result/{token} - Get scan results
echo   GET  /api/v1/file/status/{token} - Get scan status
echo   GET  /api/v1/stats                - Get statistics
echo   GET  /api/v1/health               - Health check
echo.
echo Web Interface: Open scanner_web.html in your browser
echo.
echo Press Ctrl+C to stop the server
echo ============================================================
echo.

python scanner_api.py

pause
