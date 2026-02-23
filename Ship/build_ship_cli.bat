@echo off
REM build_ship_cli.bat - Build RawrXD_CLI.exe (full chat + agentic, 100%% Win32 parity)
REM Uses CMake. Output: build\Release\RawrXD_CLI.exe

setlocal
cd /d "%~dp0"

echo ================================================
echo  RawrXD Ship CLI Build (Full Agentic)
echo ================================================
echo.

REM Try vcvars if not in VS environment
if not defined VCINSTALLDIR (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul`) do set "VSINSTALL=%%i"
    if defined VSINSTALL (
        call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    )
)

if not exist build mkdir build
cd build

echo Configuring...
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -A x64 2>nul
if errorlevel 1 (
    cmake .. -DCMAKE_BUILD_TYPE=Release -G "Ninja" -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl 2>nul
)

echo Building RawrXD_CLI...
cmake --build . --target RawrXD_CLI --config Release
if errorlevel 1 (
    echo.
    echo [WARN] RawrXD_CLI target failed. Trying RawrXD_Agent_Console...
    cmake --build . --target RawrXD_Agent_Console --config Release
    if errorlevel 1 (
        echo BUILD FAILED
        exit /b 1
    )
    if exist Release\RawrXD_Agent_Console.exe (
        copy Release\RawrXD_Agent_Console.exe Release\RawrXD_CLI.exe
        echo Copied RawrXD_Agent_Console.exe as RawrXD_CLI.exe
    )
)

echo.
echo ================================================
echo  Build complete
echo ================================================
if exist Release\RawrXD_CLI.exe (
    echo Output: %CD%\Release\RawrXD_CLI.exe
    dir Release\RawrXD_CLI.exe
) else if exist RawrXD_CLI.exe (
    echo Output: %CD%\RawrXD_CLI.exe
    dir RawrXD_CLI.exe
) else (
    echo Output location may vary by generator. Check build directory.
)
echo.
endlocal
