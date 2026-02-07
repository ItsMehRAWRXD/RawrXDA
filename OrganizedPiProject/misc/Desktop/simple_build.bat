@echo off
echo 🎮 Building Browser Strike Game Engine (Simple Build)...

REM Check if we have a C++ compiler
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

where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using GCC compiler...
    goto :gcc
)

echo ❌ No C++ compiler found!
echo Please install Visual Studio, MinGW, or another C++ compiler.
exit /b 1

:msvc
echo Building with MSVC...
cl /EHsc /std:c++17 /I. src\simple_main.cpp /Fe:BrowserStrike.exe
if %ERRORLEVEL% EQU 0 (
    echo ✅ Build successful!
    echo 🚀 Run with: BrowserStrike.exe
) else (
    echo ❌ Build failed!
)
goto :end

:gcc
echo Building with GCC...
g++ -std=c++17 -I. src/simple_main.cpp -o BrowserStrike.exe
if %ERRORLEVEL% EQU 0 (
    echo ✅ Build successful!
    echo 🚀 Run with: BrowserStrike.exe
) else (
    echo ❌ Build failed!
    echo Note: This is a simplified version that doesn't require external libraries
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
