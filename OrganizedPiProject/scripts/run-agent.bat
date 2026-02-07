@echo off
echo Building Agent LLM System...

javac -cp "javax.json-1.1.4.jar" *.java
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful!
echo.
echo Usage:
echo   java AgentCLI           - Enhanced CLI with orchestration
echo   java -cp . ChatManager  - GUI with agent integration
echo.
echo Set OPENAI_API_KEY environment variable for full functionality
pause