@echo off
REM BotBuilder GUI - Build using available .NET SDK

setlocal enabledelayedexpansion

echo.
echo ========== BotBuilder Build (.NET SDK) ==========
echo.

REM Navigate to project directory
cd /d "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder"

REM Try to find working dotnet
echo Step 1: Locating .NET SDK...
set "DOTNET_PATH="

REM Try locations in order
if exist "D:\13-Recovery-Files\dotnet.exe" (
    set "DOTNET_PATH=D:\13-Recovery-Files\dotnet.exe"
    echo ✅ Found dotnet at: !DOTNET_PATH!
    goto :found_dotnet
)

if exist "D:\Organized\12-Archives-Backups\dotnet.exe" (
    set "DOTNET_PATH=D:\Organized\12-Archives-Backups\dotnet.exe"
    echo ✅ Found dotnet at: !DOTNET_PATH!
    goto :found_dotnet
)

where dotnet >nul 2>&1
if errorlevel 0 (
    set "DOTNET_PATH=dotnet"
    echo ⚠️  Using system dotnet (may fail if hostfxr.dll is missing)
    goto :found_dotnet
)

echo ❌ .NET SDK not found
exit /b 1

:found_dotnet
echo.

REM Check if solution exists
if not exist "BotBuilder.sln" (
    echo ❌ Solution file not found!
    exit /b 1
)

REM Clean
echo Step 2: Cleaning previous builds...
if exist "BotBuilder\bin" rmdir /s /q "BotBuilder\bin" 2>nul
if exist "BotBuilder\obj" rmdir /s /q "BotBuilder\obj" 2>nul
if exist "bin" rmdir /s /q "bin" 2>nul
if exist "obj" rmdir /s /q "obj" 2>nul
echo ✅ Clean complete
echo.

REM Build
echo Step 3: Building with .NET...
"!DOTNET_PATH!" build BotBuilder.csproj -c Debug -v minimal
if errorlevel 1 (
    echo ❌ Build failed
    exit /b 1
)
echo ✅ Build successful
echo.

REM Find executable
echo Step 4: Locating executable...
if exist "BotBuilder\bin\Debug\net48\BotBuilder.exe" (
    set "EXE_PATH=BotBuilder\bin\Debug\net48\BotBuilder.exe"
) else if exist "bin\Debug\net48\BotBuilder.exe" (
    set "EXE_PATH=bin\Debug\net48\BotBuilder.exe"
) else if exist "BotBuilder\bin\Debug\BotBuilder.exe" (
    set "EXE_PATH=BotBuilder\bin\Debug\BotBuilder.exe"
) else if exist "bin\Debug\BotBuilder.exe" (
    set "EXE_PATH=bin\Debug\BotBuilder.exe"
) else (
    echo ❌ Executable not found
    echo.
    echo Searching for BotBuilder.exe...
    dir /s /b *.exe 2>nul | findstr /i botbuilder
    exit /b 1
)
echo ✅ Found: !EXE_PATH!
echo.

REM Run application
echo Step 5: Launching BotBuilder...
echo.
start "" "!EXE_PATH!"

echo ✅ Application launched
echo.
pause

if errorlevel 1 (
    echo ❌ Build failed
    exit /b 1
)
echo ✅ Build successful
echo.

REM Find executable
echo Step 4: Locating executable...
if exist "BotBuilder\bin\Debug\BotBuilder.exe" (
    set "EXE_PATH=BotBuilder\bin\Debug\BotBuilder.exe"
) else if exist "bin\Debug\BotBuilder.exe" (
    set "EXE_PATH=bin\Debug\BotBuilder.exe"
) else (
    echo ❌ Executable not found
    dir /s /b *BotBuilder.exe
    exit /b 1
)
echo ✅ Found: !EXE_PATH!
echo.

REM Run application
echo Step 5: Launching BotBuilder...
echo.
call "!EXE_PATH!"

echo.
echo ✅ Application closed
echo.
echo Build Summary:
echo   - Build: SUCCESS
echo   - Executable: !EXE_PATH!
echo.
pause
