@echo off
REM RawrXD Agentic - Direct PowerShell Launcher
REM This runs the PowerShell script directly, avoiding EXE antivirus false positives

setlocal enabledelayedexpansion
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "Launch-RawrXD-Agentic.ps1" %*

pause
