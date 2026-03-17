@echo off
REM Build script for RawrXD Titan IDE (CLI + GUI)
REM This builds the complete Titan ring buffer stack

setlocal EnableDelayedExpansion

echo ============================================================
echo RawrXD Titan Build - 64MB DMA Ring + AVX-512
echo ============================================================

REM Set paths
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "SRC_DIR=D:\RawrXD\src"
set "SHIP_DIR=D:\RawrXD\Ship"
set "OUT_DIR=D:\RawrXD\bin"

REM Create output directory if needed
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM Initialize Visual Studio environment
echo [1/5] Initializing Visual Studio 2022 x64 environment...
call "%VCVARS%"
if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment from BuildTools
    exit /b 1
)

echo.
echo [2/5] Assembling Titan Streaming Orchestrator...
ml64 /c /Zi /Fo"%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" "%SRC_DIR%\Titan_Streaming_Orchestrator_Fixed.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble orchestrator
    exit /b 1
)
echo     OK: Titan_Streaming_Orchestrator_Fixed.obj

echo.
echo [3/5] Assembling CLI Consumer...
ml64 /c /Zi /Fo"%OUT_DIR%\RawrXD_CLI_Titan.obj" "%SHIP_DIR%\RawrXD_CLI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble CLI
    exit /b 1
)
echo     OK: RawrXD_CLI_Titan.obj

echo.
echo [4/5] Assembling GUI IDE...
ml64 /c /Zi /Fo"%OUT_DIR%\RawrXD_GUI_Titan.obj" "%SHIP_DIR%\RawrXD_GUI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble GUI
    exit /b 1
)
echo     OK: RawrXD_GUI_Titan.obj

echo.
echo [5/5] Linking executables...

REM Link CLI (console subsystem)
link /SUBSYSTEM:CONSOLE /OUT:"%OUT_DIR%\RawrXD-CLI.exe" ^
    /DEBUG /PDB:"%OUT_DIR%\RawrXD-CLI.pdb" ^
    "%OUT_DIR%\RawrXD_CLI_Titan.obj" ^
    "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" ^
    kernel32.lib user32.lib ws2_32.lib
if errorlevel 1 (
    echo ERROR: Failed to link CLI
    exit /b 1
)
echo     OK: RawrXD-CLI.exe

REM Link GUI (windows subsystem)
link /SUBSYSTEM:WINDOWS /OUT:"%OUT_DIR%\RawrXD-IDE.exe" ^
    /DEBUG /PDB:"%OUT_DIR%\RawrXD-IDE.pdb" ^
    "%OUT_DIR%\RawrXD_GUI_Titan.obj" ^
    "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" ^
    kernel32.lib user32.lib gdi32.lib comctl32.lib
if errorlevel 1 (
    echo ERROR: Failed to link GUI
    exit /b 1
)
echo     OK: RawrXD-IDE.exe

echo.
echo ============================================================
echo BUILD COMPLETE
echo ============================================================
echo.
echo Output files:
echo   CLI: %OUT_DIR%\RawrXD-CLI.exe
echo   GUI: %OUT_DIR%\RawrXD-IDE.exe
echo.
echo Architecture:
echo   - 64MB Memory-Mapped Ring Buffer
echo   - AVX-512 Non-Temporal Streaming
echo   - Lock-Free Michael-Scott Queue
echo   - SRW Lock Synchronization
echo   - High-Level Titan API Exports
echo.

dir /b "%OUT_DIR%\*.exe" 2>nul
echo.
echo Run 'RawrXD-CLI.exe' for headless inference
echo Run 'RawrXD-IDE.exe' for graphical IDE
echo ============================================================

endlocal
