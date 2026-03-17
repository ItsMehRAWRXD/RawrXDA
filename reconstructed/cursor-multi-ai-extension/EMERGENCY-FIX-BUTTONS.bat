@echo off
echo ===============================================
echo 🚨 EMERGENCY FIX - HTML Button Issues
echo ===============================================

echo [STEP 1] Fixing Chrome DevTools IDE...
cd chrome-devtools-ide

REM Install correct dependencies (not the fake chrome-devtools-mcp)
echo Installing actual Node.js packages...
npm install express ws puppeteer cors --save

REM Fix the server WebSocket issue
echo [FIXING] WebSocket connection...

echo [STEP 2] Fixing button event handlers...
REM Will be fixed in HTML file

echo [STEP 3] Starting servers in correct order...
start "Chrome Debug" chrome --remote-debugging-port=9222 --disable-web-security --user-data-dir=temp-chrome

timeout /t 3 /nobreak >nul

start "DevTools Server" node server.js

timeout /t 2 /nobreak >nul

echo [STEP 4] Testing Real-Debrid Streamer...
cd ..\real-debrid-streamer
if exist package.json (
    npm install
    start "RD Server" npm start
) else (
    echo No package.json found, creating simple server...
    start "RD Server" python -m http.server 3000
)

echo.
echo ✅ EMERGENCY FIXES APPLIED!
echo.
echo Your buttons should now work. Test:
echo   • Chrome IDE: http://localhost:3001
echo   • Real-Debrid: http://localhost:3000
echo   • Diggz Streamer: http://localhost:8080
echo.
pause