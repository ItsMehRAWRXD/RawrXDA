@echo off
REM Setup script for Scratch AI Integration
REM This script sets up the complete environment for running AI assistance in Scratch

echo ========================================
echo   Scratch AI Integration Setup
echo ========================================
echo.

REM Check if Java is available
java -version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: Java is not installed or not in PATH
    echo Please install Java 8 or later
    pause
    exit /b 1
)

REM Check if PHP is available (optional)
php --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Warning: PHP is not available. Only Java AI client will be used.
    set USE_PHP=false
) else (
    echo PHP found. Both Java and PHP AI clients will be available.
    set USE_PHP=true
)

REM Check for GEMINI_API_KEY
if "%GEMINI_API_KEY%"=="" (
    echo.
    echo Error: GEMINI_API_KEY environment variable is not set
    echo Please set your Gemini API key:
    echo   set GEMINI_API_KEY=your_api_key_here
    echo.
    echo You can get a free API key from:
    echo   https://aistudio.google.com/app/apikey
    echo.
    pause
    exit /b 1
)

echo API key found: %GEMINI_API_KEY:~0,10%...
echo.

REM Compile the bridge server
echo Compiling Scratch AI Bridge server...
javac ScratchAIBridge.java
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to compile ScratchAIBridge.java
    echo Make sure you have the required dependencies
    pause
    exit /b 1
)

echo Bridge server compiled successfully.
echo.

REM Create startup script
echo Creating startup script...
(
echo @echo off
echo echo Starting Scratch AI Bridge Server...
echo echo.
echo echo Make sure Scratch is open and the extension is loaded before using.
echo echo Press Ctrl+C to stop the server.
echo echo.
echo java ScratchAIBridge
echo pause
) > start-scratch-ai.bat

echo Startup script created: start-scratch-ai.bat
echo.

REM Test the bridge server
echo Testing bridge server startup...
timeout /t 2 >nul
start /min java ScratchAIBridge
timeout /t 3 >nul

REM Test connection
curl -s http://localhost:8001/health >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Bridge server test: SUCCESS
    taskkill /f /im java.exe >nul 2>&1
) else (
    echo Bridge server test: FAILED
    echo Please check if port 8001 is available
)

echo.
echo ========================================
echo   Setup Complete!
echo ========================================
echo.
echo Next steps:
echo 1. Run: start-scratch-ai.bat
echo 2. Open Scratch ^(scratch.mit.edu or Scratch Desktop^)
echo 3. Go to Extensions ^(bottom left^)
echo 4. Click "Load an Extension"
echo 5. Choose "Load Experimental Extension"
echo 6. Copy the contents of scratch-ai-extension.js
echo 7. Start using AI blocks in your Scratch projects!
echo.
echo For help, see the documentation in scratch-ai-readme.md
echo.
pause
