@echo off
REM Simple build wrapper for BotBuilder using VS 2022

setlocal

REM Set paths
set "VS_PATH=D:\Microsoft Visual Studio 2022"
set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
set "BOTBUILDER_PATH=%CD%"

echo.
echo ========== BotBuilder Build ==========
echo.
echo VS Path: %VS_PATH%
echo Build Path: %BOTBUILDER_PATH%
echo.

REM Initialize environment
echo [1/3] Initializing VS environment...
call "%VCVARS%" x86_amd64
if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment
    exit /b 1
)
echo OK
echo.

REM Clean
echo [2/3] Cleaning previous builds...
if exist "bin" rmdir /s /q "bin"
if exist "obj" rmdir /s /q "obj"
echo OK
echo.

REM Build
echo [3/3] Building BotBuilder...
msbuild "BotBuilder.sln" /p:Configuration=Debug /p:Platform="Any CPU" /nologo /v:minimal
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)
echo OK
echo.

REM Check result
if exist "BotBuilder\bin\Debug\BotBuilder.exe" (
    echo.
    echo ========== BUILD SUCCESS ==========
    echo.
    echo Executable: BotBuilder\bin\Debug\BotBuilder.exe
    echo.
    echo Run it:
    echo   BotBuilder\bin\Debug\BotBuilder.exe
    echo.
    exit /b 0
) else if exist "bin\Debug\BotBuilder.exe" (
    echo.
    echo ========== BUILD SUCCESS ==========
    echo.
    echo Executable: bin\Debug\BotBuilder.exe
    echo.
    echo Run it:
    echo   bin\Debug\BotBuilder.exe
    echo.
    exit /b 0
) else (
    echo ERROR: Executable not found
    echo Searching...
    dir /s /b bin\*BotBuilder*
    exit /b 1
)
