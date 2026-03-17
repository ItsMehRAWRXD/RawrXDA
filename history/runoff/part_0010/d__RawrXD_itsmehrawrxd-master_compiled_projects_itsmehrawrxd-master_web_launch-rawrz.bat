@echo off
:: RawrZ Ultimate Security Platform Launcher
:: Professional batch launcher for Windows systems

title RawrZ Ultimate Security Platform v3.0
color 0A

echo.
echo ████████████████████████████████████████████████████████████████████
echo █    ____                    ______   _    _ _ _   _                 █
echo █   ^|  _ \ __ ___      ___ ___^|__  /  ^| ^|  ^| ^| ^| ^| ^|_   ___ ___     █
echo █   ^| ^|_) / _` \ \ /\ / / '_  ^|/ /   ^| ^|  ^| ^| ^| ^| __^| ^| ^| ^| ^|_^|   █
echo █   ^|  _ ^< (_^| ^|\ V  V /^| ^|   / /_   ^| ^|_^| ^| ^|_^| ^|__^| ^| ^| ^|  _    █
echo █   ^|_^| \_\__,_^| \_/\_/ ^|_^|  __/_^|   \___/^| \___^|___^| ^|_^| ^|_^|    █
echo █                                                                  █
echo █              Ultimate Security Platform v3.0                    █
echo █                Professional Launcher                            █
echo ████████████████████████████████████████████████████████████████████
echo.

:: Check if Node.js is installed
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ Node.js is not installed or not in PATH
    echo.
    echo Please install Node.js from https://nodejs.org/
    echo.
    pause
    exit /b 1
)

echo ✅ Node.js detected
node --version

:: Check if files exist
if not exist "rawrz-ultimate.js" (
    echo ❌ rawrz-ultimate.js not found in current directory
    pause
    exit /b 1
)

if not exist "rawrz-console.js" (
    echo ❌ rawrz-console.js not found in current directory
    pause
    exit /b 1
)

echo ✅ RawrZ files detected
echo.

:MENU
cls
echo.
echo ████████████████████████████████████████████████████████████████████
echo █                      RawrZ Ultimate v3.0                        █
echo █                     Select Launch Method                        █
echo ████████████████████████████████████████████████████████████████████
echo.
echo 🎯 LAUNCH OPTIONS:
echo.
echo   1. 🔥 Interactive Console (Professional GUI)
echo   2. 📋 Command Line Help
echo   3. ⚡ Quick Encrypt Only
echo   4. 🏗️  Quick Generate Stubs  
echo   5. 🪟 Quick Generate Formats
echo   6. 📊 Quick Payload Matrix
echo   7. 📚 Show Documentation
echo   8. 🔍 System Information
echo   9. ⚙️  Advanced CLI Mode
echo   0. ❌ Exit
echo.

set /p choice="Select option (0-9): "

if "%choice%"=="1" goto INTERACTIVE
if "%choice%"=="2" goto HELP
if "%choice%"=="3" goto QUICK_ENCRYPT
if "%choice%"=="4" goto QUICK_STUBS
if "%choice%"=="5" goto QUICK_FORMATS
if "%choice%"=="6" goto QUICK_MATRIX
if "%choice%"=="7" goto DOCUMENTATION
if "%choice%"=="8" goto SYSINFO
if "%choice%"=="9" goto ADVANCED
if "%choice%"=="0" goto EXIT

echo ❌ Invalid option. Please try again.
timeout /t 2 >nul
goto MENU

:INTERACTIVE
echo.
echo 🔥 Starting Interactive Console...
echo.
node rawrz-console.js
goto MENU

:HELP
echo.
echo 📋 RawrZ Ultimate Help System
echo.
node rawrz-ultimate.js help
echo.
pause
goto MENU

:QUICK_ENCRYPT
echo.
echo ⚡ Quick Encrypt Only
echo.
set /p inputfile="Enter input file path: "
if "%inputfile%"=="" goto MENU

set /p method="Enter encryption method (aes256/chacha20/camellia/hybrid): "
if "%method%"=="" set method=aes256

set /p output="Enter output name (default: encrypted_payload): "
if "%output%"=="" set output=encrypted_payload

echo.
echo 🔄 Encrypting with %method%...
node rawrz-ultimate.js encrypt-only %method% "%inputfile%" %output%.bin

echo.
echo ✅ Encryption completed!
echo 📁 Check current directory for output files
pause
goto MENU

:QUICK_STUBS
echo.
echo 🏗️ Quick Generate Stubs
echo.
set /p encfile="Enter encrypted file path: "
if "%encfile%"=="" goto MENU

set /p languages="Enter languages (comma-separated or 'all'): "
if "%languages%"=="" set languages=cpp,python,csharp

set /p stubdir="Enter output directory (default: stubs): "
if "%stubdir%"=="" set stubdir=stubs

echo.
echo 🔄 Generating stubs for: %languages%
node rawrz-ultimate.js generate-stubs "%encfile%" %languages% ./%stubdir%/

echo.
echo ✅ Stubs generated!
echo 📁 Check ./%stubdir%/ directory
pause
goto MENU

:QUICK_FORMATS
echo.
echo 🪟 Quick Generate Formats
echo.
set /p encfile="Enter encrypted file path: "
if "%encfile%"=="" goto MENU

set /p formats="Enter formats (comma-separated or 'all'): "
if "%formats%"=="" set formats=exe,dll,ps1

set /p formatdir="Enter output directory (default: formats): "
if "%formatdir%"=="" set formatdir=formats

echo.
echo 🔄 Generating formats: %formats%
node rawrz-ultimate.js generate-formats "%encfile%" %formats% ./%formatdir%/

echo.
echo ✅ Formats generated!
echo 📁 Check ./%formatdir%/ directory
pause
goto MENU

:QUICK_MATRIX
echo.
echo 📊 Quick Payload Matrix
echo.
echo ⚠️  WARNING: This can generate hundreds of files!
echo.
set /p confirm="Continue? (y/n): "
if not "%confirm%"=="y" goto MENU

set /p encfile="Enter encrypted file path: "
if "%encfile%"=="" goto MENU

set /p languages="Enter languages ('all' or specific): "
if "%languages%"=="" set languages=cpp,python,csharp

set /p formats="Enter formats ('all' or specific): "
if "%formats%"=="" set formats=exe,dll,ps1

set /p matrixdir="Enter output directory (default: matrix): "
if "%matrixdir%"=="" set matrixdir=matrix

echo.
echo 🔄 Generating payload matrix...
echo   Languages: %languages%
echo   Formats: %formats%
node rawrz-ultimate.js payload-matrix "%encfile%" %languages% %formats% ./%matrixdir%/

echo.
echo ✅ Matrix generated!
echo 📁 Check ./%matrixdir%/ directory
pause
goto MENU

:DOCUMENTATION
echo.
echo 📚 Opening Documentation...
echo.
if exist "README.md" (
    type README.md | more
) else (
    echo ❌ README.md not found
)
echo.
pause
goto MENU

:SYSINFO
echo.
echo 🔍 System Information
echo ═══════════════════════════════════════════════════════
echo.
echo 💻 System: %COMPUTERNAME%
echo 👤 User: %USERNAME%
echo 📁 Current Directory: %CD%
echo 📅 Date: %DATE%
echo ⏰ Time: %TIME%
echo.
echo 🔧 Node.js Version:
node --version
echo.
echo 📊 RawrZ Platform Status:
echo   ✅ rawrz-ultimate.js: Available
echo   ✅ rawrz-console.js: Available  
echo   ✅ README.md: Available
echo.
echo 🎯 Platform Capabilities:
node rawrz-ultimate.js list-languages
echo.
node rawrz-ultimate.js list-formats
echo.
node rawrz-ultimate.js list-encryption
echo.
pause
goto MENU

:ADVANCED
echo.
echo ⚙️ Advanced CLI Mode
echo ═══════════════════════════════════════════════════════
echo.
echo You are now in advanced command line mode.
echo Type 'help' for command reference or 'exit' to return to menu.
echo.

:ADVANCED_LOOP
set /p cmd="RawrZ> "

if "%cmd%"=="exit" goto MENU
if "%cmd%"=="help" (
    node rawrz-ultimate.js help
    goto ADVANCED_LOOP
)
if "%cmd%"=="" goto ADVANCED_LOOP

:: Execute the command
echo.
node rawrz-ultimate.js %cmd%
echo.
goto ADVANCED_LOOP

:EXIT
echo.
echo 🔥 Thank you for using RawrZ Ultimate Security Platform! 🔥
echo.
echo 🛡️ Stay secure and use responsibly!
echo.
timeout /t 3 >nul
exit /b 0