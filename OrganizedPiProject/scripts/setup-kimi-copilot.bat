@echo off
cd /d C:\Users\Garre\Desktop

echo Setting up Kimi K2 as VS Code Copilot...

git clone https://github.com/bascodes/copilot-ollama.git
cd copilot-ollama

set /p OPENROUTER_KEY="Enter your OpenRouter API key: "
set OPENROUTER_API_KEY=%OPENROUTER_KEY%

echo Starting proxy server...
uv run main.py