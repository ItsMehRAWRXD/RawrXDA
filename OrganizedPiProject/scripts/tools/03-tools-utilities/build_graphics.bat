@echo off
echo Building Browser Strike - Advanced Graphics Engine...

REM Check for compiler
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC compiler...
    goto :msvc
)

where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using GCC compiler...
    goto :gcc
)

echo No C++ compiler found!
echo Please install Visual Studio or MinGW
exit /b 1

:msvc
echo Building with MSVC...
cl /EHsc /std:c++17 /I. /Iinclude /Iinclude\GL ^
   src\graphics_main.cpp ^
   src\texture_manager.cpp ^
   src\hdr_renderer.cpp ^
   src\physics_world.cpp ^
   /Fe:BrowserStrikeGraphics.exe ^
   /link opengl32.lib glu32.lib user32.lib gdi32.lib ^
   /DGLAD_GL_IMPLEMENTATION
if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Run with: BrowserStrikeGraphics.exe
) else (
    echo Build failed!
)
goto :end

:gcc
echo Building with GCC...
g++ -std=c++17 -I. -Iinclude -Iinclude/GL ^
    src/graphics_main.cpp ^
    src/texture_manager.cpp ^
    src/hdr_renderer.cpp ^
    src/physics_world.cpp ^
    -o BrowserStrikeGraphics.exe ^
    -lopengl32 -lglu32 -luser32 -lgdi32 ^
    -DGLAD_GL_IMPLEMENTATION
if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Run with: BrowserStrikeGraphics.exe
) else (
    echo Build failed!
)
goto :end

:end
echo.
echo Advanced Graphics Features:
echo   - PBR Materials (Wood, Metal, Concrete, Stone)
echo   - HDR Rendering with Tone Mapping
echo   - Particle Effects (Explosions, Muzzle Flash, etc.)
echo   - Dynamic Lighting System
echo   - Shadow Mapping
echo   - XQZ Wireframe Mode
echo   - Procedural Texture Generation
echo.
echo Controls:
echo   WASD - Move around
echo   Mouse - Look around
echo   V - Toggle camera mode
echo   TAB - Toggle XQZ wireframe
echo   F - Create explosion
echo   G - Create muzzle flash
echo   ESC - Exit
