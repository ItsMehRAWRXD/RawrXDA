@echo off
REM ============================================================================
REM RawrXD CoT Stack Toggle Script (Action Item #18)
REM Usage:
REM   cot_toggle.bat enable   — Enable CoT backend
REM   cot_toggle.bat disable  — Disable CoT backend (IDE stays fully usable)
REM   cot_toggle.bat status   — Check current CoT status
REM   cot_toggle.bat rollback — Full rollback: disable CoT + stop Python backend
REM ============================================================================

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set COT_CONFIG=%SCRIPT_DIR%..\config\cot_toggle.json
set ROLLBACK_CONFIG=%SCRIPT_DIR%..\config\rollback.json
set COT_ENGINE=%SCRIPT_DIR%..\services\rawrxd_cot_engine.py

if "%1"=="" goto :usage
if "%1"=="enable" goto :enable
if "%1"=="disable" goto :disable
if "%1"=="status" goto :status
if "%1"=="rollback" goto :rollback
goto :usage

:enable
echo [RawrXD] Enabling CoT stack...
powershell -Command "(Get-Content '%COT_CONFIG%') -replace '\"cot_enabled\": false', '\"cot_enabled\": true' | Set-Content '%COT_CONFIG%'"
powershell -Command "(Get-Content '%ROLLBACK_CONFIG%') -replace '\"rollback_active\": true', '\"rollback_active\": false' | Set-Content '%ROLLBACK_CONFIG%'"
echo [RawrXD] CoT enabled. Start the Python backend: python services\rawrxd_cot_engine.py
echo [RawrXD] Then restart the IDE to pick up the change.
goto :done

:disable
echo [RawrXD] Disabling CoT stack (IDE stays fully usable)...
powershell -Command "(Get-Content '%COT_CONFIG%') -replace '\"cot_enabled\": true', '\"cot_enabled\": false' | Set-Content '%COT_CONFIG%'"
echo [RawrXD] CoT disabled. Prompts will go directly to LLM.
echo [RawrXD] Restart the IDE to apply.
goto :done

:status
echo [RawrXD] CoT Stack Status:
echo.
if exist "%COT_CONFIG%" (
    type "%COT_CONFIG%"
) else (
    echo   Config not found: %COT_CONFIG%
)
echo.
echo Checking Python CoT backend...
powershell -Command "try { $r = Invoke-WebRequest -Uri 'http://localhost:5000/api/cot/health' -TimeoutSec 3 -ErrorAction Stop; Write-Host '  Backend: RUNNING' -ForegroundColor Green; Write-Host $r.Content } catch { Write-Host '  Backend: NOT RUNNING' -ForegroundColor Red }"
goto :done

:rollback
echo [RawrXD] FULL ROLLBACK — Disabling CoT and stopping Python backend...
powershell -Command "(Get-Content '%COT_CONFIG%') -replace '\"cot_enabled\": true', '\"cot_enabled\": false' | Set-Content '%COT_CONFIG%'"
powershell -Command "(Get-Content '%ROLLBACK_CONFIG%') -replace '\"rollback_active\": false', '\"rollback_active\": true' | Set-Content '%ROLLBACK_CONFIG%'"
taskkill /F /IM python.exe /FI "WINDOWTITLE eq rawrxd_cot*" 2>nul
echo [RawrXD] Rollback complete. CoT is disabled. IDE is fully usable.
echo [RawrXD] To re-enable: cot_toggle.bat enable
goto :done

:usage
echo Usage: cot_toggle.bat ^<enable^|disable^|status^|rollback^>
echo.
echo   enable   — Enable CoT reasoning pipeline
echo   disable  — Disable CoT (IDE stays fully usable, prompts go to LLM directly)
echo   status   — Check current CoT configuration and backend status
echo   rollback — Full rollback: disable CoT + kill Python backend
goto :done

:done
endlocal
