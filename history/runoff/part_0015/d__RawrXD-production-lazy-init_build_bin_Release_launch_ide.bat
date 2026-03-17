@echo off
REM RawrXD Qt IDE Launcher with proper environment setup
cd /d "%~dp0"

REM Set Qt plugin paths
set QT_PLUGIN_PATH=%cd%
set QT_QPA_PLATFORM_PLUGIN_PATH=%cd%\platforms
set QT_DEBUG_PLUGINS=0

REM Disable problematic features during startup
set RAWRXD_SAFE_MODE=1
set RAWRXD_DISABLE_AUTO_LOAD=1

echo Starting RawrXD Qt IDE...
echo Working Directory: %cd%
echo Qt Plugin Path: %QT_PLUGIN_PATH%
echo.

REM Launch with error handling
start "" "RawrXD-QtShell.exe"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Failed to launch IDE (Exit Code: %ERRORLEVEL%)
    pause
)
