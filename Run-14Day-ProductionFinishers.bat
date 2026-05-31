@echo off
setlocal

set ROOT=%~dp0
set SCRIPT=%ROOT%Run-14Day-ProductionFinishers.ps1

if not exist "%SCRIPT%" (
  echo Missing launcher: %SCRIPT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
exit /b %ERRORLEVEL%
