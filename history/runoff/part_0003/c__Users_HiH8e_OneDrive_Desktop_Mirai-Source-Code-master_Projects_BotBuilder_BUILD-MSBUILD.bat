@echo off
REM BotBuilder - Build using VS 2022 Developer Command Prompt

setlocal enabledelayedexpansion

echo.
echo ========== BotBuilder Build (VS 2022) ==========
echo.

cd /d "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder"

REM Initialize VS environment first
echo Initializing Visual Studio environment...
call "D:\Microsoft Visual Studio 2022\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64 >nul 2>&1
echo ✅ Environment ready
echo.

REM Clean
echo Step 1: Cleaning previous builds...
if exist "bin" rmdir /s /q "bin" 2>nul
if exist "obj" rmdir /s /q "obj" 2>nul
echo ✅ Clean complete
echo.

REM Try building with msbuild (should now be in PATH)
echo Step 2: Building with MSBuild...
msbuild "BotBuilder.sln" /p:Configuration=Release /p:Platform="Any CPU" /v:minimal /nologo
if not errorlevel 1 goto :build_ok

echo ⚠️  Release build failed, trying Debug...
msbuild "BotBuilder.sln" /p:Configuration=Debug /p:Platform="Any CPU" /v:minimal /nologo
if not errorlevel 1 goto :build_ok

echo ❌ Build failed
exit /b 1

:build_ok
echo ✅ Build successful
echo.

REM Find the executable
echo Step 3: Locating executable...
set "EXE_PATH="

for /r %%F in (BotBuilder.exe) do (
    if exist "%%F" (
        set "EXE_PATH=%%F"
        goto :found_exe
    )
)

echo ❌ Executable not found
exit /b 1

:found_exe
echo ✅ Found: !EXE_PATH!
echo.

REM Run the application
echo Step 4: Launching BotBuilder...
start "" "!EXE_PATH!"
echo ✅ BotBuilder launched
echo.
echo ================================================
echo Build successful!
echo Executable: !EXE_PATH!
echo ================================================
echo.

pause
