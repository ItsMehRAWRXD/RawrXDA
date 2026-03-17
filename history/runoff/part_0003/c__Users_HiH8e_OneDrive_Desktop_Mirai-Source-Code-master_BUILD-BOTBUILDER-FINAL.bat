@echo off
REM =============================================================================
REM  Launch VS 2022 Developer Command Prompt + Build BotBuilder
REM  This script properly invokes the Developer Command Prompt environment
REM =============================================================================

setlocal enabledelayedexpansion

echo.
echo ================================================================================
echo  VS 2022 Developer Command Prompt - BotBuilder Build Initialization
echo ================================================================================
echo.

REM Find VS 2022 installation
REM Checking multiple possible locations
set VS_INSTALL=D:\Microsoft Visual Studio 2022
set VCVARS=%VS_INSTALL%\VC\Auxiliary\Build\vcvarsall.bat

if not exist "%VCVARS%" (
    REM Try other location
    set VS_INSTALL=C:\Program Files\Microsoft Visual Studio 2022
    set VCVARS=!VS_INSTALL!\VC\Auxiliary\Build\vcvarsall.bat
)

if not exist "%VCVARS%" (
    echo [ERROR] Visual Studio 2022 not found at: %VS_INSTALL%
    echo.
    echo Searching for VS 2022 installation...
    for /d %%D in (C:\Program Files\Microsoft Visual Studio 2022\* D:\Microsoft Visual Studio 2022\*) do (
        if exist "%%D\VC\Auxiliary\Build\vcvarsall.bat" (
            set VCVARS=%%D\VC\Auxiliary\Build\vcvarsall.bat
            set VS_INSTALL=%%D
            echo Found: !VS_INSTALL!
            goto :found_vs
        )
    )
    echo [ERROR] Visual Studio 2022 not found
    exit /b 1
)

:found_vs
echo [OK] Found VS 2022 at: %VS_INSTALL%
echo [OK] VCVars: %VCVARS%
echo.

REM Initialize Visual Studio environment
echo [STEP 1/5] Initializing Visual Studio environment...
call "%VCVARS%" x86_amd64
if errorlevel 1 (
    echo [WARNING] vcvarsall.bat returned non-zero, but environment may still be set
)
echo.

REM Verify tools are available
echo [STEP 2/5] Verifying build tools...
where msbuild >nul 2>&1
if errorlevel 1 (
    echo [ERROR] MSBuild not found in PATH
    exit /b 1
)
echo [OK] MSBuild found
where nuget >nul 2>&1
if errorlevel 1 (
    echo [WARNING] NuGet not found, continuing anyway
) else (
    echo [OK] NuGet found
)
echo.

REM Build BotBuilder
set BOTBUILDER_PATH=c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder
set SOLUTION_FILE=%BOTBUILDER_PATH%\BotBuilder.sln

echo [STEP 3/5] Cleaning previous builds...
cd /d "%BOTBUILDER_PATH%"
if exist "bin" rmdir /s /q "bin" >nul 2>&1
if exist "obj" rmdir /s /q "obj" >nul 2>&1
echo [OK] Cleaned
echo.

echo [STEP 4/5] Building solution with MSBuild...
echo   Solution: %SOLUTION_FILE%
echo.
msbuild "%SOLUTION_FILE%" /p:Configuration=Release /p:Platform="x86" /v:minimal /nologo
if errorlevel 1 (
    echo [WARNING] First build attempt failed, trying Debug configuration...
    msbuild "%SOLUTION_FILE%" /p:Configuration=Debug /p:Platform="Any CPU" /v:minimal /nologo
    if errorlevel 1 (
        echo [ERROR] Build failed with both Release and Debug configurations
        exit /b 1
    )
)
echo [OK] Build succeeded
echo.

echo [STEP 5/5] Verifying build artifacts...
if exist "bin\Debug\BotBuilder.exe" (
    echo [OK] Found: bin\Debug\BotBuilder.exe
    set EXE_PATH=bin\Debug\BotBuilder.exe
) else if exist "bin\Release\BotBuilder.exe" (
    echo [OK] Found: bin\Release\BotBuilder.exe
    set EXE_PATH=bin\Release\BotBuilder.exe
) else (
    echo [ERROR] Executable not found
    echo Checking what was built...
    dir /s /b "bin\*BotBuilder*"
    exit /b 1
)
echo.

echo ================================================================================
echo  BUILD SUCCESSFUL!
echo ================================================================================
echo.
echo Application: %EXE_PATH%
echo Full path: %CD%\%EXE_PATH%
echo.
echo Test Instructions:
echo   1. Run: %EXE_PATH%
echo   2. Test Configuration Tab: Type in Bot Name, C2 Server, C2 Port
echo   3. Test Advanced Tab: Check/uncheck checkboxes
echo   4. Test Build Tab: Click BUILD button, watch progress
echo   5. Test Preview Tab: Verify size, hash, score appear
echo   6. Test Exit Button: Close application
echo.
echo If all tests pass:
echo   git add Projects/BotBuilder/
echo   git commit -m "Phase 3 Task 2: BotBuilder GUI verified"
echo   git push origin phase3-botbuilder-gui
echo.
pause
