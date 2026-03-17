@echo off
setlocal enabledelayedexpansion

REM ==========================================================================
REM RawrXD Standalone Inference Build (No llama-server.exe)
REM ==========================================================================
REM Compiles and links:
REM   1. Titan_InferenceCore.asm    - GGUF loader, Dequantize, Transformer
REM   2. Titan_Streaming_Orchestrator_NoHTTP.asm - Native inference thread
REM   3. RawrXD_CLI.asm (or _GUI.asm) - Frontend
REM Result: RawrXD-Agent.exe (fully self-contained LLM runtime)
REM ==========================================================================

echo.
echo ========================================
echo RawrXD Standalone Inference Engine Build
echo No external llama-server.exe required
echo ========================================
echo.

REM Find ml64.exe
set "ML64="
for /f "delims=" %%i in ('where /r "%ProgramFiles(x86)%\Microsoft Visual Studio" ml64.exe 2^>nul ^| findstr /i "x64" ^| findstr /i "hostx64"') do (
    set "ML64=%%i"
    goto :found_ml64
)

for /f "delims=" %%i in ('where /r "%ProgramFiles%\Microsoft Visual Studio" ml64.exe 2^>nul ^| findstr /i "x64" ^| findstr /i "hostx64"') do (
    set "ML64=%%i"
    goto :found_ml64
)

echo [-] ml64.exe not found. Install VS Build Tools x64.
exit /b 1

:found_ml64
echo [+] Found ml64.exe: %ML64%

REM Find link.exe
for %%D in ("%ML64%") do (
    set "LINK_DIR=%%~dpD"
)
set "LINK=%LINK_DIR%link.exe"

if not exist "%LINK%" (
    echo [-] link.exe not found at %LINK%
    exit /b 1
)
echo [+] Found link.exe: %LINK%

REM Build paths
set "SRCDIR=D:\rawrxd\src"
set "OUTDIR=D:\rawrxd\bin"
set "OBJDIR=D:\rawrxd\obj"

if not exist "%OBJDIR%" mkdir "%OBJDIR%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo.
echo ============ ASSEMBLY PHASE ============
echo.

REM Step 1: Assemble Inference Core
echo [1/3] Assembling Titan_InferenceCore.asm...
"%ML64%" /c /Zi /O2 /Fo"%OBJDIR%\Titan_InferenceCore.obj" "%SRCDIR%\Titan_InferenceCore.asm"
if errorlevel 1 (
    echo [-] Assembly failed: Titan_InferenceCore.asm
    exit /b 1
)
echo [+] Titan_InferenceCore.obj OK

REM Step 2: Assemble Orchestrator (No HTTP)
echo [2/3] Assembling Titan_Streaming_Orchestrator_NoHTTP.asm...
"%ML64%" /c /Zi /O2 /Fo"%OBJDIR%\Titan_Orchestrator_NoHTTP.obj" "%SRCDIR%\Titan_Streaming_Orchestrator_NoHTTP.asm"
if errorlevel 1 (
    echo [-] Assembly failed: Titan_Streaming_Orchestrator_NoHTTP.asm
    exit /b 1
)
echo [+] Titan_Orchestrator_NoHTTP.obj OK

REM Step 3: Assemble CLI (or GUI)
echo [3/3] Assembling RawrXD_CLI.asm...
"%ML64%" /c /Zi /O2 /Fo"%OBJDIR%\RawrXD_CLI.obj" "%SRCDIR%\RawrXD_CLI.asm"
if errorlevel 1 (
    echo [-] Assembly failed: RawrXD_CLI.asm
    exit /b 1
)
echo [+] RawrXD_CLI.obj OK

echo.
echo ============ LINKING PHASE ============
echo.

REM Link all objects
echo [*] Linking RawrXD-Agent.exe (Standalone Inference)...
"%LINK%" /SUBSYSTEM:CONSOLE ^
    /OUT:"%OUTDIR%\RawrXD-Agent.exe" ^
    /DEBUG ^
    "%OBJDIR%\Titan_InferenceCore.obj" ^
    "%OBJDIR%\Titan_Orchestrator_NoHTTP.obj" ^
    "%OBJDIR%\RawrXD_CLI.obj" ^
    kernel32.lib ntdll.lib advapi32.lib

if errorlevel 1 (
    echo [-] Linking failed
    exit /b 1
)

echo [+] Linking successful

echo.
echo ========================================
echo [SUCCESS] Built RawrXD-Agent.exe
echo ========================================
echo.
echo Location: %OUTDIR%\RawrXD-Agent.exe
echo Size:     (check: dir "%OUTDIR%\RawrXD-Agent.exe")
echo.
echo Features:
echo   - Native GGUF loading (direct mmap)
echo   - Q2_K / Q4_0 dequantization (on-the-fly)
echo   - Transformer inference (AVX-512 matmul)
echo   - Streaming output (ring buffer)
echo   - Zero external dependencies (no llama-server.exe)
echo.
echo Usage:
echo   RawrXD-Agent.exe model.gguf "Hello world"
echo   RawrXD-Agent.exe model.gguf < prompt.txt
echo.

endlocal
exit /b 0
