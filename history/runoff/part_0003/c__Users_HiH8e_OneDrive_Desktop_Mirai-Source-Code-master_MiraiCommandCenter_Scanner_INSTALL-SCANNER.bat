@echo off
echo ============================================================
echo Multi-AV Scanner - Installation and Setup
echo ============================================================
echo.

REM Check Python installation
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.8+ from https://www.python.org/
    pause
    exit /b 1
)

echo [*] Python found
echo.

REM Install requirements
echo [*] Installing required packages...
python -m pip install --upgrade pip
python -m pip install -r requirements.txt

if errorlevel 1 (
    echo ERROR: Failed to install requirements
    pause
    exit /b 1
)

echo.
echo ============================================================
echo Installation Complete!
echo ============================================================
echo.
echo Next Steps:
echo 1. Run START-SCANNER-API.bat to start the API server
echo 2. Open scanner_web.html in your browser
echo 3. Generate an API token
echo 4. Start scanning files!
echo.
echo Available Components:
echo - scanner_api.py       : REST API server
echo - multi_av_scanner.py  : Core scanning engine
echo - av_engines.py        : Additional AV integrations
echo - scanner_client.py    : Python client library
echo - scanner_web.html     : Web interface
echo.
pause
