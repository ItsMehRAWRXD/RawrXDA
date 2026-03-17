@echo off
REM ============================================================================
REM NASM Setup Script - Windows x64
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ================================================
echo   NASM Installation Setup
echo ================================================
echo.

REM Check if NASM already installed
set "NASM_PATH=C:\Program Files\NASM\nasm.exe"
if exist "%NASM_PATH%" (
    echo ✓ NASM already installed at: %NASM_PATH%
    "%NASM_PATH%" -version
    goto :EOF
)

REM Check for NASM zip on D: drive
if exist "D:\nasm\nasm.zip" (
    echo Found NASM zip on D:\nasm\nasm.zip
    echo Extracting...
    
    REM Create Program Files directory
    if not exist "C:\Program Files\NASM" mkdir "C:\Program Files\NASM"
    
    REM Extract using PowerShell (more reliable)
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Add-Type -AssemblyName 'System.IO.Compression.FileSystem'; ^
         [System.IO.Compression.ZipFile]::ExtractToDirectory('D:\nasm\nasm.zip', 'C:\Program Files\NASM')" 2>nul
    
    if errorlevel 1 (
        echo ERROR: Failed to extract NASM zip
        exit /b 1
    )
    
    echo ✓ NASM extracted successfully
    "%NASM_PATH%" -version
    echo.
    echo ✓ NASM is now ready to use!
    goto :EOF
)

REM If not found locally, provide download instructions
echo.
echo ERROR: NASM not found
echo.
echo Option 1: Download and Install NASM
echo   - Visit: https://www.nasm.us/
echo   - Download the Windows installer
echo   - Install to: C:\Program Files\NASM
echo.
echo Option 2: Extract from local zip
echo   - Place nasm.zip at: D:\nasm\nasm.zip
echo   - Run this script again
echo.
exit /b 1
