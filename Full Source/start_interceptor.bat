@echo off
REM OS Explorer Interceptor - Quick Start

echo Starting OS Explorer Interceptor CLI...
start "" "%CD%\bin\os_interceptor_cli.exe"

echo.
echo.
echo To use with PowerShell:
echo   Import-Module "%CD%\modules\OSExplorerInterceptor.psm1"
echo   Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming

echo.
pause
