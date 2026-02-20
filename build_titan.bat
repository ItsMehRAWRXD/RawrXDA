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
echo [1/7] Locating ml64.exe...
set "ML64="
for /f "delims=" %%i in ('where ml64.exe 2^>nul') do (
    if not defined ML64 set "ML64=%%i"
)
if not defined ML64 (
    for /f "delims=" %%i in ('dir /s /b "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" 2^>nul') do (
        if not defined ML64 set "ML64=%%i"
    )
)
if not defined ML64 (
    for /f "delims=" %%i in ('dir /s /b "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" 2^>nul') do (
        if not defined ML64 set "ML64=%%i"
    )
)
if not defined ML64 (
    echo ERROR: ml64.exe not found. Trying to use VS environment...
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set "ML64=ml64.exe"
    set "LINK_EXE=link.exe"
    goto :SKIP_LINK_SEARCH
)
echo     Found: %ML64%

REM Find link.exe
set "LINK_EXE="
for /f "delims=" %%i in ('where link.exe 2^>nul') do (
    if not defined LINK_EXE set "LINK_EXE=%%i"
)
if not defined LINK_EXE (
    for /f "delims=" %%i in ('dir /s /b "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" 2^>nul') do (
        if not defined LINK_EXE set "LINK_EXE=%%i"
    )
)
if not defined LINK_EXE (
    set "LINK_EXE=link.exe"
)
echo     Found: %LINK_EXE%

:SKIP_LINK_SEARCH

echo.
echo [2/6] Assembling Titan Streaming Orchestrator...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" "%SRC_DIR%\Titan_Streaming_Orchestrator_Fixed.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble orchestrator
    exit /b 1
)
echo     OK: Titan_Streaming_Orchestrator_Fixed.obj

echo.
echo [3/6] Assembling Titan Inference Core (Native GGUF Engine)...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\Titan_InferenceCore.obj" "%SRC_DIR%\Titan_InferenceCore.asm"
if errorlevel 1 (
    echo WARNING: InferenceCore assembly failed - continuing without native inference
    set "INFER_OBJ="
) else (
    echo     OK: Titan_InferenceCore.obj
    set "INFER_OBJ=%OUT_DIR%\Titan_InferenceCore.obj"
)

echo.
echo [4/6] Assembling MetaReverse Bootstrap...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\RawrXD_Titan_MetaReverse.obj" "%SHIP_DIR%\RawrXD_Titan_MetaReverse.asm"
if errorlevel 1 (
    echo WARNING: MetaReverse assembly failed - will use standard libs
    set "META_OBJ="
    set "USE_NODEFAULTLIB=0"
) else (
    echo     OK: RawrXD_Titan_MetaReverse.obj
    set "META_OBJ=%OUT_DIR%\RawrXD_Titan_MetaReverse.obj"
    set "USE_NODEFAULTLIB=1"
)

echo.
echo [5/7] Assembling CLI Consumer...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\RawrXD_CLI_Titan.obj" "%SHIP_DIR%\RawrXD_CLI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble CLI
    exit /b 1
)
echo     OK: RawrXD_CLI_Titan.obj

echo.
echo [6/7] Assembling GUI IDE...
"%ML64%" /c /Zi /Fo"%OUT_DIR%\RawrXD_GUI_Titan.obj" "%SHIP_DIR%\RawrXD_GUI_Titan.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble GUI
    exit /b 1
)
echo     OK: RawrXD_GUI_Titan.obj

echo.
echo [7/7] Linking executables...

REM Try to find SDK lib path for fallback
set "SDK_LIB="
for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\Windows Kits\10\Lib\*\um\x64\kernel32.lib" 2^>nul') do (
    if not defined SDK_LIB for %%j in ("%%~dpi..") do set "SDK_LIB=%%~dpj"
)

if "%USE_NODEFAULTLIB%"=="1" (
    echo     Mode: NODEFAULTLIB (MetaReverse PEB Walking)
    
    REM Link CLI (console subsystem, no libs needed)
    "%LINK_EXE%" /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:main ^
        /DEBUG /PDB:"%OUT_DIR%\RawrXD-CLI.pdb" ^
        /OUT:"%OUT_DIR%\RawrXD-CLI.exe" ^
        "%OUT_DIR%\RawrXD_CLI_Titan.obj" ^
        %META_OBJ% ^
        "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" ^
        %INFER_OBJ%
    if errorlevel 1 (
        echo     CLI link failed, trying with libs...
        goto :LINK_WITH_LIBS
    )
    echo     OK: RawrXD-CLI.exe

    REM Link GUI (windows subsystem, no libs needed)
    "%LINK_EXE%" /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:WinMain ^
        /DEBUG /PDB:"%OUT_DIR%\RawrXD-IDE.pdb" ^
        /OUT:"%OUT_DIR%\RawrXD-IDE.exe" ^
        "%OUT_DIR%\RawrXD_GUI_Titan.obj" ^
        %META_OBJ% ^
        "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" ^
        %INFER_OBJ%
    if errorlevel 1 (
        echo     GUI link failed, trying with libs...
        goto :LINK_WITH_LIBS
    )
    echo     OK: RawrXD-IDE.exe
    goto :LINK_DONE
)

:LINK_WITH_LIBS
echo     Mode: Standard (with kernel32.lib)

if defined SDK_LIB (
    set "LIB=%SDK_LIB%x64;%LIB%"
    echo     SDK Lib Path: %SDK_LIB%x64
)

REM Link CLI with standard libs
"%LINK_EXE%" /SUBSYSTEM:CONSOLE ^
    /DEBUG /PDB:"%OUT_DIR%\RawrXD-CLI.pdb" ^
    /OUT:"%OUT_DIR%\RawrXD-CLI.exe" ^
    "%OUT_DIR%\RawrXD_CLI_Titan.obj" ^
    "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" ^
    %INFER_OBJ% ^
    kernel32.lib user32.lib ws2_32.lib
if errorlevel 1 (
    echo ERROR: Failed to link CLI
    exit /b 1
)
echo     OK: RawrXD-CLI.exe

REM Link GUI with standard libs
"%LINK_EXE%" /SUBSYSTEM:WINDOWS ^
    /DEBUG /PDB:"%OUT_DIR%\RawrXD-IDE.pdb" ^
    /OUT:"%OUT_DIR%\RawrXD-IDE.exe" ^
    "%OUT_DIR%\RawrXD_GUI_Titan.obj" ^
    "%OUT_DIR%\Titan_Streaming_Orchestrator_Fixed.obj" ^
    %INFER_OBJ% ^
    kernel32.lib user32.lib gdi32.lib comctl32.lib
if errorlevel 1 (
    echo ERROR: Failed to link GUI
    exit /b 1
)
echo     OK: RawrXD-IDE.exe

:LINK_DONE

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
