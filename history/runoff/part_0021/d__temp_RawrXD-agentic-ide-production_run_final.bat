@echo off
setlocal EnableDelayedExpansion

:: Quick-run wrapper: builds static loader, then launches IDE with the loader and selected model
for /f "delims=" %%E in ('"prompt $E$E$E$E & for %%A in (1) do rem"') do set "ESC=%%E"
set "GREEN=%ESC%[92m"
set "RED=%ESC%[91m"
set "YELLOW=%ESC%[93m"
set "BLUE=%ESC%[94m"
set "RESET=%ESC%[0m"

set "PROJECT_ROOT=D:\temp\RawrXD-agentic-ide-production"
set "BIN_DIR=%PROJECT_ROOT%\build-sovereign-static\bin"
set "IDE_DIR=%PROJECT_ROOT%\RawrXD-IDE\release"

set "MODEL=%~1"
if "%MODEL%"=="" set "MODEL=%PROJECT_ROOT%\RawrXD-ModelLoader\phi-3-mini.gguf"

if not exist "%MODEL%" (
    echo %YELLOW%⚠ Model not found at "%MODEL%"%RESET%
    echo    Usage: run_final.bat path\to\model.gguf
    exit /b 1
)

echo %BLUE%[1/2] Building static loader and IDE...%RESET%
call "%PROJECT_ROOT%\build_static_final.bat"
if errorlevel 1 (
    echo %RED%❌ Build failed; aborting run%RESET%
    exit /b 1
)

echo %BLUE%[2/2] Launching RawrXD IDE with secure loader...%RESET%
set "PATH=%BIN_DIR%;%PATH%"
if not exist "%IDE_DIR%\RawrXD-IDE.exe" (
    echo %RED%❌ RawrXD-IDE.exe not found in %IDE_DIR%%RESET%
    exit /b 1
)
pushd "%IDE_DIR%"
"RawrXD-IDE.exe" --loader "%BIN_DIR%\RawrXD-SovereignLoader.dll" --model "%MODEL%" --mode secure
set "LAUNCH_RC=%ERRORLEVEL%"
popd

if not "%LAUNCH_RC%"=="0" (
    echo %RED%❌ IDE exited with code %LAUNCH_RC%%RESET%
    exit /b %LAUNCH_RC%
)

echo %GREEN%✅ Run complete. Loader and IDE executed successfully.%RESET%
exit /b 0
