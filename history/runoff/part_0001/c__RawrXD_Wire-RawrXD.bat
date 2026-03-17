@echo off
setlocal

set SCRIPT=%~dp0Wire-RawrXD.ps1
if not exist "%SCRIPT%" (
  echo [ERROR] Wire-RawrXD.ps1 not found: %SCRIPT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
if errorlevel 1 (
  echo [FAIL] Wiring failed.
  exit /b 1
)

echo [OK] Wiring completed.
exit /b 0
