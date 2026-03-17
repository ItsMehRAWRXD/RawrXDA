@echo off
echo Starting Sovereign NVMe Oracle Service...
sc.exe start SovereignNVMeOracle
if %ERRORLEVEL% NEQ 0 (
    echo Failed to start service. Error: %ERRORLEVEL%
    echo Checking service status...
    sc.exe query SovereignNVMeOracle
    pause
    exit /b %ERRORLEVEL%
)
echo Service start command sent.
timeout /t 3 /nobreak >nul
echo.
echo Checking service status...
sc.exe query SovereignNVMeOracle
pause
