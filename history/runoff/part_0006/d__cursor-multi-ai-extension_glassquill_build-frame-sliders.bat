@echo off
cd /d "D:\cursor-multi-ai-extension\glassquill"

echo Building GlassQuill Frame Slider IDE...
echo ========================================

if not exist "bin" mkdir bin
cd src

cl /EHsc /O2 /std:c++17 ^
   glassquill_frame_sliders.cpp ^
   /Fe:../bin/GlassQuill-FrameSliders.exe ^
   /link user32.lib gdi32.lib opengl32.lib dwmapi.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful!
    echo ✓ Executable: D:\cursor-multi-ai-extension\glassquill\bin\GlassQuill-FrameSliders.exe
    echo.
    echo FRAME SLIDER FEATURES:
    echo • Left Frame = Text Editor RGBA sliders ^(4 vertical sliders^)
    echo • Right Frame = Syntax Highlighter RGBA sliders ^(4 vertical sliders^)
    echo • Top Frame = Experimental Effects ^(5 horizontal sliders^)
    echo   - Glow: Pulsing rainbow halos
    echo   - Diamonds: Animated crystal patterns  
    echo   - Water: Flowing wave effects
    echo   - Jello: Wobbling segments
    echo   - Tie-Dye: Spiral color bursts
    echo • Bottom Frame = Additional experimental effects
    echo • Chameleon text with HSV color cycling
    echo • Professional editor with line numbers
    echo.
    echo Controls:
    echo • Drag any frame edge to adjust sliders
    echo • F4 = Toggle chameleon effect
    echo • Mouse wheel = Scroll editor
    echo • Arrow keys = Navigate text
    echo • ESC = Exit
    echo.
    echo The entire window border is now your control panel!
    echo.
    cd ../bin
    start GlassQuill-FrameSliders.exe
) else (
    echo.
    echo ✗ Build failed!
    echo Check that Visual Studio Build Tools are installed
    echo Error details above show what went wrong
)

cd ..
pause