@echo off
REM RawrXD Toolchain PATH Installer
REM Adds C:\RawrXD to user PATH for easy CLI access

echo.
echo ========================================================
echo   RawrXD Toolchain - PATH Installation
echo ========================================================
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo [Admin] Installing to SYSTEM PATH...
    setx PATH "%PATH%;C:\RawrXD" /M
    echo.
    echo SUCCESS: Added C:\RawrXD to SYSTEM PATH
    echo All users can now access RawrXD-CLI from anywhere
) else (
    echo [User] Installing to USER PATH...
    setx PATH "%PATH%;C:\RawrXD"
    echo.
    echo SUCCESS: Added C:\RawrXD to USER PATH
    echo You can now access RawrXD-CLI from anywhere
)

echo.
echo Test the installation with:
echo   RawrXD-CLI.ps1 info
echo   RawrXD-CLI.bat info
echo.
echo NOTE: Close and reopen your terminal for changes to take effect
echo.

pause
