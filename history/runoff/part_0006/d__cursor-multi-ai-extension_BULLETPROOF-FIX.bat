@echo off
echo ===============================================
echo 🎯 BULLETPROOF BUTTON FIX
echo ===============================================

echo Step 1: Fixing package.json (removing fake chrome-devtools-mcp)...
cd /d "D:\cursor-multi-ai-extension\chrome-devtools-ide"

echo Step 2: Installing real packages...
npm install

if %ERRORLEVEL% NEQ 0 (
    echo [FALLBACK] npm install failed, trying individual packages...
    npm install express
    npm install ws  
    npm install puppeteer
    npm install cors
)

echo Step 3: Starting Chrome with remote debugging...
start "Chrome-Debug" chrome.exe --remote-debugging-port=9222 --disable-web-security --user-data-dir="%TEMP%\chrome-debug-buttons"

echo Step 4: Waiting for Chrome to initialize...
timeout /t 5 /nobreak

echo Step 5: Starting Node.js server...
echo const express = require('express'); > quick-server.js
echo const path = require('path'); >> quick-server.js
echo const app = express(); >> quick-server.js
echo app.use(express.static('public')); >> quick-server.js
echo app.listen(3001, () => console.log('Server running on http://localhost:3001')); >> quick-server.js

start "Quick-Server" cmd /k "node quick-server.js"

echo.
echo ✅ READY TO TEST!
echo.
echo Open: http://localhost:3001
echo.
echo If buttons STILL don't work:
echo 1. Press F12 in browser
echo 2. Click Console tab  
echo 3. Look for red error messages
echo 4. Tell me what errors you see
echo.
pause