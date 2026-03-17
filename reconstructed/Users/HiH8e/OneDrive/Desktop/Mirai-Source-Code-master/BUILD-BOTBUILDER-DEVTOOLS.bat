@echo off
REM =============================================================================
REM  VS 2022 Cross Tools Developer Command Prompt - BotBuilder Build
REM  Location: D:\~dev\sdk\x86_x64 Cross Tools Command Prompt for VS 2022 (2).lnk
REM =============================================================================

setlocal enabledelayedexpansion

set BOTBUILDER_PATH=c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder
set SOLUTION_FILE=%BOTBUILDER_PATH%\BotBuilder.sln

echo.
echo ================================================================================
echo  BotBuilder Build via VS 2022 Cross Tools Command Prompt
echo ================================================================================
echo.

REM Check if solution exists
if not exist "%SOLUTION_FILE%" (
    echo [ERROR] Solution file not found: %SOLUTION_FILE%
    exit /b 1
)

echo [INFO] Solution: %SOLUTION_FILE%
echo.

REM Change to solution directory
cd /d "%BOTBUILDER_PATH%"
if errorlevel 1 (
    echo [ERROR] Failed to change directory to %BOTBUILDER_PATH%
    exit /b 1
)

REM Clean previous builds
echo [STEP 1/4] Cleaning previous build artifacts...
if exist "bin" (
    rmdir /s /q "bin" >nul 2>&1
    echo [OK] Removed bin folder
)
if exist "obj" (
    rmdir /s /q "obj" >nul 2>&1
    echo [OK] Removed obj folder
)
echo.

REM Restore NuGet packages
echo [STEP 2/4] Restoring NuGet packages...
nuget restore "%SOLUTION_FILE%"
if errorlevel 1 (
    echo [WARNING] NuGet restore had issues, continuing anyway...
)
echo.

REM Build the solution
echo [STEP 3/4] Building solution...
msbuild "%SOLUTION_FILE%" /p:Configuration=Debug /p:Platform="Any CPU" /v:minimal
if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo [OK] Build succeeded
echo.

REM Check for build artifacts
echo [STEP 4/4] Verifying build artifacts...
if exist "bin\Debug\BotBuilder.exe" (
    echo [OK] Build executable created: bin\Debug\BotBuilder.exe
    echo.
    echo ================================================================================
    echo  BUILD SUCCESSFUL!
    echo ================================================================================
    echo.
    echo To run the application:
    echo   - Option 1 (Direct): bin\Debug\BotBuilder.exe
    echo   - Option 2 (From VS): Press F5 in Visual Studio
    echo.
    echo To test:
    echo   1. Run: bin\Debug\BotBuilder.exe
    echo   2. Test all 4 tabs: Configuration, Advanced, Build, Preview
    echo   3. Click BUILD button, watch progress bar
    echo   4. Verify Preview tab populates with results
    echo   5. Test Exit button
    echo.
    exit /b 0
) else (
    echo [ERROR] Build executable not found at expected location
    exit /b 1
)
