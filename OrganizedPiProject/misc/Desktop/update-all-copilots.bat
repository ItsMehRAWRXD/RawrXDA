@echo off
echo Updating ALL copilot sources to use Kimi K2...

REM VS Code settings
if exist "%APPDATA%\Code\User\settings.json" (
    echo {"github.copilot.advanced": {"debug.overrideEngine": "http://localhost:11434"}} > "%APPDATA%\Code\User\kimi-override.json"
    echo VS Code updated
)

REM Cursor settings
if exist "%APPDATA%\Cursor\User\settings.json" (
    echo {"github.copilot.advanced": {"debug.overrideEngine": "http://localhost:11434"}} > "%APPDATA%\Cursor\User\kimi-override.json"
    echo Cursor updated
)

REM Cline extension
if exist "%USERPROFILE%\.cline" (
    echo {"apiProvider": "openai", "apiUrl": "http://localhost:11434/v1", "model": "moonshotai/kimi-k2"} > "%USERPROFILE%\.cline\kimi-config.json"
    echo Cline updated
)

echo ALL copilot sources now use Kimi K2!
pause