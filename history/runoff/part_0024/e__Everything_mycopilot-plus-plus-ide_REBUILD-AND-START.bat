@echo off
title MyCoPilot++ IDE - Quick Rebuild
color 0A
cls

echo.
echo ========================================
echo  MyCoPilot++ IDE - Quick Rebuild Script
echo ========================================
echo.

cd /d E:\Everything\mycopilot-plus-plus-ide

echo [1/3] Cleaning old build...
if exist "MyCoPilot-IDE.exe" del /F /Q "MyCoPilot-IDE.exe" >nul 2>&1
echo     Done!

echo.
echo [2/3] Building new executable...
call build-exe.bat

echo.
echo [3/3] Testing the build...
if exist "MyCoPilot-IDE.exe" (
    echo     ✅ Build successful!
    echo.
    echo ========================================
    echo  BUILD COMPLETE
    echo ========================================
    echo.
    echo Executable: MyCoPilot-IDE.exe
    echo Location: E:\Everything\mycopilot-plus-plus-ide\
    echo.
    echo Press any key to start the server...
    pause >nul
    echo.
    echo Starting MyCoPilot++ IDE Server...
    echo Server will run on http://localhost:8080
    echo Press Ctrl+C to stop the server
    echo.
    start http://localhost:8080
    .\MyCoPilot-IDE.exe
) else (
    echo     ❌ Build failed!
    echo     Check the error messages above.
    echo.
    pause
)
