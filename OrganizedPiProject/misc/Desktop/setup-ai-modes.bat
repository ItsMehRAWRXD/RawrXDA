@echo off
REM Complete AI Mode Switching Setup
REM This script sets up the complete offline/online/local AI mode switching system

echo ========================================
echo   AI Mode Switching Setup
echo ========================================
echo.

REM Check prerequisites
echo Checking prerequisites...

REM Check Java
java -version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: Java is not installed or not in PATH
    echo Please install Java 8 or later
    pause
    exit /b 1
)
echo [OK] Java found

REM Check PHP
php --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: PHP is not installed or not in PATH
    echo Please install PHP 8.0 or later
    pause
    exit /b 1
)
echo [OK] PHP found

REM Check Node.js
node --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: Node.js is not installed or not in PATH
    echo Please install Node.js
    pause
    exit /b 1
)
echo [OK] Node.js found

REM Check Docker
docker --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Warning: Docker is not installed
    echo Offline linting will not work without Docker
    echo Please install Docker Desktop
    set DOCKER_AVAILABLE=false
) else (
    echo [OK] Docker found
    set DOCKER_AVAILABLE=true
)

REM Check API key
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
echo [OK] GEMINI_API_KEY found: %GEMINI_API_KEY:~0,10%...

echo.

REM Compile Java files
echo Compiling Java components...
javac AiChat.java
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to compile AiChat.java
    pause
    exit /b 1
)

javac ScratchAIBridge.java
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to compile ScratchAIBridge.java
    pause
    exit /b 1
)
echo [OK] Java components compiled

REM Compile the enhanced AiChat
javac AiChat.java
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to compile enhanced AiChat.java
    pause
    exit /b 1
)
echo [OK] Enhanced AiChat compiled with Cursor-like features
if "%DOCKER_AVAILABLE%"=="true" (
    echo.
    echo Building Docker sandbox image with linters...
    docker build -f Dockerfile.sandbox -t rawrz-secure-compiler:1.0 .
    if %ERRORLEVEL% neq 0 (
        echo Error: Failed to build Docker image
        echo Offline linting will not work
        set DOCKER_AVAILABLE=false
    ) else (
        echo [OK] Docker image built successfully
    )
)

REM Create startup scripts
echo.
echo Creating startup scripts...

REM PHP AI CLI startup
(
echo @echo off
echo echo Starting PHP AI CLI...
echo echo Available modes: online, offline, local
echo echo Current default mode:
echo php ultra_turbo_ai_cli.php --set-mode 2^>nul ^|^| echo "Not set - will use online"
echo echo.
echo echo Usage examples:
echo echo   php ultra_turbo_ai_cli.php --ask "Hello AI" --env
echo echo   php ultra_turbo_ai_cli.php --mode offline --ask "Code review" --examples php
echo echo   php ultra_turbo_ai_cli.php --mode local --ask "Help" --env
echo echo.
echo pause
) > start-php-ai.bat

REM Java AI Chat startup
(
echo @echo off
echo echo Starting Java AI Chat...
echo echo Press Ctrl+C or empty line to exit
echo echo.
echo java AiChat
echo pause
) > start-java-chat.bat

REM Secure compile API startup
if "%DOCKER_AVAILABLE%"=="true" (
    (
    echo @echo off
    echo echo Starting Secure Compile API with linting...
    echo echo Endpoints:
    echo echo   POST /compile - Compile code
    echo echo   POST /lint - Lint code ^(offline^)
    echo echo.
    echo node secure-compile-api.js
    echo pause
    ) > start-compile-api.bat
)

REM Review orchestrator startup
(
echo @echo off
echo echo Starting Review Orchestrator...
echo echo Endpoints:
echo echo   POST /review - Multi-AI code review
echo echo   GET /health - Health check
echo echo.
echo echo Modes: online ^(default^), offline ^(linters^), local
echo echo.
echo node review-orchestrator.js
echo pause
) > start-orchestrator.bat

REM Scratch AI Bridge startup
(
echo @echo off
echo echo Starting Scratch AI Bridge...
echo echo This enables AI assistance in Scratch programming
echo echo.
echo java ScratchAIBridge
echo pause
) > start-scratch-bridge.bat

REM Private Puller startup
(
echo @echo off
echo echo Starting Private Puller ^(No-Key Web Search^)...
echo echo Features: DuckDuckGo search, web fetching, offline summarization
echo echo Open: http://127.0.0.1:8787
echo echo.
echo node puller.js
echo pause
) > start-puller.bat

echo [OK] Startup scripts created

REM Create comprehensive test script
echo.
echo Creating test script...
(
echo @echo off
echo echo ========================================
echo echo   AI Mode System Test
echo echo ========================================
echo echo.
echo echo Testing PHP AI CLI modes...
echo echo.
echo echo 1. Testing online mode:
echo php ultra_turbo_ai_cli.php --mode online --ask "Say hello" --env
echo echo.
echo echo 2. Testing offline mode:
echo php ultra_turbo_ai_cli.php --mode offline --ask "Code review" --examples java
echo echo.
echo echo 3. Testing mode persistence:
echo php ultra_turbo_ai_cli.php --set-mode offline
echo echo.
echo echo 4. Testing Enhanced Java AI Chat:
echo echo "Hello AI" ^| java AiChat
echo echo "Testing with parameters:" ^| java AiChat --stdin --quiet --temperature 0.7
echo echo.
if "%DOCKER_AVAILABLE%"=="true" (
    echo echo 5. Testing linting endpoint:
    echo curl -X POST http://localhost:4040/lint -H "Content-Type: application/json" -d "{\"files\":[{\"path\":\"test.js\",\"content\":\"const x=1;\\nconsole.log(y)\"}]}"
    echo echo.
    echo echo 6. Testing review orchestrator:
    echo curl -X POST http://localhost:5050/review -H "Content-Type: application/json" -d "{\"files\":[\"test.js\"],\"mode\":\"offline\"}"
    echo echo.
    echo echo 7. Testing Private Puller:
    echo curl -X POST http://localhost:8787/search -H "Content-Type: application/json" -d "{\"q\":\"Java programming\",\"mode\":\"offline\"}"
    echo echo.
)
echo echo ========================================
echo echo   Test Complete
echo echo ========================================
echo pause
) > test-ai-modes.bat

echo [OK] Test script created

echo.
echo ========================================
echo   Setup Complete!
echo ========================================
echo.
echo Components installed:
echo [OK] PHP AI CLI with mode switching
echo [OK] Enhanced Java AI Chat ^(Cursor-style REPL^)
echo [OK] Java Scratch AI Bridge
echo [OK] Node.js Review Orchestrator
echo [OK] Private Puller ^(No-Key Web Search^)
if "%DOCKER_AVAILABLE%"=="true" (
    echo [OK] Docker sandbox with linters
    echo [OK] Secure Compile API with /lint endpoint
) else (
    echo [SKIP] Docker components ^(Docker not available^)
)
echo [OK] AutoHotkey mode switcher
echo.
echo Startup scripts created:
echo   start-php-ai.bat        - PHP AI CLI
echo   start-java-chat.bat     - Enhanced Java AI Chat REPL
echo   start-orchestrator.bat  - Review Orchestrator
echo   start-puller.bat        - Private Puller ^(Web Search^)
if "%DOCKER_AVAILABLE%"=="true" (
    echo   start-compile-api.bat   - Secure Compile API
)
echo   start-scratch-bridge.bat - Scratch AI Bridge
echo   test-ai-modes.bat       - Run all tests
echo.
echo Usage:
echo 1. Run: test-ai-modes.bat ^(test everything^)
echo 2. Use AutoHotkey: CopilotModeSwitch.ahk ^(Ctrl+Alt+M^)
echo 3. Start services as needed with startup scripts
echo.
echo For editor integration, see the examples in:
echo   - AiChat.java ^(Java REPL for editor integration^)
echo   - ultra_turbo_ai_cli.php ^(PHP CLI with modes^)
echo   - CopilotModeSwitch.ahk ^(AutoHotkey mode switcher^)
echo.
pause
