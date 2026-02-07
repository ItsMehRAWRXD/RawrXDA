@echo off
echo Building Ultra 4K 500+ FPS Demo...
echo Optimized for 4K resolution at 500+ FPS
echo ===============================================

REM Check for C++ compiler
where g++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set COMPILER=g++
    set COMPILE_FLAGS=-std=c++17 -O3 -march=native -mtune=native -flto -ffast-math -funroll-loops -fomit-frame-pointer -DNDEBUG
    set LINK_FLAGS=-lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32
    set OUTPUT=ultra_4k_500fps_demo.exe
    echo Found g++ compiler
    goto :compile
)

where cl >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set COMPILER=cl
    set COMPILE_FLAGS=/std:c++17 /O2 /Ox /Ot /GL /arch:AVX2 /DNDEBUG
    set LINK_FLAGS=/SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF /LTCG
    set OUTPUT=ultra_4k_500fps_demo.exe
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
echo Compiling Ultra 4K 500+ FPS Demo...
echo Compiler: %COMPILER%
echo Flags: %COMPILE_FLAGS%
echo.

REM Create bin directory if it doesn't exist
if not exist "bin" mkdir bin

REM Compile the ultra 4K demo
%COMPILER% %COMPILE_FLAGS% src\ultra_4k_500fps_demo.cpp src\ultra_fast_engine.cpp src\glad.c src\glfw3.c -o bin\%OUTPUT% %LINK_FLAGS%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===============================================
    echo Ultra 4K 500+ FPS Demo compiled successfully!
    echo ===============================================
    echo.
    echo Executable: bin\%OUTPUT%
    echo.
    echo Features:
    echo - Optimized for 4K resolution (3840x2160)
    echo - Target: 500+ FPS on high refresh rate monitors
    echo - Ultra-fast rendering with minimal overhead
    echo - Competitive graphics settings
    echo - Adaptive quality scaling
    echo.
    echo Controls:
    echo - WASD: Move player
    echo - Mouse: Look around
    echo - F1: Toggle debug info
    echo - F2: Toggle adaptive quality
    echo - F3: Switch between 500/1000 FPS target
    echo - ESC: Exit
    echo.
    echo Run with: bin\%OUTPUT%
    echo.
    echo This demo is designed for competitive players
    echo who need maximum FPS at 4K resolution.
    echo.
) else (
    echo.
    echo ===============================================
    echo Compilation failed!
    echo ===============================================
    echo.
    echo Please check:
    echo 1. All source files exist
    echo 2. Compiler is properly installed
    echo 3. Required libraries are available
    echo.
)

pause
