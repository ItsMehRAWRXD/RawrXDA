@echo off
REM RawrXD-AgenticIDE Launcher
REM Automatically configures and launches the Qt-based IDE

setlocal enabledelayedexpansion

title RawrXD Agentic IDE Launcher
cls

echo.
echo ===================================================
echo  RawrXD Agentic IDE - AI-Powered Code Editor
echo ===================================================
echo.

REM Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
set "IDE_EXE=%SCRIPT_DIR%RawrXD-AgenticIDE.exe"

REM Check if IDE executable exists
if not exist "%IDE_EXE%" (
    echo ERROR: RawrXD-AgenticIDE.exe not found at:
    echo   %IDE_EXE%
    echo.
    echo Please ensure the executable is in the same directory as this launcher.
    pause
    exit /b 1
)

echo [*] Checking prerequisites...
echo.

REM Check for Ollama (AI completions)
tasklist /FI "IMAGENAME eq ollama.exe" 2>NUL | find /I /N "ollama.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo [✓] Ollama service detected - AI completions enabled
) else (
    echo [!] WARNING: Ollama not running
    echo     AI code completions will be unavailable.
    echo     To enable: Start Ollama (ollama serve) in another terminal
    echo.
)

REM Check for Clang/LSP
where clangd >NUL 2>&1
if "%ERRORLEVEL%"=="0" (
    echo [✓] Clang found - LSP diagnostics enabled
) else (
    echo [!] WARNING: Clang not installed
    echo     Real-time diagnostics will be unavailable.
    echo.
)

echo.
echo [*] Launching RawrXD-AgenticIDE...
echo.

REM Launch the IDE
start "" "%IDE_EXE%"

echo [✓] IDE launched successfully
echo.
echo Troubleshooting:
echo - If the IDE doesn't start, check that Qt runtime DLLs are available
echo - For AI completions, ensure Ollama is running (ollama serve)
echo - See DEPLOYMENT_GUIDE.md for detailed configuration
echo.
endlocal
