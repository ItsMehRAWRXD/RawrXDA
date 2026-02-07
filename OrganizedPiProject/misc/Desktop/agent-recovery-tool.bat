@echo off
REM Agent Recovery Tool - Windows Version
REM Restore Communication with Silent AI Agents

echo 🔧 Agent Recovery Tool - Windows
echo ================================
echo.

REM Check agent communication status
echo [INFO] Checking agent communication status...

REM Check for running AI processes
tasklist /FI "IMAGENAME eq java.exe" /FO TABLE | findstr /I "java"
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Found Java processes running
) else (
    echo [WARNING] No Java processes found
)

REM Check for agent-related files
if exist "AgenticOrchestrator.java" (
    echo [INFO] Found AgenticOrchestrator.java
) else (
    echo [WARNING] AgenticOrchestrator.java not found
)

if exist "agent-recovery" (
    echo [INFO] Found agent recovery directory
    dir agent-recovery
) else (
    echo [INFO] No agent recovery directory found
)

REM Create agent health check
echo [INFO] Creating agent health check...
echo { > agent-health-check.json
echo     "timestamp": "%date% %time%", >> agent-health-check.json
echo     "status": "recovery_attempt", >> agent-health-check.json
echo     "recovery_initiated": true >> agent-health-check.json
echo } >> agent-health-check.json

echo [SUCCESS] Created agent-health-check.json

REM Attempt to restart communication
echo [INFO] Attempting to restart agent communication...

REM Kill any stuck processes (if needed)
REM taskkill /F /IM java.exe 2>nul

REM Clear any lock files
del .agent-lock 2>nul
del .communication-lock 2>nul

echo [SUCCESS] Agent recovery process completed!
echo [WARNING] Please manually restart your AI system to restore communication
echo.
echo To restart your AI system:
echo 1. Run your main AI application
echo 2. Check agent-health-check.json for status
echo 3. Verify agents are responding to commands

pause
