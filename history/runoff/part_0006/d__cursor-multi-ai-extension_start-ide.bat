@echo off
echo Starting Chrome DevTools IDE Server...
cd /d "D:\cursor-multi-ai-extension"
start "" http://localhost:8080/DevMarketIDE/
start "" http://localhost:8080/chrome-devtools-ide/public/
node local-dev-server.js
pause