@echo off
echo Installing Cursor Ollama Proxy Extension...

REM Install vsce if not present
npm install -g vsce

REM Package the extension
vsce package

REM Install the extension
for %%f in (*.vsix) do (
    echo Installing %%f
    cursor --install-extension %%f
)

echo Extension installed! Restart Cursor to activate.
pause