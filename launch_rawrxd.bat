@echo off
:: ============================================================================
:: RawrXD IDE - All-In-One Launcher
:: Starts Ollama + serve.py + Opens Browser
:: ============================================================================
setlocal enabledelayedexpansion

title RawrXD IDE Launcher

:: --- Configuration ---
set SERVE_PORT=11435
set OLLAMA_PORT=11434
set OLLAMA_URL=http://localhost:%OLLAMA_PORT%
set GUI_URL=http://localhost:%SERVE_PORT%/gui
set SERVE_PY=%~dp0gui\serve.py
set GGUF_DIR=D:\OllamaModels

:: --- Colors ---
echo.
echo [33m  ╔══════════════════════════════════════════════════╗[0m
echo [33m  ║  [1;35m🦖 RawrXD IDE — All-In-One Launcher[0m[33m             ║[0m
echo [33m  ╠══════════════════════════════════════════════════╣[0m
echo [33m  ║  Ollama:   %OLLAMA_URL%                   ║[0m
echo [33m  ║  Server:   http://localhost:%SERVE_PORT%                ║[0m
echo [33m  ║  GUI:      %GUI_URL%            ║[0m
echo [33m  ╚══════════════════════════════════════════════════╝[0m
echo.

:: ============================================================================
:: Step 1: Check Python
:: ============================================================================
echo [36m[1/5][0m Checking Python...
where python >nul 2>&1
if errorlevel 1 (
    echo [31m  ✘ Python not found in PATH.[0m
    echo [33m  Install Python 3.10+ from https://python.org[0m
    pause
    exit /b 1
)
for /f "tokens=*" %%v in ('python --version 2^>^&1') do set PYVER=%%v
echo [32m  ✔ %PYVER%[0m

:: ============================================================================
:: Step 2: Check serve.py exists
:: ============================================================================
echo [36m[2/5][0m Checking serve.py...
if not exist "%SERVE_PY%" (
    echo [31m  ✘ serve.py not found at %SERVE_PY%[0m
    echo [33m  Make sure you're running this from the rawrxd directory.[0m
    pause
    exit /b 1
)
echo [32m  ✔ Found %SERVE_PY%[0m

:: ============================================================================
:: Step 3: Check / Start Ollama
:: ============================================================================
echo [36m[3/5][0m Checking Ollama...

:: First check if ollama is in PATH
where ollama >nul 2>&1
if errorlevel 1 (
    echo [33m  ⚠ Ollama not found in PATH.[0m
    echo [33m  The server will still work — it just won't have Ollama models.[0m
    echo [33m  Local GGUF files in %GGUF_DIR% will still be listed.[0m
    goto :skip_ollama
)

:: Check if Ollama is already running
curl -s -o nul -w "" %OLLAMA_URL%/api/tags >nul 2>&1
if not errorlevel 1 (
    echo [32m  ✔ Ollama already running on port %OLLAMA_PORT%[0m
    goto :ollama_ready
)

:: Start Ollama in background
echo [33m  → Starting Ollama serve...[0m
start "Ollama Server" /min cmd /c "ollama serve 2>&1"

:: Wait for Ollama to come up (max 15 seconds)
set /a WAIT=0
:ollama_wait
if %WAIT% geq 15 (
    echo [33m  ⚠ Ollama didn't respond in 15s — continuing anyway.[0m
    goto :skip_ollama
)
timeout /t 1 /nobreak >nul
curl -s -o nul -w "" %OLLAMA_URL%/api/tags >nul 2>&1
if not errorlevel 1 (
    echo [32m  ✔ Ollama started successfully on port %OLLAMA_PORT%[0m
    goto :ollama_ready
)
set /a WAIT+=1
echo [33m  ... waiting (%WAIT%s)[0m
goto :ollama_wait

:ollama_ready
:: Show model count
for /f "tokens=*" %%m in ('curl -s %OLLAMA_URL%/api/tags 2^>nul ^| findstr /c:"name"') do (
    echo [32m  Models available[0m
)

:skip_ollama
echo.

:: ============================================================================
:: Step 4: Start serve.py
:: ============================================================================
echo [36m[4/5][0m Starting RawrXD HTTP Server...

:: Check if port is already in use
netstat -an | findstr ":%SERVE_PORT% " | findstr "LISTENING" >nul 2>&1
if not errorlevel 1 (
    echo [33m  ⚠ Port %SERVE_PORT% already in use![0m
    echo [33m  Another instance may be running. Trying to connect...[0m
    goto :open_browser
)

:: Launch serve.py in background
start "RawrXD Server" /min cmd /c "cd /d %~dp0 && python "%SERVE_PY%" --port %SERVE_PORT% --ollama-url %OLLAMA_URL% 2>&1"

:: Wait for serve.py to come up (max 10 seconds)
set /a WAIT=0
:serve_wait
if %WAIT% geq 10 (
    echo [31m  ✘ serve.py didn't start in 10s.[0m
    echo [33m  Check the "RawrXD Server" window for errors.[0m
    pause
    exit /b 1
)
timeout /t 1 /nobreak >nul
curl -s -o nul -w "" http://localhost:%SERVE_PORT%/health >nul 2>&1
if not errorlevel 1 (
    echo [32m  ✔ RawrXD Server running on port %SERVE_PORT%[0m
    goto :open_browser
)
set /a WAIT+=1
echo [33m  ... waiting (%WAIT%s)[0m
goto :serve_wait

:: ============================================================================
:: Step 5: Open Browser
:: ============================================================================
:open_browser
echo [36m[5/5][0m Opening browser...
echo.

:: Brief pause to let the server finish init
timeout /t 1 /nobreak >nul

start "" "%GUI_URL%"

echo [32m  ✔ Browser opened to %GUI_URL%[0m
echo.
echo [1;33m  ╔══════════════════════════════════════════════════╗[0m
echo [1;33m  ║  [1;32m✔ RawrXD IDE is LIVE[0m[1;33m                             ║[0m
echo [1;33m  ╠══════════════════════════════════════════════════╣[0m
echo [1;33m  ║  GUI:     %GUI_URL%            ║[0m
echo [1;33m  ║  Server:  http://localhost:%SERVE_PORT%                ║[0m
echo [1;33m  ║  Ollama:  %OLLAMA_URL%                   ║[0m
echo [1;33m  ╠══════════════════════════════════════════════════╣[0m
echo [1;33m  ║  Press Ctrl+C here to stop the launcher.        ║[0m
echo [1;33m  ║  Close "Ollama Server" window to stop Ollama.   ║[0m
echo [1;33m  ║  Close "RawrXD Server" window to stop serve.py. ║[0m
echo [1;33m  ╚══════════════════════════════════════════════════╝[0m
echo.

:: Keep this window alive so user can see status
echo [33mPress any key to stop all services and exit...[0m
pause >nul

:: Cleanup: kill serve.py and ollama if we started them
echo.
echo [33mShutting down...[0m
taskkill /fi "WINDOWTITLE eq RawrXD Server" /f >nul 2>&1
taskkill /fi "WINDOWTITLE eq Ollama Server" /f >nul 2>&1
echo [32m  ✔ Services stopped. Goodbye![0m
timeout /t 2 /nobreak >nul
exit /b 0
