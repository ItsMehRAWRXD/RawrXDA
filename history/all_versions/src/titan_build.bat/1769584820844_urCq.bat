@echo off
setlocal enabledelayedexpansion

REM Titan Streaming Orchestrator - MASM64 build (object + static lib)

set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%"

set SRC=%SCRIPT_DIR%agentic\Titan_Streaming_Orchestrator.asm
set OBJ_DIR=%SCRIPT_DIR%build\obj
set BIN_DIR=%SCRIPT_DIR%build\bin
set OBJ=%OBJ_DIR%\Titan_Streaming_Orchestrator.obj
set LIB=%BIN_DIR%\Titan_Streaming_Orchestrator.lib

echo.
echo ============================================================
echo Titan Streaming Orchestrator - Build
echo ============================================================
echo.

echo [1/4] Checking toolchain (ml64, lib)...
where ml64 >nul 2>&1 || goto :no_masm
where lib  >nul 2>&1 || goto :no_lib
echo OK - toolchain located

echo.
echo [2/4] Preparing output folders...
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

echo.
echo [3/4] Assembling Titan_Streaming_Orchestrator.asm...
echo ml64 /nologo /c /Zi /Zf /Fo"%OBJ%" "%SRC%"
ml64 /nologo /c /Zi /Zf /Fo"%OBJ%" "%SRC%"
if errorlevel 1 goto :error

echo.
echo [4/4] Creating static library...
echo lib /nologo /OUT:"%LIB%" "%OBJ%"
lib /nologo /OUT:"%LIB%" "%OBJ%"
if errorlevel 1 goto :error

echo.
echo ============================================================
echo BUILD SUCCESS
echo ============================================================
echo OBJ: %OBJ%
echo LIB: %LIB%
echo.
popd
exit /b 0

:no_masm
echo ERROR: ml64.exe not found. Please run from "x64 Native Tools Command Prompt for VS 2022".
goto :fail

:no_lib
echo ERROR: lib.exe not found. Please run from "x64 Native Tools Command Prompt for VS 2022".
goto :fail

:error
echo.
echo ============================================================
echo BUILD FAILED
echo ============================================================

:fail
popd
exit /b 1
