@echo off
cd /d "D:\cursor-multi-ai-extension\glassquill"

echo Building GlassQuill Chameleon IDE...
echo ====================================

if not exist "bin" mkdir bin
cd src

cl /EHsc /O2 /std:c++17 ^
   glassquill_chameleon.cpp ^
   /Fe:../bin/glassquill_chameleon.exe ^
   /link user32.lib gdi32.lib opengl32.lib dwmapi.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful!
    echo ✓ Executable: D:\cursor-multi-ai-extension\glassquill\bin\glassquill_chameleon.exe
    echo.
    echo Features included:
    echo • Chameleon color-changing text effect
    echo • Built-in GLSL vertex/fragment shaders
    echo • Syntax highlighting for C++, GLSL, headers
    echo • Real-time transparency controls
    echo • File type auto-detection
    echo • Line numbers and professional editor layout
    echo.
    echo Controls:
    echo • F4 = Toggle chameleon effect
    echo • Ctrl+S = Save file
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