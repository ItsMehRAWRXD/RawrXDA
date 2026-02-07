@echo off
echo ========================================
echo      SUNSHINE ENGINE STARTUP
echo ========================================
echo.

echo [1/6] Checking system requirements...
where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: GCC compiler not found!
    echo Please install MinGW-w64 or Visual Studio Build Tools
    pause
    exit /b 1
)
echo ✓ GCC compiler found

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found!
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)
echo ✓ CMake found

echo.
echo [2/6] Creating build directory...
if not exist "build" mkdir build
cd build
echo ✓ Build directory ready

echo.
echo [3/6] Configuring project with CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed!
    echo Please check your CMake installation and dependencies
    pause
    exit /b 1
)
echo ✓ CMake configuration successful

echo.
echo [4/6] Building the FPS system...
cmake --build . --config Release -j4
if %errorlevel% neq 0 (
    echo ERROR: Build failed!
    echo Please check the error messages above
    pause
    exit /b 1
)
echo ✓ Build successful

echo.
echo [5/6] Checking for required libraries...
if not exist "..\include\glad\glad.h" (
    echo WARNING: GLAD library not found
    echo The system will attempt to download it automatically
)

if not exist "..\include\GLFW\glfw3.h" (
    echo WARNING: GLFW library not found
    echo The system will attempt to download it automatically
)

if not exist "..\include\glm\glm.hpp" (
    echo WARNING: GLM library not found
    echo The system will attempt to download it automatically
)

echo.
echo [6/6] Starting the FPS system...
echo.
echo ========================================
echo    FPS SYSTEM FEATURES AVAILABLE:
echo ========================================
echo ✓ Innovative FPS Mode with AI assistance
echo ✓ Advanced Movement (bunny hop, wall run, slide)
echo ✓ Comprehensive Weapon System (8 weapon types)
echo ✓ Destructible Environment
echo ✓ Enhanced Particle Effects
echo ✓ 3D Positional Audio
echo ✓ Modern HUD System
echo ✓ Multiplayer Networking
echo ✓ 200-Player Warzone Support
echo ✓ Multiple Game Modes
echo ========================================
echo.

if exist "Release\FPSGame.exe" (
    echo Starting FPS Game...
    Release\FPSGame.exe
) else if exist "FPSGame.exe" (
    echo Starting FPS Game...
    FPSGame.exe
) else (
    echo ERROR: FPS Game executable not found!
    echo Please check the build output for errors
    echo.
    echo Available files in build directory:
    dir /b *.exe
    echo.
    pause
    exit /b 1
)

echo.
echo FPS System has been shut down.
pause
