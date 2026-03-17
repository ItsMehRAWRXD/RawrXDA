@echo off
REM Titan Streaming Orchestrator - Build Script
REM Automated compilation and linking for MASM64

setlocal enabledelayedexpansion

echo.
echo ============================================================
echo Titan Streaming Orchestrator - Build Process
echo ============================================================
echo.

REM Check for required tools
echo [1/5] Checking build environment...
where ml64 >nul 2>&1
if errorlevel 1 (
    echo ERROR: ml64.exe not found in PATH
    echo Solution: Run from "x64 Native Tools Command Prompt for VS 2022"
    goto error
)

where link >nul 2>&1
if errorlevel 1 (
    echo ERROR: link.exe not found in PATH
    echo Solution: Run from "x64 Native Tools Command Prompt for VS 2022"
    goto error
)

echo OK - ml64.exe and link.exe found

REM Create output directory
echo.
echo [2/5] Creating output directories...
if not exist build mkdir build
if not exist build\obj mkdir build\obj
if not exist build\bin mkdir build\bin
echo OK - Directories created

REM Assemble
echo.
echo [3/5] Assembling MASM64 source...
echo Command: ml64 /c /Zd /Zi /Fo"build\obj\titan.obj" "Titan_Streaming_Orchestrator_Fixed.asm"

ml64 /c /Zd /Zi /Fo"build\obj\titan.obj" "Titan_Streaming_Orchestrator_Fixed.asm"
if errorlevel 1 (
    echo ERROR: Assembly failed
    echo.
    echo Common issues:
    echo - Missing EXTERN declarations
    echo - Invalid instruction syntax
    echo - Improper PROC FRAME blocks
    echo.
    echo Check TROUBLESHOOTING.md for solutions
    goto error
)
echo OK - Assembly successful

REM Link
echo.
echo [4/5] Linking executable...
echo Command: link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:"build\bin\titan.exe" "build\obj\titan.obj"

link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:"build\bin\titan.exe" "build\obj\titan.obj" kernel32.lib ws2_32.lib
if errorlevel 1 (
    echo ERROR: Linking failed
    echo.
    echo Common issues:
    echo - Unresolved external symbols
    echo - Missing library files
    echo - Invalid subsystem specification
    echo.
    echo Check TROUBLESHOOTING.md for solutions
    goto error
)
echo OK - Linking successful

REM Verify
echo.
echo [5/5] Verifying output...
if exist "build\bin\titan.exe" (
    for %%A in ("build\bin\titan.exe") do (
        echo OK - Executable created: %%~fA (%%~zA bytes)
    )
) else (
    echo ERROR: Executable not found
    goto error
)

echo.
echo ============================================================
echo BUILD SUCCESSFUL
echo ============================================================
echo.
echo Executable: build\bin\titan.exe
echo Object file: build\obj\titan.obj
echo.
echo To run: build\bin\titan.exe
echo.
goto end

:error
echo.
echo ============================================================
echo BUILD FAILED
echo ============================================================
echo.
echo For help, see: TROUBLESHOOTING.md
echo.
exit /b 1

:end
exit /b 0
