@echo off
title RawrXD Feature Test Assistant
echo ============================================
echo  RawrXD Feature Test Assistant Launcher
echo ============================================
echo.
echo Checking Ollama status...
curl -s http://localhost:11434/ >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Ollama is not running on :11434
    echo     Please start Ollama first.
    echo.
    pause
    exit /b 1
)
echo [OK] Ollama is running on :11434
echo.
echo Opening test assistant...
start "" "d:\rawrxd\dist\RawrXD-IDE-v7.4.0-win64\gui\test_assistant.html"
echo.
echo Test assistant opened in your default browser.
echo Click "Run All Tests" to start the full test suite.
echo.
