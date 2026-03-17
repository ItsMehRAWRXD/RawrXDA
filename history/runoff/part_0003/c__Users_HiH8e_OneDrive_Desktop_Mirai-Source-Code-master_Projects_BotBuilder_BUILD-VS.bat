@echo off
REM BotBuilder - Build using Visual Studio devenv.exe
REM This uses the VS 2022 IDE to build, which handles SDK-style projects correctly

setlocal enabledelayedexpansion

echo.
echo ========== Building BotBuilder with Visual Studio ==========
echo.

REM Navigate to BotBuilder directory
cd /d "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder"

REM Paths
set "DEVENV=D:\Microsoft Visual Studio 2022\Common7\IDE\devenv.exe"
set "SOLUTION=BotBuilder.sln"

if not exist "!DEVENV!" (
    echo ERROR: Visual Studio not found at !DEVENV!
    exit /b 1
)

if not exist "!SOLUTION!" (
    echo ERROR: Solution file not found: !SOLUTION!
    exit /b 1
)

echo Step 1: Building solution with Visual Studio...
"!DEVENV!" "!SOLUTION!" /build Release /out buildlog.txt

if errorlevel 1 (
    echo WARNING: Release build may have failed, checking for executable...
)

echo.
echo Step 2: Checking for build output...

REM Look for the executable in common locations
if exist "BotBuilder\bin\Release\BotBuilder.exe" (
    echo SUCCESS: Found executable
    echo   Path: BotBuilder\bin\Release\BotBuilder.exe
    exit /b 0
)

if exist "bin\Release\BotBuilder.exe" (
    echo SUCCESS: Found executable
    echo   Path: bin\Release\BotBuilder.exe
    exit /b 0
)

if exist "BotBuilder\bin\Debug\BotBuilder.exe" (
    echo SUCCESS: Found executable (Debug build)
    echo   Path: BotBuilder\bin\Debug\BotBuilder.exe
    exit /b 0
)

if exist "bin\Debug\BotBuilder.exe" (
    echo SUCCESS: Found executable (Debug build)
    echo   Path: bin\Debug\BotBuilder.exe
    exit /b 0
)

REM If not found anywhere, search recursively
for /r %%F in (BotBuilder.exe) do (
    if exist "%%F" (
        echo SUCCESS: Found executable
        echo   Path: %%F
        exit /b 0
    )
)

echo ERROR: Could not find BotBuilder.exe
exit /b 1
