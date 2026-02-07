@echo off
echo Updating all Copilot extensions...

code --install-extension GitHub.copilot --force
code --install-extension GitHub.copilot-chat --force
code --install-extension GitHub.copilot-labs --force

echo Copilot extensions updated successfully!
pause