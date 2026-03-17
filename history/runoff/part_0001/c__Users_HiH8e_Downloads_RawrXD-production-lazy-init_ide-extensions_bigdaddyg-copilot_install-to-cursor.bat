@echo off
setlocal enabledelayedexpansion

set SOURCE=C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\ide-extensions\bigdaddyg-copilot
set DEST=%APPDATA%\Cursor\extensions\bigdaddyg-copilot-1.0.0

echo.
echo Installing BigDaddyG Extension to Cursor...
echo Source: %SOURCE%
echo Destination: %DEST%
echo.

REM Check if source exists
if not exist "%SOURCE%" (
    echo ERROR: Source not found!
    pause
    exit /b 1
)

REM Create destination directory if needed
if not exist "%APPDATA%\Cursor\extensions" (
    mkdir "%APPDATA%\Cursor\extensions"
)

REM Remove old version
if exist "%DEST%" (
    echo Removing old version...
    rmdir /s /q "%DEST%" >nul 2>&1
    timeout /t 1 /nobreak >nul
)

REM Copy extension
echo Copying files...
xcopy "%SOURCE%" "%DEST%" /E /I /Y /Q >nul 2>&1

if exist "%DEST%\out\extension.js" (
    echo.
    echo SUCCESS! Extension installed to:
    echo %DEST%
    echo.
    echo NEXT STEP: Close and restart Cursor completely
    echo.
) else (
    echo ERROR: Installation failed!
    exit /b 1
)

pause
