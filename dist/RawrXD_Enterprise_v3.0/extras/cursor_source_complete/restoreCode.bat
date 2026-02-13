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

REM Check if codeBinDir is in PATH
echo !current_path! | findstr /i /c:"%codeBinDir%" >nul
if errorlevel 1 (
    echo %codeBinDir% is not in PATH, nothing to remove
    exit /b 0
)

REM Remove codeBinDir from PATH using a more robust approach
REM Parse PATH into individual entries and reconstruct without codeBinDir
set "new_path="
set "first_entry=true"

REM Loop through each PATH entry
for %%p in ("!current_path:;=" "!") do (
    set "entry=%%~p"

    REM Skip empty entries and the codeBinDir entry
    if not "!entry!"=="" (
        if /i not "!entry!"=="%codeBinDir%" (
            REM Add semicolon separator except for the first entry
            if "!first_entry!"=="true" (
                set "new_path=!entry!"
                set "first_entry=false"
            ) else (
                set "new_path=!new_path!;!entry!"
            )
        )
    )
)

REM Update registry with new PATH
reg add HKCU\Environment /v Path /t REG_EXPAND_SZ /d "!new_path!" /f >nul 2>&1
if errorlevel 1 (
    echo Error: Failed to update PATH in registry
    exit /b 1
)

echo Successfully removed %codeBinDir% from PATH
