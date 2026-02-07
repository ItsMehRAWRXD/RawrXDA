@echo off
echo Building Competitive 4K 500+ FPS Game System...
echo Optimized for competitive players and 4K high refresh rate monitors
echo ===============================================

REM Check for C++ compiler
where g++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set COMPILER=g++
    set COMPILE_FLAGS=-std=c++17 -O3 -march=native -mtune=native -flto -ffast-math -funroll-loops -fomit-frame-pointer -DNDEBUG
    set LINK_FLAGS=-lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32
    set OUTPUT=competitive_4k_game.exe
    set ULTRA_OUTPUT=ultra_4k_500fps_demo.exe
    echo Found g++ compiler
    goto :compile
)

where cl >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set COMPILER=cl
    set COMPILE_FLAGS=/std:c++17 /O2 /Ox /Ot /GL /arch:AVX2 /DNDEBUG
    set LINK_FLAGS=/SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF /LTCG
    set OUTPUT=competitive_4k_game.exe
    set ULTRA_OUTPUT=ultra_4k_500fps_demo.exe
    echo Found MSVC compiler
    goto :compile
)

echo No C++ compiler found! Please install MinGW or Visual Studio.
echo.
echo For MinGW: Download from https://www.mingw-w64.org/
echo For Visual Studio: Download Visual Studio Community (free)
echo.
pause
exit /b 1

:compile
echo.
echo Compiling Competitive 4K Game System...
echo Compiler: %COMPILER%
echo Flags: %COMPILE_FLAGS%
echo.

REM Create bin directory if it doesn't exist
if not exist "bin" mkdir bin

echo Building main competitive game with ultra-fast engine integration...
%COMPILER% %COMPILE_FLAGS% src\fps_game_engine.cpp src\ultra_fast_engine.cpp src\glad.c src\glfw3.c -o bin\%OUTPUT% %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo Main competitive game compiled successfully!
) else (
    echo Failed to compile main competitive game!
    goto :error
)

echo.
echo Building ultra 4K 500+ FPS demo...
%COMPILER% %COMPILE_FLAGS% src\ultra_4k_500fps_demo.cpp src\ultra_fast_engine.cpp src\glad.c src\glfw3.c -o bin\%ULTRA_OUTPUT% %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo Ultra 4K demo compiled successfully!
) else (
    echo Failed to compile ultra 4K demo!
    goto :error
)

echo.
echo Building competitive 4K demo...
%COMPILER% %COMPILE_FLAGS% src\competitive_4k_demo.cpp src\fps_game_engine.cpp src\ultra_fast_engine.cpp src\glad.c src\glfw3.c -o bin\competitive_4k_demo.exe %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo Competitive 4K demo compiled successfully!
) else (
    echo Failed to compile competitive 4K demo!
    goto :error
)

echo.
echo Building potato mode demo...
%COMPILER% %COMPILE_FLAGS% src\potato_mode_demo.cpp src\fps_game_engine.cpp src\potato_mode_optimizer.cpp src\ultra_fast_engine.cpp src\glad.c src\glfw3.c -o bin\potato_mode_demo.exe %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo Potato mode demo compiled successfully!
) else (
    echo Failed to compile potato mode demo!
    goto :error
)

echo.
echo Building theme park demo...
%COMPILER% %COMPILE_FLAGS% src\theme_park_demo.cpp src\theme_park_generator.cpp src\fps_game_engine.cpp src\glad.c src\glfw3.c -o bin\theme_park_demo.exe %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo Theme park demo compiled successfully!
) else (
    echo Failed to compile theme park demo!
    goto :error
)

echo.
echo Building wasteland serpent demo...
%COMPILER% %COMPILE_FLAGS% src\wasteland_serpent_demo.cpp src\wasteland_serpent_system.cpp src\fps_game_engine.cpp src\glad.c src\glfw3.c -o bin\wasteland_serpent_demo.exe %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo Wasteland serpent demo compiled successfully!
    goto :success
) else (
    echo Failed to compile wasteland serpent demo!
    goto :error
)

:success
echo.
echo ===============================================
echo Competitive 4K Game System compiled successfully!
echo ===============================================
echo.
echo Executables:
echo - Main Game: bin\%OUTPUT%
echo - Ultra 4K Demo: bin\%ULTRA_OUTPUT%
echo - Competitive 4K Demo: bin\competitive_4k_demo.exe
echo - Potato Mode Demo: bin\potato_mode_demo.exe
echo - Theme Park Demo: bin\theme_park_demo.exe
echo - Wasteland Serpent Demo: bin\wasteland_serpent_demo.exe
echo.
echo Features:
echo - Competitive graphics mode for maximum FPS
echo - 4K resolution optimization (3840x2160)
echo - Target: 500+ FPS on high refresh rate monitors
echo - Ultra-fast rendering with minimal overhead
echo - Adaptive quality scaling
echo - Optimized for competitive play
echo.
echo Graphics Modes Available:
echo - ULTRA: Maximum quality
echo - HIGH: High quality
echo - MEDIUM: Balanced quality
echo - LOW: Low quality
echo - COMPETITIVE: Ultra-fast for competitive play
echo - CUSTOM: User-defined settings
echo.
echo Competitive Mode Features:
echo - Disables all non-essential visual effects
echo - Optimizes for maximum frame rate
echo - Reduces input lag
echo - Minimal resource usage
echo - 4K optimization for high refresh rate monitors
echo.
echo Controls:
echo - WASD: Move player
echo - Mouse: Look around
echo - F1: Toggle debug info
echo - F2: Toggle adaptive quality
echo - F3: Switch between 500/1000 FPS target
echo - F4: Toggle competitive mode
echo - F5: Toggle 4K optimization
echo - ESC: Exit
echo.
echo Run with:
echo - Main Game: bin\%OUTPUT%
echo - Ultra Demo: bin\%ULTRA_OUTPUT%
echo - Competitive Demo: bin\competitive_4k_demo.exe
echo - Potato Demo: bin\potato_mode_demo.exe
echo - Theme Park Demo: bin\theme_park_demo.exe
echo - Wasteland Serpent Demo: bin\wasteland_serpent_demo.exe
echo.
echo This system is designed for competitive players
echo who need maximum FPS at 4K resolution.
echo.
goto :end

:error
echo.
echo ===============================================
echo Compilation failed!
echo ===============================================
echo.
echo Please check:
echo 1. All source files exist
echo 2. Compiler is properly installed
echo 3. Required libraries are available
echo 4. Ultra-fast engine files are present
echo.

:end
pause
