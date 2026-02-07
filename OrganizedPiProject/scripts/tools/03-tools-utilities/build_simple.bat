@echo off
echo Building Simple FPS Game Engine...

REM Create build directory
if not exist build_simple mkdir build_simple
cd build_simple

REM Compile the simple game
g++ -std=c++17 -O2 -o simple_game.exe ../src/simple_game.cpp

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Executable location: build_simple\simple_game.exe
    echo.
    echo To run the game:
    echo cd build_simple
    echo simple_game.exe
    echo.
) else (
    echo.
    echo Build failed!
    echo Check the error messages above.
    echo.
)

pause
