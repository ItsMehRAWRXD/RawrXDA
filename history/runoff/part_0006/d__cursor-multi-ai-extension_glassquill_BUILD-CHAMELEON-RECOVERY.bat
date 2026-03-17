@echo off
echo ========================================
echo   GlassQuill Chameleon IDE Builder
echo ========================================
echo.

rem Check if we're in the right directory
if not exist "src\glassquill.cpp" (
    echo ✗ ERROR: glassquill.cpp not found!
    echo.
    echo Make sure you're running this from:
    echo D:\cursor-multi-ai-extension\glassquill\
    echo.
    pause
    exit /b 1
)

rem Check for Visual Studio compiler
where cl >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ✗ ERROR: Visual Studio C++ compiler not found!
    echo.
    echo Please run this from:
    echo "x64 Native Tools Command Prompt for VS"
    echo.
    echo You can find it in Start Menu under:
    echo Visual Studio 2019/2022 ^> x64 Native Tools Command Prompt
    echo.
    pause
    exit /b 1
)

echo ✓ Found source file: src\glassquill.cpp
echo ✓ Found Visual Studio compiler
echo.
echo Building chameleon IDE...

cd src
cl /EHsc /O2 /std:c++17 glassquill.cpp /Fe:../GlassQuill-Chameleon.exe /link user32.lib gdi32.lib opengl32.lib dwmapi.lib
cd ..

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo ✓ BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Executable created: GlassQuill-Chameleon.exe
    echo.
    echo CHAMELEON FEATURES:
    echo • Rainbow text that flows like a chameleon's skin
    echo • HSV color cycling with time-based animations
    echo • Pulsing glow effects that breathe with text
    echo • Transparent window with opacity control
    echo • Professional editor with line numbers
    echo • Real-time 60fps color transformations
    echo.
    echo CONTROLS:
    echo • F4 = Toggle chameleon effect ON/OFF
    echo • Drag opacity slider = Adjust transparency
    echo • Click editor area = Focus for typing
    echo • Mouse wheel = Scroll through code
    echo • Arrow keys = Navigate cursor
    echo • ESC = Exit application
    echo.
    echo Starting your chameleon IDE...
    start GlassQuill-Chameleon.exe
) else (
    echo.
    echo ========================================
    echo ✗ BUILD FAILED!
    echo ========================================
    echo.
    echo Common fixes:
    echo • Run from x64 Native Tools Command Prompt
    echo • Install Visual Studio Build Tools
    echo • Check Windows SDK is installed
    echo.
)

pause