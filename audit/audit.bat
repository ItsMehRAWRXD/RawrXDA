@echo off
echo [RawrXD] Starting Interface Audit...
python "%~dp0interface_audit.py"
if %ERRORLEVEL% EQU 0 (
    echo [RawrXD] Audit Success.
) else (
    echo [RawrXD] Audit Failed.
)
pause
