@echo off
REM Build script for RawrXD Titan IDE (CLI + GUI)
REM MetaReverse Build - No kernel32.lib required! Uses PEB walking.

setlocal EnableDelayedExpansion

echo ============================================================
echo RawrXD Titan Build - MetaReverse (Zero Static Imports)
echo ============================================================
echo Architecture: 64MB DMA Ring + AVX-512 + PEB Walking
echo ============================================================

REM Set paths
set "SRC_DIR=D:\RawrXD\src"
set "SHIP_DIR=D:\RawrXD\Ship"
set "OUT_DIR=D:\RawrXD\bin"

REM Create output directory if needed
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM Find ml64.exe anywhere (VS Build Tools, VS Community, etc.)
echo [1/6] Locating ml64.exe...
set "ML64="
for /f "delims=" %%i in ('where ml64.exe 2^>nul') do (
    if not defined ML64 set "ML64=%%i"
)
if not defined ML64 (
    for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\Microsoft Visual Studio\*\ml64.exe" 2^>nul') do (
        if not defined ML64 set "ML64=%%i"
    )
)
if not defined ML64 (
    echo ERROR: ml64.exe not found. Install Visual Studio Build Tools.
    exit /b 1
)
echo     Found: %ML64%

REM Find link.exe
set "LINK_EXE="
for /f "delims=" %%i in ('where link.exe 2^>nul') do (
    if not defined LINK_EXE set "LINK_EXE=%%i"
)
if not defined LINK_EXE (
    for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\Microsoft Visual Studio\*\link.exe" 2^>nul ^| findstr /i "x64"') do (
        if not defined LINK_EXE set "LINK_EXE=%%i"
    )
)
if not defined LINK_EXE (
    echo ERROR: link.exe not found.
    exit /b 1
)
echo     Found: %LINK_EXE%

echo.
echo [2/6] Assembling Titan Streaming Orchestrator...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" "%SRC_DIR%\Titan_Streaming_Orchestrator_Fixed.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble orchestrator
    exit /b 1
)
echo     OK: Titan_Streaming_Orchestrator_Fixed.obj

echo.
echo [3/6] Assembling MetaReverse Bootstrap...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\RawrXD_Titan_MetaReverse.obj" "%SHIP_DIR%\RawrXD_Titan_MetaReverse.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble MetaReverse
    exit /b 1
)
echo     OK: RawrXD_Titan_MetaReverse.obj

echo.
echo [4/6] Assembling CLI Consumer...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\RawrXD_CLI_Titan.obj" "%SHIP_DIR%\RawrXD_CLI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble CLI
    exit /b 1
)
echo     OK: RawrXD_CLI_Titan.obj

echo.
echo [5/6] Assembling GUI IDE...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\RawrXD_GUI_Titan.obj" "%SHIP_DIR%\RawrXD_GUI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble GUI
    exit /b 1
)
echo     OK: RawrXD_GUI_Titan.obj

echo.
echo [6/6] Linking executables (NODEFAULTLIB - Zero Static Imports)...

REM Link CLI (console subsystem, no libs needed)
"%LINK_EXE%" /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:main ^
    /DEBUG /PDB:"%OUT_DIR%\RawrXD-CLI.pdb" ^
    /OUT:"%OUT_DIR%\RawrXD-CLI.exe" ^
    "%OUT_DIR%\RawrXD_CLI_Titan.obj" ^
    "%OUT_DIR%\RawrXD_Titan_MetaReverse.obj" ^
    "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj"
if errorlevel 1 (
    echo ERROR: Failed to link CLI
    exit /b 1
)
echo     OK: RawrXD-CLI.exe

REM Link GUI (windows subsystem, no libs needed)
"%LINK_EXE%" /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:WinMain ^
    /DEBUG /PDB:"%OUT_DIR%\RawrXD-IDE.pdb" ^
    /OUT:"%OUT_DIR%\RawrXD-IDE.exe" ^
    "%OUT_DIR%\RawrXD_GUI_Titan.obj" ^
    "%OUT_DIR%\RawrXD_Titan_MetaReverse.obj" ^
    "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj"
if errorlevel 1 (
    echo ERROR: Failed to link GUI
    exit /b 1
)
echo     OK: RawrXD-IDE.exe

echo.
echo ============================================================
echo BUILD COMPLETE - MetaReverse Mode
echo ============================================================
echo.
echo Output files:
echo   CLI: %OUT_DIR%\RawrXD-CLI.exe
echo   GUI: %OUT_DIR%\RawrXD-IDE.exe
echo.
echo Architecture:
echo   - 64MB Memory-Mapped Ring Buffer
echo   - AVX-512 Non-Temporal Streaming
echo   - PEB Walking API Resolution (Zero Imports)
echo   - Lock-Free Michael-Scott Queue
echo   - High-Level Titan API Exports
echo.

dir /b "%OUT_DIR%\*.exe" 2>nul
echo.
echo Run 'RawrXD-CLI.exe' for headless inference
echo Run 'RawrXD-IDE.exe' for graphical IDE
echo ============================================================

endlocal
