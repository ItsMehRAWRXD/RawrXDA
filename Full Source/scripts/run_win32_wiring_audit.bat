@echo off
REM Run Win32 IDE wiring self-audit; writes reports/win32_ide_wiring_manifest.json and .md
cd /d "%~dp0\.."
python scripts/win32_ide_wiring_audit.py --out reports
exit /b %ERRORLEVEL%
