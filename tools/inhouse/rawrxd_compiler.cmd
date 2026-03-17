@echo off
setlocal
set SCRIPT_DIR=%~dp0
where /q pwsh
if %ERRORLEVEL%==0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%rawrxd_compiler.ps1" %*
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%rawrxd_compiler.ps1" %*
)
exit /b %ERRORLEVEL%
