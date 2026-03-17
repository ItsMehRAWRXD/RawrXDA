@echo off
echo Stopping and removing old service...
sc.exe stop SovereignNVMeOracle
timeout /t 2 /nobreak >nul
sc.exe delete SovereignNVMeOracle
timeout /t 1 /nobreak >nul

echo Installing Sovereign NVMe Oracle Service...
sc.exe create SovereignNVMeOracle binPath= "D:\rawrxd\build\nvme_oracle_service.exe" start= auto obj= LocalSystem
if %ERRORLEVEL% NEQ 0 (
    echo Failed to create service. Error: %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo Service created successfully.

echo Starting service...
sc.exe start SovereignNVMeOracle
if %ERRORLEVEL% NEQ 0 (
    echo Failed to start service. Error: %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)
echo Service started successfully.
echo.
echo Checking service status...
sc.exe query SovereignNVMeOracle
pause
