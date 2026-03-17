@echo off
echo ========================================
echo  Quick Setup: rawrz-http-encryptor
echo ========================================
echo.

cd /d "D:\Security Research aka GitHub Repos\rawrz-http-encryptor\ItsMehRAWRXD-rawrz-http-encryptor-47724c6"

if not exist package.json (
    echo ERROR: package.json not found!
    exit /b 1
)

echo Installing npm dependencies...
call npm install

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Server ready to run.
    echo.
    echo To start: npm start
    echo Control Panel: http://localhost:8080/panel
) else (
    echo.
    echo FAILED! Check errors above.
    exit /b 1
)
