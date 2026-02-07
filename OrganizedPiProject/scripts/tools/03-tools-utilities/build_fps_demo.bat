@echo off
echo ========================================
echo Building FPS Demo - Advanced Features
echo ========================================

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring project with CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo Building FPS Demo...
cmake --build . --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo To run the FPS Demo:
echo   cd build
echo   .\Release\FPSDemo.exe
echo.
echo Features included:
echo   - Advanced Weapon System (8 weapon types)
echo   - Sophisticated Movement System (wall running, sliding, bunny hopping)
echo   - Dynamic Warzone Generator (16 themed areas)
echo   - Physics Integration
echo   - HDR Rendering
echo.
echo Controls:
echo   WASD - Move
echo   Mouse - Look around
echo   SPACE - Jump
echo   SHIFT - Sprint
echo   CTRL - Crouch
echo   C - Slide
echo   B - Bunny hop
echo   G - Grapple
echo   1-5 - Switch weapons
echo   R - Reload
echo   Left Click - Fire
echo   F1-F3 - Toggle UI
echo   P - Pause
echo   ESC - Release mouse
echo.
pause
