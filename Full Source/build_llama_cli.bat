@echo off
setlocal

rem ============================================================================
rem build_llama_cli.bat
rem Wrapper for build_llama_cli.ps1 (more reliable than cmd response parsing).
rem ============================================================================

powershell -ExecutionPolicy Bypass -File "%~dp0build_llama_cli.ps1"
exit /b %errorlevel%

