@echo off
echo Building Advanced MultiverseEngine with HDR + Physics...

REM Check for Visual Studio environment
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC compiler...
    goto :msvc
)

echo No C++ compiler found!
echo Please install Visual Studio or run from Developer Command Prompt
exit /b 1

:msvc
echo Building with MSVC...

REM Try to build the advanced version first
cl /EHsc /std:c++17 /I. /Iinclude ^
   src/advanced_main.cpp ^
   src/camera.cpp ^
   src/random_room_generator.cpp ^
   src/dust2_generator.cpp ^
   src/mirage_generator.cpp ^
   src/deathmatch.cpp ^
   src/network_stub.cpp ^
   src/physics_world.cpp ^
   src/hdr_renderer.cpp ^
   /Fe:MultiverseEngine_Advanced.exe ^
   /link opengl32.lib glu32.lib user32.lib gdi32.lib

if %ERRORLEVEL% EQU 0 (
    echo Advanced build successful!
    echo Run with: MultiverseEngine_Advanced.exe
) else (
    echo Advanced build failed, trying simple version...
    goto :simple
)

goto :end

:simple
echo Building simple version...

cl /EHsc /std:c++17 /I. /Iinclude ^
   src/main.cpp ^
   src/camera.cpp ^
   src/random_room_generator.cpp ^
   src/dust2_generator.cpp ^
   src/mirage_generator.cpp ^
   src/deathmatch.cpp ^
   src/network_stub.cpp ^
   /Fe:MultiverseEngine.exe ^
   /link opengl32.lib glu32.lib user32.lib gdi32.lib

if %ERRORLEVEL% EQU 0 (
    echo Simple build successful!
    echo Run with: MultiverseEngine.exe
) else (
    echo Build failed!
)

:end
echo.
echo Controls:
echo   WASD - Move around
echo   Mouse - Look around
echo   ESC - Exit
echo.
echo Advanced version features:
echo   - HDR lighting with tone-mapping
echo   - Physics simulation
echo   - Dust2 and Mirage maps
echo   - Real-time lighting
