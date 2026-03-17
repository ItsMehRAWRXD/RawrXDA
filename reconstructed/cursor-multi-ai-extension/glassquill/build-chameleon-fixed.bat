@echo off
cd /d "D:\cursor-multi-ai-extension\glassquill"

echo Building GlassQuill Chameleon IDE (Fixed Edition)...
echo ===================================================

if not exist "bin" mkdir bin
cd src

cl /EHsc /O2 /std:c++17 ^
   glassquill_chameleon_fixed.cpp ^
   /Fe:../bin/glassquill_chameleon.exe ^
   /link user32.lib gdi32.lib opengl32.lib dwmapi.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful!
    echo ✓ Executable: D:\cursor-multi-ai-extension\glassquill\bin\glassquill_chameleon.exe
    echo.
    echo FIXED FEATURES:
    echo • Software-based chameleon color cycling ^(no shader extensions needed^)
    echo • HSV-to-RGB color transformation with glow effects
    echo • Built-in syntax highlighting for C++, GLSL, headers
    echo • Real-time transparency controls
    echo • File type auto-detection
    echo • Line numbers and professional editor layout
    echo • Compatible with basic OpenGL 1.1 ^(no extensions required^)
    echo.
    echo Controls:
    echo • F4 = Toggle chameleon effect
    echo • Click chameleon button = Toggle effect
    echo • Drag opacity slider = Adjust transparency
    echo • Ctrl+S = Save file to D:\cursor-multi-ai-extension\glassquill\test.cpp
    echo • Ctrl+O = Open D:\cursor-multi-ai-extension\glassquill\src\glassquill.cpp
    echo • Mouse wheel = Scroll editor
    echo • Arrow keys = Navigate text
    echo • ESC = Exit
    echo.
    echo The chameleon effect now uses software HSV color cycling!
    echo Your text will shimmer with rainbow colors that flow like a real chameleon!
    echo.
    cd ../bin
    start glassquill_chameleon.exe
) else (
    echo.
    echo ✗ Build failed!
    echo Check that Visual Studio Build Tools are installed
    echo Error details above show what went wrong
)

cd ..
pause