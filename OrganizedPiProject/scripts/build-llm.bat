@echo off
echo Building Harbor LLM System...

cd custom-editor\src
javac *.java
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

cd ..\..
javac *.java
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful!
echo.
echo Usage:
echo   java EditorCLI          - Start CLI interface
echo   java LLMInterface       - Start GUI interface
echo.
pause