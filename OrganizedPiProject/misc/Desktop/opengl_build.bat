@echo off
echo Building Browser Strike OpenGL Engine...

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
   opengl_main.cpp ^
   /Fe:BrowserStrike.exe ^
   /link opengl32.lib glu32.lib user32.lib gdi32.lib
if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Run with: BrowserStrike.exe
) else (
    echo Build failed!
)
goto :end

:gcc
echo Building with GCC...
g++ -std=c++17 -I. -Iinclude -Iinclude/GL ^
    opengl_main.cpp ^
    -o BrowserStrike.exe ^
    -lopengl32 -lglu32 -luser32 -lgdi32
if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Run with: BrowserStrike.exe
) else (
    echo Build failed!
)
goto :end

:end
echo.
echo Controls:
echo   WASD - Move around
echo   Mouse - Look around
echo   V - Toggle camera mode
echo   W - Toggle wireframe
echo   ESC - Exit
