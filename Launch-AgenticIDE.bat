@echo off
setlocal
pwsh -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch-AgenticIDE.ps1" %*
exit /b %errorlevel%
