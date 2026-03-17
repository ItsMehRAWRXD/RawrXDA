@echo off
echo ===============================================
echo 🚀 SIMPLE BUTTON FIX - No Dependencies 
echo ===============================================

echo [STEP 1] Going to Chrome DevTools IDE...
cd /d "D:\cursor-multi-ai-extension\chrome-devtools-ide"

echo [STEP 2] Installing ONLY the packages that exist...
call npm install express --save
call npm install ws --save  
call npm install puppeteer --save
call npm install cors --save

echo [STEP 3] Starting Chrome with debugging...
start "Chrome Debug" chrome --remote-debugging-port=9222 --disable-web-security --user-data-dir="%TEMP%\chrome-debug"

echo [STEP 4] Waiting for Chrome to start...
timeout /t 5 /nobreak >nul

echo [STEP 5] Starting Chrome DevTools IDE server...
start "DevTools Server" cmd /k "echo Starting Chrome DevTools IDE Server && node server.js"

echo [STEP 6] Waiting for server...
timeout /t 3 /nobreak >nul

echo ✅ FIXES COMPLETE!
echo.
echo Test your buttons at: http://localhost:3001
echo Chrome DevTools: http://localhost:9222
echo.
echo If buttons still don't work, press Ctrl+Shift+I in browser
echo and check Console tab for JavaScript errors.
echo.
pause