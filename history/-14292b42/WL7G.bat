@echo off
cd /d "D:\cursor-multi-ai-extension\glassquill"

echo Building GlassQuill Chameleon IDE...
echo ====================================

if not exist "bin" mkdir bin
cd src

cl /EHsc /O2 /std:c++17 ^
   glassquill.cpp ^
   /Fe:../bin/glassquill_marketplace.exe ^
   /link user32.lib gdi32.lib opengl32.lib dwmapi.lib shell32.lib ole32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful!
    echo ✓ Executable: D:\cursor-multi-ai-extension\glassquill\bin\glassquill_marketplace.exe
    echo.
    echo Features included:
    echo • Chameleon color-changing text effect
    echo • Software-based HSV color cycling
    echo • Marketplace extension browser (Ctrl+M)
    echo • Extension auto-installation system
    echo • Real-time transparency controls
    echo • Line numbers and professional editor layout
    echo.
    echo Controls:
    echo • F4 = Toggle chameleon effect
    echo • Ctrl+M = Browse marketplace and install extension
    echo • Ctrl+O = Open file
    echo • Mouse wheel = Scroll
    echo • ESC = Exit
    echo.
    cd ../bin
    start glassquill_chameleon.exe
) else (
    echo.
    echo ✗ Build failed!
    echo Check that Visual Studio Build Tools are installed
)

cd ..
pause