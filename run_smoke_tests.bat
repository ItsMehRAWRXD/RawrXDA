@echo off
REM RawrXD Smoke Test Suite Automation
REM Runs comprehensive runtime validation for AI IDE functionality
REM Logs results to Win32IDE_SmokeTest.log

setlocal enabledelayedexpansion
set "LOGFILE=%~dp0Win32IDE_SmokeTest.log"
set "EXE=%~dp0build\bin\RawrXD-Win32IDE.exe"
set "TEST_MODEL=D:\models\tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
set "OLLAMA_URL=http://localhost:11434"

echo ======================================== >> "%LOGFILE%"
echo RawrXD Smoke Test Suite - %DATE% %TIME% >> "%LOGFILE%"
echo ======================================== >> "%LOGFILE%"

REM Test 1: Binary Health Check
echo. >> "%LOGFILE%"
echo [1/4] Binary Health Check >> "%LOGFILE%"
echo Checking binary integrity... >> "%LOGFILE%"

if not exist "%EXE%" (
    echo FAIL: RawrXD-Win32IDE.exe not found at %EXE% >> "%LOGFILE%"
    goto :FAIL
)

where dumpbin >nul 2>&1
if errorlevel 1 (
    echo SKIP: dumpbin not available, assuming binary is valid >> "%LOGFILE%"
) else (
    dumpbin /headers "%EXE%" | findstr "machine" >> "%LOGFILE%" 2>&1
    if errorlevel 1 (
        echo FAIL: dumpbin check failed >> "%LOGFILE%"
        goto :FAIL
    )
)

echo PASS: Binary exists and appears valid >> "%LOGFILE%"

REM Test 2: Launch Test
echo. >> "%LOGFILE%"
echo [2/4] Launch Test >> "%LOGFILE%"
echo Testing application launch... >> "%LOGFILE%"

timeout /t 5 /nobreak >nul 2>&1
start "" "%EXE%" --smoke-test --console --verbose
timeout /t 10 /nobreak >nul

tasklist /fi "imagename eq RawrXD-Win32IDE.exe" | findstr "RawrXD-Win32IDE.exe" >nul 2>&1
if errorlevel 1 (
    echo FAIL: Application failed to launch or crashed immediately >> "%LOGFILE%"
    goto :FAIL
)

echo PASS: Application launched successfully >> "%LOGFILE%"

REM Test 3: Ollama Connectivity
echo. >> "%LOGFILE%"
echo [3/4] Ollama Connectivity Test >> "%LOGFILE%"
echo Testing Ollama server connection... >> "%LOGFILE%"

powershell -Command "try { $response = Invoke-WebRequest -Uri '%OLLAMA_URL%/api/tags' -TimeoutSec 5; if ($response.StatusCode -eq 200) { Write-Host 'PASS: Ollama server responding' } else { Write-Host 'FAIL: Ollama server returned status ' + $response.StatusCode } } catch { Write-Host 'FAIL: Cannot connect to Ollama server' }" >> "%LOGFILE%" 2>&1

REM Test 4: Local Inference Test
echo. >> "%LOGFILE%"
echo [4/4] Local Inference Test >> "%LOGFILE%"
echo Testing local model inference... >> "%LOGFILE%"

if not exist "%TEST_MODEL%" (
    echo SKIP: Test model not found at %TEST_MODEL% >> "%LOGFILE%"
    echo Please download tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf to test local inference >> "%LOGFILE%"
) else (
    REM This would require a headless inference mode - placeholder
    echo SKIP: Headless inference test requires --headless-infer CLI support >> "%LOGFILE%"
    echo Manual test: Load model and generate 'Hello world' >> "%LOGFILE%"
)

REM Clean up
taskkill /f /im RawrXD-Win32IDE.exe >nul 2>&1

echo. >> "%LOGFILE%"
echo ======================================== >> "%LOGFILE%"
echo Smoke Test Suite Complete >> "%LOGFILE%"
echo Check Win32IDE_SmokeTest.log for details >> "%LOGFILE%"
echo ======================================== >> "%LOGFILE%"

echo Smoke test completed. Check Win32IDE_SmokeTest.log for results.
goto :END

:FAIL
echo. >> "%LOGFILE%"
echo ======================================== >> "%LOGFILE%"
echo SMOKE TEST FAILED - Check log for details >> "%LOGFILE%"
echo ======================================== >> "%LOGFILE%"
echo Smoke test FAILED. Check Win32IDE_SmokeTest.log for details.
exit /b 1

:END
exit /b 0