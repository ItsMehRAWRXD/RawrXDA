@echo off
echo Building Bot Warfare Demo - Intelligent AI System
echo ================================================

REM Check if we have a C++ compiler
where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    where cl >nul 2>nul
    if %ERRORLEVEL% neq 0 (
        echo No C++ compiler found! Please install MinGW or Visual Studio.
        pause
        exit /b 1
    )
    set COMPILER=cl
    set COMPILE_FLAGS=/EHsc /I"include" /I"include\glad" /I"include\GLFW" /I"include\glm" /std:c++17
    set LINK_FLAGS=opengl32.lib user32.lib gdi32.lib shell32.lib
    set OUTPUT=bot_warfare_demo.exe
) else (
    set COMPILER=g++
    set COMPILE_FLAGS=-std=c++17 -I"include" -I"include\glad" -I"include\GLFW" -I"include\glm" -O2 -Wall
    set LINK_FLAGS=-lopengl32 -luser32 -lgdi32 -lshell32
    set OUTPUT=bot_warfare_demo.exe
)

echo Using compiler: %COMPILER%
echo.

REM Create output directory
if not exist bin mkdir bin

REM Compile the bot warfare demo
echo Compiling intelligent bot warfare demo...
%COMPILER% %COMPILE_FLAGS% src\bot_warfare_demo.cpp src\intelligent_bot_ai.cpp src\glad.c src\glfw3.c -o bin\%OUTPUT% %LINK_FLAGS%

if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    echo.
    echo Make sure you have:
    echo   1. A C++ compiler (MinGW or Visual Studio)
    echo   2. OpenGL development libraries
    echo   3. All include files in the include directory
    pause
    exit /b 1
)

echo.
echo ================================================
echo Build completed successfully!
echo.
echo Executable created: bin\%OUTPUT%
echo.
echo Bot Warfare Demo Features:
echo   - 8 Intelligent AI bots with different personalities
echo   - Adaptive difficulty that scales to your skill
echo   - Multiple bot personalities (Aggressive, Defensive, Tactical, etc.)
echo   - Team coordination and advanced AI tactics
echo   - Real-time performance tracking and adaptation
echo.
echo Controls:
echo   WASD - Move player
echo   Mouse - Look around
echo   ESC - Exit
echo.
echo Press any key to run the demo now...
pause >nul

REM Run the demo
echo Running Bot Warfare Demo...
bin\%OUTPUT%

echo.
echo Demo finished. Press any key to exit...
pause >nul
