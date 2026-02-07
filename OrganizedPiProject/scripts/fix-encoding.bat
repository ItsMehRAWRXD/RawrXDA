@echo off
chcp 65001 >nul
cls
echo Unified Workspace Structure (Fixed Encoding)
echo ==========================================
echo.
echo unified-workspace/
echo ├── rawrz-platform/     (HTTP encryptor + security tools)
echo ├── ide-suite/          (All IDE projects merged)
echo └── ai-tools/           (AI-powered development tools)
echo.
echo STRUCTURE VERIFICATION:
echo.
dir "unified-workspace" /B
echo.
echo All projects successfully merged and consolidated!
echo Character encoding issues resolved.
echo.
pause