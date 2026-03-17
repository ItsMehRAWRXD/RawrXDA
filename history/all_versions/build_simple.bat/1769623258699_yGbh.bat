@echo off
REM Build script for RawrXD Titan IDE - Uses VS Developer Environment
REM This script invokes itself through cmd to preserve VS environment

setlocal EnableDelayedExpansion

echo ============================================================
echo RawrXD Titan Build - 64MB DMA Ring + AVX-512
echo ============================================================

set "SRC_DIR=D:\RawrXD\src"
set "SHIP_DIR=D:\RawrXD\Ship"
set "OUT_DIR=D:\RawrXD\bin"
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [1/6] Initializing Visual Studio 2022 x64 environment...
call "%VCVARS%"
if errorlevel 1 (
    echo ERROR: Could not initialize VS environment
    exit /b 1
)

echo.
echo [2/6] Assembling Titan Streaming Orchestrator...
ml64 /c /Zi /Fo"%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" "%SRC_DIR%\Titan_Streaming_Orchestrator_Fixed.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble orchestrator
    exit /b 1
)
echo     OK: Titan_Streaming_Orchestrator_Fixed.obj

echo.
echo [3/6] Assembling Titan Unified Core...
if exist "%SRC_DIR%\RawrXD_Titan_UNIFIED.asm" (
    ml64 /c /Zi /Fo"%OUT_DIR%\RawrXD_Titan_UNIFIED.obj" "%SRC_DIR%\RawrXD_Titan_UNIFIED.asm"
    if errorlevel 1 (
        echo WARNING: UNIFIED assembly failed, trying InferenceCore...
        if exist "%SRC_DIR%\Titan_InferenceCore.asm" (
            ml64 /c /Zi /Fo"%OUT_DIR%\Titan_InferenceCore.obj" "%SRC_DIR%\Titan_InferenceCore.asm"
        )
    ) else (
        echo     OK: RawrXD_Titan_UNIFIED.obj
    )
) else if exist "%SRC_DIR%\Titan_InferenceCore.asm" (
    ml64 /c /Zi /Fo"%OUT_DIR%\Titan_InferenceCore.obj" "%SRC_DIR%\Titan_InferenceCore.asm"
    echo     OK: Titan_InferenceCore.obj
) else (
    echo     SKIP: No inference core found
)

echo.
echo [4/6] Assembling CLI Consumer...
ml64 /c /Zi /Fo"%OUT_DIR%\RawrXD_CLI_Titan.obj" "%SHIP_DIR%\RawrXD_CLI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble CLI
    exit /b 1
)
echo     OK: RawrXD_CLI_Titan.obj

echo.
echo [5/6] Assembling GUI IDE...
ml64 /c /Zi /Fo"%OUT_DIR%\RawrXD_GUI_Titan.obj" "%SHIP_DIR%\RawrXD_GUI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble GUI
    exit /b 1
)
echo     OK: RawrXD_GUI_Titan.obj

echo.
echo [6/6] Linking executables...

REM Set LIB path for Windows SDK
for /f "delims=" %%i in ('dir /s /b "C:\Program Files (x86)\Windows Kits\10\Lib\*\um\x64\kernel32.lib" 2^>nul') do (
    for %%j in ("%%~dpi.") do set "SDK_LIB=%%~dpj"
)
if defined SDK_LIB (
    set "LIB=%SDK_LIB%;%LIB%"
    echo     SDK Path: %SDK_LIB%
)

REM Also add UCRT
for /f "delims=" %%i in ('dir /s /b "C:\Program Files (x86)\Windows Kits\10\Lib\*\ucrt\x64\*.lib" 2^>nul') do (
    for %%j in ("%%~dpi.") do set "UCRT_LIB=%%~dpj"
)
if defined UCRT_LIB (
    set "LIB=%UCRT_LIB%;%LIB%"
)

REM Collect all object files
set "OBJS=%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj"
if exist "%OUT_DIR%\RawrXD_Titan_UNIFIED.obj" set "OBJS=%OBJS% %OUT_DIR%\RawrXD_Titan_UNIFIED.obj"
if exist "%OUT_DIR%\Titan_InferenceCore.obj" set "OBJS=%OBJS% %OUT_DIR%\Titan_InferenceCore.obj"

REM Link CLI
echo     Linking CLI...
link /SUBSYSTEM:CONSOLE /OUT:"%OUT_DIR%\RawrXD-CLI.exe" /DEBUG /PDB:"%OUT_DIR%\RawrXD-CLI.pdb" "%OUT_DIR%\RawrXD_CLI_Titan.obj" %OBJS% kernel32.lib user32.lib ws2_32.lib
if errorlevel 1 (
    echo ERROR: Failed to link CLI
    exit /b 1
)
echo     OK: RawrXD-CLI.exe

REM Link GUI
echo     Linking GUI...
link /SUBSYSTEM:WINDOWS /OUT:"%OUT_DIR%\RawrXD-IDE.exe" /DEBUG /PDB:"%OUT_DIR%\RawrXD-IDE.pdb" "%OUT_DIR%\RawrXD_GUI_Titan.obj" %OBJS% kernel32.lib user32.lib gdi32.lib comctl32.lib
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
dir /b "%OUT_DIR%\*.exe" 2>nul
echo.
echo Architecture:
echo   - 64MB Memory-Mapped Ring Buffer
echo   - AVX-512 Non-Temporal Streaming  
echo   - Native GGUF Inference (No Server)
echo   - High-Level Titan API
echo ============================================================

endlocal
