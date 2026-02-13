@echo off
setlocal enabledelayedexpansion

REM Check if codeBinDir is provided
if "%codeBinDir%"=="" (
    echo Error: codeBinDir environment variable is not set
    exit /b 1
)

REM Create temporary file in temp directory
set "temp_file=%TEMP%\vscode_path_update_%RANDOM%.txt"

REM Query current PATH from registry
reg query HKCU\Environment /v PATH > "%temp_file%" 2>&1
if errorlevel 1 (
    echo Error: Failed to query PATH from registry
    if exist "%temp_file%" del "%temp_file%"
    exit /b 1
)

REM Read current PATH value
set "current_path="
for /F "tokens=2,*" %%A in ('type "%temp_file%"') do set "current_path=%%B"

REM Clean up temp file
if exist "%temp_file%" del "%temp_file%"

REM Check if we got a valid PATH
if "!current_path!"=="" (
    echo Error: Could not retrieve current PATH from registry
    exit /b 1
)

REM Check if codeBinDir is already in PATH
echo !current_path! | findstr /i /c:"%codeBinDir%" >nul
if not errorlevel 1 (
    echo %codeBinDir% is already in PATH
    exit /b 0
)

REM Build new PATH with codeBinDir at the beginning
set "new_path=%codeBinDir%;!current_path!"

REM Update registry with new PATH
reg add HKCU\Environment /v Path /t REG_EXPAND_SZ /d "!new_path!" /f >nul 2>&1
if errorlevel 1 (
    echo Error: Failed to update PATH in registry
    exit /b 1
)

echo Successfully added %codeBinDir% to PATH
