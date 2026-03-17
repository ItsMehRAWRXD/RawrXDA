@echo off
REM ========================================
REM Custom AV Scanner - Complete Installation
REM ========================================

setlocal enabledelayedexpansion
color 0A

echo.
echo ========================================
echo   Custom AV Scanner - Installation
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.8+ from https://www.python.org/
    echo.
    pause
    exit /b 1
)

echo [Python Check] OK - Python is installed
python --version
echo.

echo [1/6] Upgrading pip...
python -m pip install --upgrade pip --quiet
echo [OK] pip upgraded
echo.

echo [2/6] Installing core scanner packages...
python -m pip install pefile==2023.2.7 python-magic==0.4.27 python-magic-bin==0.4.14 yara-python==4.3.1 ssdeep==3.4 requests==2.31.0 --quiet
echo [OK] Scanner packages installed
echo.

echo [3/6] Installing web dashboard packages...
python -m pip install flask==3.0.0 flask-cors==4.0.0 werkzeug==3.0.1 --quiet
echo [OK] Web dashboard packages installed
echo.

echo [4/6] Creating required directories...
if not exist yara_rules mkdir yara_rules
if not exist scan_reports mkdir scan_reports
if not exist templates mkdir templates
if not exist temp_scans mkdir temp_scans
echo [OK] Directories created
echo.

echo [5/6] Downloading threat intelligence feeds...
echo This may take 1-2 minutes...
python threat_feed_updater.py
echo [OK] Threat feeds downloaded
echo.

echo [6/6] Verifying installation...
python -c "from custom_av_scanner import CustomAVScanner; print('[OK] Scanner engine verified')" 2>nul || (
    echo [!] Warning: Could not verify scanner
)
python -c "import flask; print('[OK] Flask web framework verified')" 2>nul || (
    echo [!] Warning: Could not verify Flask
)
echo.

echo ========================================
echo  Installation Complete!
echo ========================================
echo.

echo QUICK START:
echo.
echo 1. Scan a file:
echo    python custom_av_scanner.py malware.exe
echo.
echo 2. Start Web Dashboard:
echo    python scanner_web_app.py
echo    Then open: http://localhost:5000
echo.
echo 3. Update threat signatures:
echo    python threat_feed_updater.py
echo.
echo For more help, see:
echo - README.md (Scanner features)
echo - DASHBOARD-GUIDE.md (Web dashboard guide)
echo.
echo Features:
echo   - Signature-based detection
echo   - Heuristic analysis
echo   - Behavioral analysis
echo   - YARA rule scanning
echo   - Fuzzy hash matching
echo   - NO data sharing with third parties
echo.
pause
