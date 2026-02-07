@echo off
cd /d C:\Users\Garre\Desktop\Desktop

echo Setting up Kimi K2 as VS Code Copilot...

if exist copilot-ollama rmdir /s /q copilot-ollama
git clone https://github.com/bascodes/copilot-ollama.git
cd copilot-ollama

set /p OPENROUTER_KEY="Enter your OpenRouter API key: "
set OPENROUTER_API_KEY=%OPENROUTER_KEY%

echo Installing dependencies...
pip install flask requests

echo Starting proxy server...
python main.py