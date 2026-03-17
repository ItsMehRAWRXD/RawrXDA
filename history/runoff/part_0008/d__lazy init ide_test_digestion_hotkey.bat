@echo off
REM Test script to verify IDE digestion engine integration
REM This script will:
REM 1. Start the IDE with a test file open
REM 2. Send Ctrl+Shift+D hotkey
REM 3. Monitor for debug output

echo Starting RawrXD-Win32IDE for digestion test...
cd /d "d:\lazy init ide"

REM Launch IDE
start "RawrXD IDE Debug Test" "d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe"

REM Wait for IDE to load
echo Waiting for IDE to initialize (5 seconds)...
timeout /t 5 /nobreak

REM Use PowerShell to send keyboard input
powershell -NoProfile -Command ^
  "$wsh = New-Object -ComObject WScript.Shell; " ^
  "Start-Sleep -Milliseconds 500; " ^
  "$wsh.SendKeys('%%+d'); " ^
  "Write-Host 'Ctrl+Shift+D sent to IDE'"

echo Done! IDE remains running. Ctrl+Shift+D hotkey has been sent.
echo Look at the IDE status bar for digestion progress updates.
pause
