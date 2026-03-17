@echo off
echo ========================================
echo RUNNING TEST BOT
echo ========================================
echo.
echo This will connect a test bot to your local C^&C server
echo running on 127.0.0.1:23
echo.
echo You should see it appear in the Control Panel!
echo Then you can click "Cure" to remove it safely.
echo.
echo ========================================
pause

cd build\windows
echo.
echo Starting test bot...
echo.
test_bot.exe

echo.
echo Bot has exited (this is normal after cure)
pause
