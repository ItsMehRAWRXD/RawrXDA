@echo off
echo Rebuilding Unified Workspace
echo ===========================

python rebuild-unified.py

echo.
echo Workspace rebuilt! Structure:
echo unified-workspace/
echo   ├── rawrz-platform/     (HTTP encryptor + security tools)
echo   ├── ide-suite/          (All IDE projects merged)  
echo   └── ai-tools/           (AI-powered development tools)

pause