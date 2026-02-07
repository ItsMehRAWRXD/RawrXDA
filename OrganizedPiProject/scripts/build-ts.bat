@echo off
echo Building TypeScript AI Bridge...

cd ai-first-editor
call npm install
call npm run build
cd ..

javac *.java
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful!
echo.
echo Usage:
echo   java AgentCLI
echo   /key openai sk-your-key-here
echo   /switch openai  
echo   /complete "function fibonacci(n) {"
echo   /providers
pause