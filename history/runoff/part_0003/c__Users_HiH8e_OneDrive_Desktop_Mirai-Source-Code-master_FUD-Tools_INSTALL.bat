@echo off
echo ============================================================
echo FUD Tools Suite - Installation
echo ============================================================
echo.

REM Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo [!] ERROR: Python not found
    echo [*] Install Python 3.8+ from https://www.python.org/
    pause
    exit /b 1
)

echo [*] Python found
echo.

REM Install requirements
echo [*] Installing Python packages...
python -m pip install --upgrade pip
python -m pip install -r requirements.txt

if errorlevel 1 (
    echo [!] ERROR: Failed to install requirements
    pause
    exit /b 1
)

echo.
echo [*] Creating output directories...
mkdir output 2>nul
mkdir output\loaders 2>nul
mkdir output\launchers 2>nul
mkdir output\crypted 2>nul
mkdir output\spoofed 2>nul
mkdir panel_uploads 2>nul
mkdir panel_output 2>nul

echo.
echo ============================================================
echo Installation Complete!
echo ============================================================
echo.
echo Available Tools:
echo   1. FUD Loader        - python fud_loader.py
echo   2. FUD Launcher      - python fud_launcher.py
echo   3. FUD Crypter       - python fud_crypter.py
echo   4. Registry Spoofer  - python reg_spoofer.py
echo   5. Auto-Crypt Panel  - python crypt_panel.py
echo   6. Cloaking Tracker  - python cloaking_tracker.py
echo.
echo Optional Tools:
echo   - MinGW-w64 (for C++ compilation)
echo   - WiX Toolset (for MSI generation)
echo   - Windows SDK (for MSIX signing)
echo.
echo Quick Start:
echo   RUN-MENU.bat - Launch interactive menu
echo.
pause
