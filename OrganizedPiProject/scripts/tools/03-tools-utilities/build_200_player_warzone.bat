@echo off
echo ========================================
echo Building 200-Player Warzone Demo
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
echo Building 200-Player Warzone Demo...
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
echo To run the 200-Player Warzone Demo:
echo   cd build
echo   .\Release\Warzone200Player.exe
echo.
echo 200-Player Warzone Features:
echo   - 4km x 4km massive warzone map
echo   - 200 players maximum capacity
echo   - 32+ themed combat areas
echo   - 800+ loot spawn points
echo   - 25+ vehicle spawns
echo   - 40-minute game duration
echo   - Spectator mode for large-scale viewing
echo   - Performance optimizations for 200 players
echo   - LOD (Level of Detail) rendering
echo   - Distance culling for performance
echo.
echo Controls:
echo   TAB - Toggle Spectator/Player Mode
echo   WASD - Move (Q/E for up/down in spectator)
echo   Mouse - Look around
echo   SPACE - Jump (Player mode)
echo   SHIFT - Sprint/Fast spectator
echo   CTRL - Crouch (Player mode)
echo   C - Slide (Player mode)
echo   B - Bunny hop (Player mode)
echo   G - Grapple (Player mode)
echo   1-5 - Switch weapons
echo   R - Reload
echo   Left Click - Fire
echo   F1-F4 - Toggle UI panels
echo   P - Pause
echo   ESC - Release mouse
echo.
echo Performance Tips:
echo   - Use Spectator mode (TAB) for best performance
echo   - Higher resolution may impact performance
echo   - LOD system automatically reduces detail at distance
echo   - Frame rate limited to 60 FPS for stability
echo.
pause
