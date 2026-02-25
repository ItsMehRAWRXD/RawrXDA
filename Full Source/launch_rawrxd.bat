@echo off
:: ============================================================================
:: RawrXD IDE - All-In-One Launcher
:: Prefers MASM webserver (gui\webserver.asm) for GUI + HTTPS; Node/backend for API. No Python.
:: ============================================================================
setlocal enabledelayedexpansion

title RawrXD IDE Launcher

:: --- Configuration ---
set SERVE_PORT=8080
set MASM_PORT=3000
set BACKEND_PORT=11435
set OLLAMA_PORT=11434
set OLLAMA_URL=http://localhost:%OLLAMA_PORT%
set NODE_SERVER=%~dp0server.js
set GUI_DIR=%~dp0gui
set MASM_EXE=%GUI_DIR%webserver.exe
set SERVER_8080_EXE=%GUI_DIR%server_8080.exe
set TOOL_SERVER_EXE=%~dp0src\tool_server.exe

:: Prefer MASM GUI when webserver.exe exists — pure metal, no Python (HTTPS via reverse proxy; default HTTP on 3000)
if exist "%MASM_EXE%" (
    set GUI_URL=http://localhost:%MASM_PORT%/launcher.html
    set USE_MASM=1
) else (
    set GUI_URL=http://localhost:%SERVE_PORT%/launcher
    set USE_MASM=0
)

:: Pure MASM backend: server_8080 on 8080 + tool_server on 11435 (no Node)
if exist "%SERVER_8080_EXE%" (
    set USE_MASM_BACKEND=1
) else (
    set USE_MASM_BACKEND=0
)

:: --- Colors ---
echo.
echo [33m  ╔══════════════════════════════════════════════════╗[0m
echo [33m  ║  [1;35m RawrXD IDE — Launcher (MASM + Backend)[0m[33m         ║[0m
echo [33m  ╠══════════════════════════════════════════════════╣[0m
if %USE_MASM%==1 (
echo [33m  ║  GUI:     MASM webserver port %MASM_PORT% (HTTPS via proxy)  ║[0m
) else (
echo [33m  ║  GUI:     Node server port %SERVE_PORT%                 ║[0m
)
if %USE_MASM_BACKEND%==1 (
echo [33m  ║  Backend: MASM server_8080 :%SERVE_PORT% ^> tool_server :%BACKEND_PORT%[0m
) else (
echo [33m  ║  Backend: Node server.js port %SERVE_PORT%              ║[0m
)
echo [33m  ║  Open:    %GUI_URL%[0m
echo [33m  ║  Ollama:  %OLLAMA_URL%                   ║[0m
echo [33m  ╚══════════════════════════════════════════════════╝[0m
echo.

:: ============================================================================
:: Step 1: Check Node.js (skip when using MASM backend)
:: ============================================================================
if %USE_MASM_BACKEND%==1 goto :skip_node_checks
echo [36m[1/5][0m Checking Node.js...
where node >nul 2>&1
if errorlevel 1 (
    echo [31m  X Node.js not found in PATH.[0m
    echo [33m  Install from https://nodejs.org/ or build server_8080.exe for pure MASM backend.[0m
    pause
    exit /b 1
)
for /f "tokens=*" %%v in ('node --version 2^>^&1') do set NODEVER=%%v
echo [32m  OK %NODEVER%[0m

:: ============================================================================
:: Step 2: Check server.js exists (skip when using MASM backend)
:: ============================================================================
echo [36m[2/5][0m Checking server.js...
if not exist "%NODE_SERVER%" (
    echo [31m  X server.js not found at %NODE_SERVER%[0m
    echo [33m  Run this from the rawrxd repo root.[0m
    pause
    exit /b 1
)
echo [32m  OK Found server.js[0m
goto :node_checks_done

:skip_node_checks
echo [36m[1/5][0m Using pure MASM backend (no Node)...
echo [32m  OK server_8080.exe + tool_server on %BACKEND_PORT%[0m
echo [36m[2/5][0m Skipping Node (MASM backend).
:node_checks_done

:: ============================================================================
:: Step 3: Check / Start Ollama (optional)
:: ============================================================================
echo [36m[3/5][0m Checking Ollama...
where ollama >nul 2>&1
if errorlevel 1 (
    echo [33m  Ollama not in PATH. Server will still run; use local models or HeadlessIDE.[0m
    goto :skip_ollama
)
curl -s -o nul -w "" %OLLAMA_URL%/api/tags >nul 2>&1
if not errorlevel 1 (
    echo [32m  OK Ollama already running on port %OLLAMA_PORT%[0m
    goto :skip_ollama
)
echo [33m  Starting Ollama...[0m
start "Ollama Server" /min cmd /c "ollama serve 2>&1"
set /a WAIT=0
:ollama_wait
if %WAIT% geq 15 goto :skip_ollama
timeout /t 1 /nobreak >nul
curl -s -o nul -w "" %OLLAMA_URL%/api/tags >nul 2>&1
if not errorlevel 1 (
    echo [32m  OK Ollama started[0m
    goto :skip_ollama
)
set /a WAIT+=1
goto :ollama_wait

:skip_ollama
echo.

:: ============================================================================
:: Step 4: Start MASM GUI (if webserver.exe) + Backend (server_8080+tool_server or Node)
:: ============================================================================
if %USE_MASM%==1 (
    echo [36m[4/5][0m Starting MASM GUI (webserver.exe port %MASM_PORT%)...
    netstat -an | findstr ":%MASM_PORT% " | findstr "LISTENING" >nul 2>&1
    if errorlevel 1 (
        start "RawrXD MASM GUI" /min cmd /c "cd /d %GUI_DIR% && webserver.exe 2>&1"
        timeout /t 2 /nobreak >nul
    )
    echo [32m  OK MASM GUI on port %MASM_PORT% (HTTPS: use reverse proxy)[0m
)

if %USE_MASM_BACKEND%==1 goto :start_masm_backend

echo [36m[4/5][0m Starting RawrXD backend (node server.js)...
netstat -an | findstr ":%SERVE_PORT% " | findstr "LISTENING" >nul 2>&1
if not errorlevel 1 (
    echo [33m  Port %SERVE_PORT% already in use; assuming backend is running.[0m
    goto :open_browser
)

start "RawrXD Server" /min cmd /c "cd /d %~dp0 && node server.js 2>&1"

set /a WAIT=0
:serve_wait
if %WAIT% geq 10 (
    echo [31m  X Backend did not start in 10s. Check "RawrXD Server" window.[0m
    pause
    exit /b 1
)
timeout /t 1 /nobreak >nul
curl -s -o nul -w "" http://localhost:%SERVE_PORT%/status >nul 2>&1
if not errorlevel 1 (
    echo [32m  OK RawrXD backend running on port %SERVE_PORT%[0m
    goto :open_browser
)
set /a WAIT+=1
echo [33m  ... waiting (%WAIT%s)[0m
goto :serve_wait

:: --- Pure MASM backend: tool_server :11435 then server_8080 :8080 ---
:start_masm_backend
echo [36m[4/5][0m Starting RawrXD backend (MASM server_8080 + tool_server)...
netstat -an | findstr ":%SERVE_PORT% " | findstr "LISTENING" >nul 2>&1
if not errorlevel 1 (
    echo [33m  Port %SERVE_PORT% already in use; assuming backend is running.[0m
    goto :open_browser
)

netstat -an | findstr ":%BACKEND_PORT% " | findstr "LISTENING" >nul 2>&1
if errorlevel 1 (
    if not exist "%TOOL_SERVER_EXE%" (
        echo [31m  X tool_server.exe not found at %TOOL_SERVER_EXE%[0m
        echo [33m  Build with: cd src ^&^& build_tool_server.bat (or run tool_server --port %BACKEND_PORT% manually)[0m
        pause
        exit /b 1
    )
    echo [33m  Starting tool_server on port %BACKEND_PORT%...[0m
    start "RawrXD Backend (tool_server)" /min cmd /c "cd /d %~dp0src && tool_server.exe --port %BACKEND_PORT% 2>&1"
    timeout /t 2 /nobreak >nul
) else (
    echo [32m  OK tool_server already on port %BACKEND_PORT%[0m
)

echo [33m  Starting server_8080 on port %SERVE_PORT%...[0m
start "RawrXD MASM Backend" /min cmd /c "cd /d %GUI_DIR% && server_8080.exe 2>&1"
timeout /t 2 /nobreak >nul

set /a WAIT=0
:masm_serve_wait
if %WAIT% geq 10 (
    echo [31m  X server_8080 did not start in 10s. Check "RawrXD MASM Backend" and "RawrXD Backend (tool_server)" windows.[0m
    pause
    exit /b 1
)
timeout /t 1 /nobreak >nul
curl -s -o nul -w "" http://localhost:%SERVE_PORT%/status >nul 2>&1
if not errorlevel 1 (
    echo [32m  OK RawrXD backend (MASM) on port %SERVE_PORT%[0m
    goto :open_browser
)
set /a WAIT+=1
echo [33m  ... waiting (%WAIT%s)[0m
goto :masm_serve_wait

:: ============================================================================
:: Step 5: Open Browser
:: ============================================================================
:open_browser
echo [36m[5/5][0m Opening browser...
timeout /t 1 /nobreak >nul
start "" "%GUI_URL%"
echo [32m  OK Browser opened to %GUI_URL%[0m
echo.
echo [1;33m  ╔══════════════════════════════════════════════════╗[0m
echo [1;33m  ║  [1;32mOK RawrXD IDE is LIVE[0m[1;33m                             ║[0m
echo [1;33m  ╠══════════════════════════════════════════════════╣[0m
echo [1;33m  ║  Launcher: %GUI_URL%   ║[0m
if %USE_MASM_BACKEND%==1 (
echo [1;33m  ║  Backend:  http://localhost:%SERVE_PORT% (MASM ^> %BACKEND_PORT%)[0m[1;33m   ║[0m
) else (
echo [1;33m  ║  Backend:  http://localhost:%SERVE_PORT% (Node)[0m[1;33m        ║[0m
)
echo [1;33m  ║  Ollama:   %OLLAMA_URL%                   ║[0m
if %USE_MASM_BACKEND%==1 (
echo [1;33m  ║  Close "RawrXD MASM Backend" / "RawrXD Backend (tool_server)" to stop.[0m[1;33m ║[0m
) else if %USE_MASM%==1 (
echo [1;33m  ║  Close "RawrXD MASM GUI" to stop GUI; "RawrXD Server" for API.[0m[1;33m ║[0m
) else (
echo [1;33m  ║  Close "RawrXD Server" window to stop backend.  ║[0m
)
echo [1;33m  ╚══════════════════════════════════════════════════╝[0m
echo.
echo [33mPress any key to exit (servers keep running until closed)...[0m
pause >nul
taskkill /fi "WINDOWTITLE eq RawrXD MASM GUI" /f >nul 2>&1
taskkill /fi "WINDOWTITLE eq RawrXD Server" /f >nul 2>&1
taskkill /fi "WINDOWTITLE eq RawrXD MASM Backend" /f >nul 2>&1
taskkill /fi "WINDOWTITLE eq RawrXD Backend (tool_server)" /f >nul 2>&1
taskkill /fi "WINDOWTITLE eq Ollama Server" /f >nul 2>&1
echo [32m  Services stopped.[0m
timeout /t 2 /nobreak >nul
exit /b 0
