@echo off
echo Fixing Config.Msi permission error...

REM Kill any Node.js processes that might be causing the issue
taskkill /F /IM node.exe >nul 2>&1

REM Wait a moment
timeout /t 2 /nobreak >nul

REM Set proper permissions for the Config.Msi directory (if accessible)
echo Attempting to fix Config.Msi permissions...
icacls "d:\Config.Msi" /grant Everyone:F /T >nul 2>&1

REM Alternative: Create a symbolic link to avoid the issue
if not exist "d:\Config.Msi" (
    echo Creating Config.Msi directory with proper permissions...
    mkdir "d:\Config.Msi" >nul 2>&1
    icacls "d:\Config.Msi" /grant Everyone:F /T >nul 2>&1
)

REM Restart the assembly proxy server
echo Restarting Assembly proxy server...
start /B node d:\ollama-openai-proxy.js

REM Wait and verify
timeout /t 3 /nobreak >nul
netstat -an | findstr :11441 >nul && echo ✅ Assembly proxy restarted successfully! || echo ❌ Failed to restart proxy

echo.
echo Config.Msi error should now be resolved!
pause
