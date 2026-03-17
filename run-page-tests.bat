@echo off
REM RawrXD Page Testing Suite
REM Comprehensive testing for all HTML pages and features

echo.
echo     ████████  ██████  ██     ██ ██████  ██   ██ ██████  
echo     ██    ██ ██    ██ ██     ██ ██   ██  ██ ██  ██   ██ 
echo     ████████  ██████  ██  █  ██ ██████    ███   ██   ██ 
echo     ██   ██  ██   ██  ██ ███ ██ ██   ██  ██ ██  ██   ██ 
echo     ██    ██ ██    ██  ███ ███  ██    ██ ██   ██ ██████  
echo.
echo   ╔══════════════════════════════════════════════════════╗
echo   ║           RawrXD Page Testing Suite v1.0            ║
echo   ║         Comprehensive HTML Testing Framework        ║
echo   ╚══════════════════════════════════════════════════════╝
echo.

REM Check if Node.js is installed
where node >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ Error: Node.js is not installed or not in PATH
    echo Please install Node.js from https://nodejs.org/
    pause
    exit /b 1
)

REM Check if dependencies are installed
if not exist "node_modules\puppeteer" (
    echo 📦 Installing testing dependencies...
    echo This may take a few minutes on first run...
    npm install
    if %errorlevel% neq 0 (
        echo ❌ Failed to install dependencies
        pause
        exit /b 1
    )
)

:MENU
cls
echo.
echo   ╔══════════════════════════════════════════════════════╗
echo   ║                  TESTING OPTIONS                    ║
echo   ╚══════════════════════════════════════════════════════╝
echo.
echo   [1] 🚀 Quick Test (Headless, Fast)
echo   [2] 🌐 Webroot Test (HTTP Server)
echo   [3] 🔍 Filter Test (RawrZ only)
echo   [4] ⚡ Parallel Test (5 workers)
echo   [5] 👀 Visual Test (Show browser)
echo   [6] 📊 Full Test Suite (All options)
echo   [7] 🛠️  Start Webroot Server Only
echo   [8] ℹ️  Show Test Statistics
echo   [9] ❌ Exit
echo.
set /p choice="Select option (1-9): "

if "%choice%"=="1" goto QUICK_TEST
if "%choice%"=="2" goto WEBROOT_TEST  
if "%choice%"=="3" goto FILTER_TEST
if "%choice%"=="4" goto PARALLEL_TEST
if "%choice%"=="5" goto VISUAL_TEST
if "%choice%"=="6" goto FULL_TEST
if "%choice%"=="7" goto START_WEBROOT
if "%choice%"=="8" goto SHOW_STATS
if "%choice%"=="9" goto EXIT

echo Invalid choice. Please select 1-9.
timeout /t 2 >nul
goto MENU

:QUICK_TEST
echo.
echo 🚀 Running Quick Test (Headless + Fast)...
node comprehensive-page-tester.js --headless true --timeout 15000 --parallel 3
goto RESULTS

:WEBROOT_TEST
echo.
echo 🌐 Running Webroot Test (HTTP Server)...
node comprehensive-page-tester.js --webroot true --headless true
goto RESULTS

:FILTER_TEST
echo.
echo 🔍 Running Filter Test (RawrZ pages only)...
node comprehensive-page-tester.js --filter rawrz --headless true
goto RESULTS

:PARALLEL_TEST
echo.
echo ⚡ Running Parallel Test (5 workers)...
node comprehensive-page-tester.js --parallel 5 --headless true
goto RESULTS

:VISUAL_TEST
echo.
echo 👀 Running Visual Test (Browser visible)...
echo Note: Browser windows will open during testing
node comprehensive-page-tester.js --headless false --parallel 1
goto RESULTS

:FULL_TEST
echo.
echo 📊 Running Full Test Suite...
echo This will test all features comprehensively
node comprehensive-page-tester.js --webroot true --parallel 3 --verbose true
goto RESULTS

:START_WEBROOT
echo.
echo 🛠️  Starting Webroot Server...
echo Server will run at http://localhost:8080
echo Press Ctrl+C to stop the server
node webroot-server.js
goto MENU

:SHOW_STATS
echo.
echo 📈 Test Statistics:
if exist "test-report.html" (
    echo ✅ Last report generated: test-report.html
    for %%A in (test-report.html) do echo    📅 Modified: %%~tA
    for %%A in (test-report.html) do echo    💾 Size: %%~zA bytes
) else (
    echo ⚠️  No test reports found
)
echo.

REM Count HTML files
set /a html_count=0
for /r %%f in (*.html) do set /a html_count+=1
echo 📄 HTML files discovered: %html_count%

REM Show available test commands
echo.
echo 📋 Available NPM Scripts:
echo    npm run test:pages       - Basic page testing
echo    npm run test:headless    - Headless browser testing
echo    npm run test:webroot     - HTTP server testing
echo    npm run webroot:start    - Start webroot server
echo.
pause
goto MENU

:RESULTS
echo.
echo ✨ Testing completed!
if exist "test-report.html" (
    echo 📊 Detailed report generated: test-report.html
    set /p open="Open report in browser? (y/N): "
    if /i "%open%"=="y" start test-report.html
)
echo.
pause
goto MENU

:EXIT
echo.
echo 👋 Thanks for using RawrXD Page Testing Suite!
echo For advanced options, check package.json scripts or use:
echo    node comprehensive-page-tester.js --help
echo.
timeout /t 3 >nul
exit /b 0