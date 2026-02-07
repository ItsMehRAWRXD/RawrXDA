@echo off
echo Building Mirage Generator Demo...

REM Create build directory
if not exist build_mirage mkdir build_mirage
cd build_mirage

REM Compile the mirage demo
g++ -std=c++17 -O2 -o mirage_demo.exe ../src/mirage_demo.cpp

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Executable location: build_mirage\mirage_demo.exe
    echo.
    echo To run the mirage demo:
    echo cd build_mirage
    echo mirage_demo.exe
    echo.
    echo Features:
    echo - Dynamic landscape changes without loading screens
    echo - 15 different mirage types
    echo - Real-time terrain morphing
    echo - Advanced shader effects
    echo - AI-driven mirage generation
    echo - Performance optimization
    echo.
) else (
    echo.
    echo Build failed!
    echo Check the error messages above.
    echo.
)

pause
