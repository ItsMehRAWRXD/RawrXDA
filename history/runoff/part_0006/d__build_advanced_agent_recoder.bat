@echo off
REM ============================================================================
REM Advanced Multi-Agent MASM Recoder Build Script
REM Spawns subagents to recode non-MASM sources to pure MASM x64
REM NO MIXING - Pure Assembly Implementation
REM ============================================================================

echo ======================================================================
echo   ADVANCED MULTI-AGENT MASM RECODER - Build System
echo   Pure Assembly - NO MIXING - Subagent Architecture
echo ======================================================================
echo.

REM Check for Visual Studio Developer Command Prompt
where ml64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [INFO] ml64 not found in PATH
    echo [INFO] Attempting to locate Visual Studio...
    echo.
    
    REM Try common Visual Studio paths
    set VS2022="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    set VS2019="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    
    if exist %VS2022% (
        echo [INFO] Found Visual Studio 2022
        call %VS2022%
        goto :build
    )
    
    if exist %VS2019% (
        echo [INFO] Found Visual Studio 2019
        call %VS2019%
        goto :build
    )
    
    echo [ERROR] Visual Studio Build Tools not found
    echo [ERROR] Please run this from a Visual Studio Developer Command Prompt
    echo.
    echo To install Visual Studio Build Tools:
    echo   1. Download from: https://visualstudio.microsoft.com/downloads/
    echo   2. Install "Desktop development with C++" workload
    echo   3. Run from Developer Command Prompt
    echo.
    pause
    exit /b 1
)

:build
echo ======================================================================
echo Phase 1: Assembling Advanced Agent Recoder
echo ======================================================================
echo.

ml64 /c /Zi /Fo"advanced_agent_masm_recoder.obj" advanced_agent_masm_recoder.asm
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Assembly failed with error code %ERRORLEVEL%
    echo.
    pause
    exit /b 1
)

echo [OK] Assembly successful
echo.

echo ======================================================================
echo Phase 2: Linking with Windows API
echo ======================================================================
echo.

link /SUBSYSTEM:CONSOLE ^
     /ENTRY:main ^
     /OUT:advanced_agent_masm_recoder.exe ^
     /DEBUG ^
     advanced_agent_masm_recoder.obj ^
     kernel32.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Linking failed with error code %ERRORLEVEL%
    echo.
    pause
    exit /b 1
)

echo [OK] Linking successful
echo.

echo ======================================================================
echo   BUILD SUCCESS - Multi-Agent MASM Recoder
echo ======================================================================
echo.
echo   Output: advanced_agent_masm_recoder.exe
echo   Type: Pure x64 Assembly (NO MIXING)
echo   Features:
echo     - Spawns up to 16 concurrent agent threads
echo     - Processes 50 files per agent batch
echo     - Recodes C/C++/Python/JS to pure MASM x64
echo     - Zero high-level language dependencies
echo.
echo ======================================================================
echo   Usage
echo ======================================================================
echo.
echo   Run the executable:
echo     advanced_agent_masm_recoder.exe
echo.
echo   What it does:
echo     1. Scans D:\RawrXD\src for non-MASM sources
echo     2. Batches files into groups of 50
echo     3. Spawns agent threads (1 per batch, max 16)
echo     4. Each agent recodes files to pure MASM x64
echo     5. Outputs to: D:\RawrXD\src_masm_recoded\
echo     6. Generates: D:\RawrXD\agent_recode_report.json
echo.
echo   Configuration (edit .asm file):
echo     MAX_AGENTS         = 16   (concurrent threads)
echo     FILES_PER_AGENT    = 50   (batch size)
echo.
echo ======================================================================
echo.

pause
