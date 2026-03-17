@echo off
setlocal
cd /d "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\ide-extensions\bigdaddyg-copilot"

echo Compiling TypeScript...
call npm run compile

echo.
echo Copying to Cursor installation...
xcopy "." "E:\Everything\cursor\extensions\bigdaddyg-copilot-1.0.0\" /E /I /Y /Q

echo.
echo Packaging as VSIX...
cd "E:\Everything\cursor\extensions\bigdaddyg-copilot-1.0.0"
call npx -y @vscode/vsce package --allow-missing-repository --out "bigdaddyg-copilot-1.0.0.vsix" 2>nul

echo.
if exist "bigdaddyg-copilot-1.0.0.vsix" (
    echo SUCCESS! Extension is ready.
    echo.
    echo Installation command:
    echo   "E:\Everything\cursor\Cursor.exe" --install-extension "E:\Everything\cursor\extensions\bigdaddyg-copilot-1.0.0\bigdaddyg-copilot-1.0.0.vsix"
) else (
    echo ERROR: VSIX generation failed
)

pause
