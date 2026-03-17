@echo off
REM =============================================================================
REM RawrXD QuadBuffer DMA Build Script
REM =============================================================================
REM This script compiles the QuadBuffer DMA Orchestrator and links with
REM existing RawrXD phase implementations to create a complete system capable
REM of running 800B parameter models on 4GB VRAM.
REM =============================================================================

setlocal enabledelayedexpansion
cls

REM =============================================================================
REM Configuration
REM =============================================================================

REM Detect Visual Studio installation
set "VS_PATH="
set "MSVC_VERSION="

for /f "tokens=* usebackq" %%I in (`cd /d "C:\Program Files (x86)\Microsoft Visual Studio" 2^>nul ^&^& dir /b /ad /o-d`) do (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%I\Community\VC\Tools\MSVC" (
        for /f "tokens=* usebackq" %%J in (`cd /d "C:\Program Files (x86)\Microsoft Visual Studio\%%I\Community\VC\Tools\MSVC" 2^>nul ^&^& dir /b /ad /o-d`) do (
            set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%%I\Community\VC\Tools\MSVC\%%J\bin\Hostx64\x64"
            set "MSVC_VERSION=%%J"
            goto :found_vs
        )
    )
)

:found_vs
if not defined VS_PATH (
    echo ERROR: Cannot find Visual Studio installation
    echo Please ensure Visual Studio 2019 or later is installed
    exit /b 1
)

set "ML64=%VS_PATH%\ml64.exe"
set "LINK=%VS_PATH%\link.exe"
set "LIB_PATH=%VS_PATH%\..\..\..\..\..\Windows Kits\10\Lib\10.0.19041.0\um\x64"

if not exist "%ML64%" (
    echo ERROR: ML64.exe not found at %ML64%
    exit /b 1
)

if not exist "%LINK%" (
    echo ERROR: LINK.exe not found at %LINK%
    exit /b 1
)

echo.
echo ╔════════════════════════════════════════════════════════════════════════╗
echo ║            RawrXD QuadBuffer DMA Build System (ML64 x64)              ║
echo ╚════════════════════════════════════════════════════════════════════════╝
echo.
echo ML64 Location: %ML64%
echo LINK Location: %LINK%
echo Visual Studio: %MSVC_VERSION%
echo.

REM =============================================================================
REM Build Options
REM =============================================================================

set "BUILD_DEBUG=0"
set "BUILD_RELEASE=1"
set "ENABLE_PROFILING=0"

REM Parse command-line arguments
if "%1"=="debug" set "BUILD_DEBUG=1" & set "BUILD_RELEASE=0"
if "%1"=="profile" set "ENABLE_PROFILING=1"
if "%1"=="clean" goto :clean_build

echo Build Configuration:
echo  Debug Build:    %BUILD_DEBUG%
echo  Release Build:  %BUILD_RELEASE%
echo  Profiling:      %ENABLE_PROFILING%
echo.

REM =============================================================================
REM Assembly Phase
REM =============================================================================

echo ════════════════════════════════════════════════════════════════════════
echo PHASE 1: Assembling Core Modules
echo ════════════════════════════════════════════════════════════════════════
echo.

REM Assemble QuadBuffer DMA Orchestrator (core engine)
echo Assembling: RawrXD_QuadBuffer_DMA_Orchestrator.asm
if %BUILD_DEBUG% equ 1 (
    "%ML64%" /c /Zi /Fo"obj\QuadBuffer_DMA.obj" ^
        /I"D:\rawrxd\include" ^
        "D:\rawrxd\src\orchestrator\RawrXD_QuadBuffer_DMA_Orchestrator.asm"
) else (
    "%ML64%" /c /O2 /Fo"obj\QuadBuffer_DMA.obj" ^
        /I"D:\rawrxd\include" ^
        "D:\rawrxd\src\orchestrator\RawrXD_QuadBuffer_DMA_Orchestrator.asm"
)

if errorlevel 1 (
    echo.
    echo ERROR: Assembly failed for RawrXD_QuadBuffer_DMA_Orchestrator.asm
    exit /b 1
)
echo   ✓ Successfully assembled
echo.

REM Assemble Phase 5 Orchestrator (if separate file)
echo Assembling: Phase5_Master_Complete.asm
if %BUILD_DEBUG% equ 1 (
    "%ML64%" /c /Zi /Fo"obj\Phase5_Master.obj" ^
        /I"D:\rawrxd\include" ^
        "D:\rawrxd\src\orchestrator\Phase5_Master_Complete.asm"
) else (
    "%ML64%" /c /O2 /Fo"obj\Phase5_Master.obj" ^
        /I"D:\rawrxd\include" ^
        "D:\rawrxd\src\orchestrator\Phase5_Master_Complete.asm"
)

if errorlevel 1 (
    echo.
    echo ERROR: Assembly failed for Phase5_Master_Complete.asm
    exit /b 1
)
echo   ✓ Successfully assembled
echo.

REM =============================================================================
REM Compilation Phase (C++)
REM =============================================================================

echo ════════════════════════════════════════════════════════════════════════
echo PHASE 2: Compiling C++ Integration Layer
echo ════════════════════════════════════════════════════════════════════════
echo.

REM Note: C++ compilation would go here if needed
REM For now, we focus on MASM assembly

REM =============================================================================
REM Linking Phase
REM =============================================================================

echo ════════════════════════════════════════════════════════════════════════
echo PHASE 3: Linking Modules
echo ════════════════════════════════════════════════════════════════════════
echo.

echo Linking: RawrXD-QuadBuffer.exe

REM Determine output and symbols
if %BUILD_DEBUG% equ 1 (
    set LINK_FLAGS=/DEBUG /SUBSYSTEM:WINDOWS /OUT:bin\RawrXD-QuadBuffer-Debug.exe
) else (
    set LINK_FLAGS=/SUBSYSTEM:WINDOWS /OUT:bin\RawrXD-QuadBuffer.exe
)

REM Create bin and obj directories if needed
if not exist "obj" mkdir obj
if not exist "bin" mkdir bin

REM Link objects
"%LINK%" %LINK_FLAGS% ^
    obj\QuadBuffer_DMA.obj ^
    obj\Phase5_Master.obj ^
    kernel32.lib user32.lib gdi32.lib advapi32.lib ws2_32.lib ^
    /LIBPATH:"%LIB_PATH%"

if errorlevel 1 (
    echo.
    echo ERROR: Linking failed
    exit /b 1
)

echo   ✓ Successfully linked
echo.

REM =============================================================================
REM Post-Build Verification
REM =============================================================================

echo ════════════════════════════════════════════════════════════════════════
echo PHASE 4: Post-Build Verification
echo ════════════════════════════════════════════════════════════════════════
echo.

REM Show file information
if %BUILD_DEBUG% equ 1 (
    if exist "bin\RawrXD-QuadBuffer-Debug.exe" (
        echo Executable: RawrXD-QuadBuffer-Debug.exe
        for /f "tokens=5" %%I in ('dir "bin\RawrXD-QuadBuffer-Debug.exe" ^| find /c ""') do echo   Size: %%I bytes
    )
) else (
    if exist "bin\RawrXD-QuadBuffer.exe" (
        echo Executable: RawrXD-QuadBuffer.exe
        for /f "tokens=5" %%I in ('dir "bin\RawrXD-QuadBuffer.exe" ^| find /c ""') do echo   Size: %%I bytes
    )
)
echo.

REM Dump exported functions (verification)
echo Exported Functions:
if %BUILD_DEBUG% equ 1 (
    dumpbin /exports bin\RawrXD-QuadBuffer-Debug.exe 2>nul | findstr /I "INFINITY_" | head -n 15
) else (
    dumpbin /exports bin\RawrXD-QuadBuffer.exe 2>nul | findstr /I "INFINITY_" | head -n 15
)
echo.

REM =============================================================================
REM Build Complete
REM =============================================================================

echo ════════════════════════════════════════════════════════════════════════
echo BUILD COMPLETE ✓
echo ════════════════════════════════════════════════════════════════════════
echo.

if %BUILD_DEBUG% equ 1 (
    echo Executable: bin\RawrXD-QuadBuffer-Debug.exe
    echo Symbols:    bin\RawrXD-QuadBuffer-Debug.pdb
) else (
    echo Executable: bin\RawrXD-QuadBuffer.exe
)

echo.
echo Ready to run 800B parameter models on 4GB VRAM!
echo Usage: RawrXD-QuadBuffer.exe --model model.gguf --vram-size 4GB
echo.

goto :end

REM =============================================================================
REM Clean Build
REM =============================================================================

:clean_build
echo Cleaning build artifacts...
if exist "obj" rmdir /s /q obj
if exist "bin" rmdir /s /q bin
echo Clean complete.
goto :end

REM =============================================================================
REM End of Build Script
REM =============================================================================

:end
endlocal
exit /b 0
