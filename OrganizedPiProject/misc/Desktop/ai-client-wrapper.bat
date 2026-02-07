@echo off
REM AI Client Wrapper for Windows
REM This script provides a consistent interface for calling AI clients from the bridge server

setlocal enabledelayedexpansion

REM Check if we have arguments
if "%~1"=="" (
    echo Usage: ai-client-wrapper.bat [java^|php] [arguments...]
    exit /b 1
)

set CLIENT_TYPE=%1
shift

REM Set up environment
set JAVA_HOME=%JAVA_HOME%
set PATH=%PATH%;%JAVA_HOME%\bin
set GEMINI_API_KEY=%GEMINI_API_KEY%

REM Check if API key is set
if "%GEMINI_API_KEY%"=="" (
    echo Error: GEMINI_API_KEY environment variable is not set
    exit /b 1
)

if "%CLIENT_TYPE%"=="java" (
    REM Use Java AI client
    java -cp .;jackson-core-2.15.2.jar;jackson-databind-2.15.2.jar;jackson-annotations-2.15.2.jar AIChatClient %*
    set EXIT_CODE=%ERRORLEVEL%
) else if "%CLIENT_TYPE%"=="php" (
    REM Use PHP AI client
    php ai_cli.php %*
    set EXIT_CODE=%ERRORLEVEL%
) else (
    echo Error: Unknown client type "%CLIENT_TYPE%". Use 'java' or 'php'.
    exit /b 1
)

exit /b %EXIT_CODE%
