@echo off
echo Building Complete Game - Intelligent Bot AI System
echo ==================================================

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
    set COMPILE_FLAGS=/EHsc /I"include" /I"include\glad" /I"include\GLFW" /I"include\glm" /std:c++17 /O2
    set LINK_FLAGS=opengl32.lib user32.lib gdi32.lib shell32.lib winmm.lib
    set OUTPUT=complete_game.exe
) else (
    set COMPILER=g++
    set COMPILE_FLAGS=-std=c++17 -I"include" -I"include\glad" -I"include\GLFW" -I"include\glm" -O2 -Wall -Wextra
    set LINK_FLAGS=-lopengl32 -luser32 -lgdi32 -lshell32 -lwinmm
    set OUTPUT=complete_game.exe
)

echo Using compiler: %COMPILER%
echo.

REM Create output directory
if not exist bin mkdir bin

REM Compile all source files
echo Compiling complete game with intelligent AI system...

REM Core game files
set SOURCES=src\bot_warfare_demo.cpp src\intelligent_bot_ai.cpp src\game_modes.cpp src\advanced_movement_system.cpp src\3d_audio_system.cpp

REM OpenGL and graphics files
set GRAPHICS_SOURCES=src\glad.c src\glfw3.c

REM Additional game systems (if they exist)
if exist src\weapon_system.cpp set SOURCES=%SOURCES% src\weapon_system.cpp
if exist src\movement_system.cpp set SOURCES=%SOURCES% src\movement_system.cpp
if exist src\advanced_warzone_generator.cpp set SOURCES=%SOURCES% src\advanced_warzone_generator.cpp
if exist src\destructible_environment.cpp set SOURCES=%SOURCES% src\destructible_environment.cpp
if exist src\hud_system.cpp set SOURCES=%SOURCES% src\hud_system.cpp
if exist src\physics_world.cpp set SOURCES=%SOURCES% src\physics_world.cpp
if exist src\texture_manager.cpp set SOURCES=%SOURCES% src\texture_manager.cpp
if exist src\hdr_renderer.cpp set SOURCES=%SOURCES% src\hdr_renderer.cpp
if exist src\load_balancing_system.cpp set SOURCES=%SOURCES% src\load_balancing_system.cpp
if exist src\multiplayer_networking.cpp set SOURCES=%SOURCES% src\multiplayer_networking.cpp

echo Compiling with sources: %SOURCES%
echo Graphics sources: %GRAPHICS_SOURCES%

%COMPILER% %COMPILE_FLAGS% %SOURCES% %GRAPHICS_SOURCES% -o bin\%OUTPUT% %LINK_FLAGS%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ================================================
    echo Build failed! Trying alternative compilation...
    echo ================================================
    
    REM Try building just the core bot warfare demo
    echo Building core bot warfare demo only...
    %COMPILER% %COMPILE_FLAGS% src\bot_warfare_demo.cpp src\intelligent_bot_ai.cpp src\glad.c src\glfw3.c -o bin\bot_warfare_demo.exe %LINK_FLAGS%
    
    if %ERRORLEVEL% neq 0 (
        echo.
        echo ================================================
        echo Build failed completely!
        echo ================================================
        echo.
        echo Make sure you have:
        echo   1. A C++ compiler (MinGW or Visual Studio)
        echo   2. OpenGL development libraries
        echo   3. All include files in the include directory
        echo   4. All source files in the src directory
        echo.
        echo Missing files might be:
        if not exist src\bot_warfare_demo.cpp echo   - src\bot_warfare_demo.cpp
        if not exist src\intelligent_bot_ai.cpp echo   - src\intelligent_bot_ai.cpp
        if not exist src\glad.c echo   - src\glad.c
        if not exist src\glfw3.c echo   - src\glfw3.c
        if not exist include\intelligent_bot_ai.hpp echo   - include\intelligent_bot_ai.hpp
        if not exist include\glad\glad.h echo   - include\glad\glad.h
        if not exist include\GLFW\glfw3.h echo   - include\GLFW\glfw3.h
        if not exist include\glm\glm.hpp echo   - include\glm\glm.hpp
        pause
        exit /b 1
    )
    
    echo.
    echo ================================================
    echo Core bot warfare demo built successfully!
    echo ================================================
    echo.
    echo Executable created: bin\bot_warfare_demo.exe
    echo.
    echo This is the core intelligent AI system with:
    echo   - 8 Intelligent AI bots
    echo   - Adaptive difficulty scaling
    echo   - Multiple bot personalities
    echo   - Team coordination
    echo   - Learning AI that adapts to your skill
    echo.
    echo Press any key to run the core demo...
    pause >nul
    
    REM Run the core demo
    echo Running Bot Warfare Demo...
    bin\bot_warfare_demo.exe
    
    echo.
    echo Demo finished. Press any key to exit...
    pause >nul
    exit /b 0
)

echo.
echo ================================================
echo Complete game built successfully!
echo ================================================
echo.
echo Executable created: bin\%OUTPUT%
echo.
echo Complete Game Features:
echo   - Intelligent AI bot system with 8+ bots
echo   - Adaptive difficulty that scales to your skill
echo   - Multiple bot personalities (Aggressive, Defensive, Tactical, etc.)
echo   - Advanced movement and combat systems
echo   - 3D audio system for immersive experience
echo   - HUD system with real-time information
echo   - Physics world with realistic interactions
echo   - Destructible environment system
echo   - Advanced warzone generation
echo   - Load balancing for large-scale battles
echo   - Multiplayer networking support
echo   - HDR rendering for stunning visuals
echo   - Texture management system
echo.
echo Game Modes Available:
echo   - Deathmatch with intelligent bots
echo   - Team Deathmatch with coordinated AI
echo   - Battle Royale with survival AI
echo   - Custom modes with adaptive AI
echo.
echo Controls:
echo   WASD - Move player
echo   Mouse - Look around
echo   ESC - Exit
echo   F1 - Toggle debug info
echo   F2 - Toggle bot info
echo   F3 - Change difficulty
echo.
echo Press any key to run the complete game...
pause >nul

REM Run the complete game
echo Running Complete Game...
bin\%OUTPUT%

echo.
echo Game finished. Press any key to exit...
pause >nul
