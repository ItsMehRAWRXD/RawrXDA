@echo off
echo ========================================
echo MyCoPilot++ IDE - Complete PKG Setup Script
echo ========================================
echo.
echo This script will:
echo 1. Install pkg dependency
echo 2. Package your application into a standalone executable
echo.
echo Note: This requires Administrator privileges for D:\ directory
echo.

pause

echo.
echo ========================================
echo STEP 1: Installing pkg dependency...
echo ========================================
echo.

npm install pkg@^5.8.1 --save-dev

if errorlevel 1 (
    echo.
    echo ❌ ERROR: Failed to install pkg dependency.
    echo.
    echo Please try one of these solutions:
    echo 1. Run Command Prompt as Administrator
    echo 2. Move your project to a user directory (not D:\ root)
    echo 3. Check if npm is properly installed
    echo.
    pause
    exit /b 1
)

echo.
echo ✅ SUCCESS: PKG dependency installed.
echo.

echo ========================================
echo STEP 2: Packaging application...
echo ========================================
echo.

npm run package

if errorlevel 1 (
    echo.
    echo ❌ ERROR: Failed to package application.
    echo.
    echo Please check:
    echo 1. All required files are present
    echo 2. package.json pkg configuration is correct
    echo 3. Assets are properly declared in pkg.assets
    echo.
    echo See PKG-README.md for detailed troubleshooting.
    echo.
    pause
    exit /b 1
)

echo.
echo ✅ SUCCESS! Application packaged successfully.
echo.
echo ========================================
echo DISTRIBUTION FILES CREATED:
echo ========================================
echo.

dir dist\*.exe 2>nul || echo No executable files found in dist folder

echo.
echo Your standalone executable is ready:
echo 📦 Location: dist/mycopilot-ide-win-x64.exe
echo 🚀 Run it directly without Node.js!
echo.

echo.
echo ========================================
echo SETUP COMPLETE!
echo ========================================
echo.
echo You can now distribute your MyCoPilot++ IDE as a standalone application.
echo.

pause
