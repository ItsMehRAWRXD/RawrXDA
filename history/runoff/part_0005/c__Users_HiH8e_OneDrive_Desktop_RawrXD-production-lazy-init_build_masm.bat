@echo off
REM Build script for MASM components

REM Set paths
set MASM_PATH="C:\masm32\bin"
set SRC_DIR=..\..\src\win32\agentic
set BUILD_DIR=..\..\build-win32-only\obj

REM Create build directory if it doesn't exist
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM Assemble the MASM file
%MASM_PATH%\ml.exe /c /coff /Cp /nologo /I%MASM_PATH%\include %SRC_DIR%\agentic_core.asm /Fo%BUILD_DIR%\agentic_core.obj

if errorlevel 1 (
    echo MASM assembly failed
    exit /b 1
)

echo MASM assembly successful
exit /b 0