@echo off
echo Building Mega Scale Multiplayer Engine Demo...
echo.

REM Check for C++ compiler
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Using g++ compiler...
    g++ -std=c++20 -O3 -Wall -Wextra -I. -Iinclude ^
        src/mega_scale_engine.cpp ^
        src/mega_scale_demo.cpp ^
        -o mega_scale_demo.exe ^
        -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32
    if %errorlevel% equ 0 (
        echo Build successful! Running demo...
        echo.
        mega_scale_demo.exe
    ) else (
        echo Build failed with g++
    )
    goto :end
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Using MSVC compiler...
    cl /std:c++20 /O2 /EHsc /I. /Iinclude ^
        src/mega_scale_engine.cpp ^
        src/mega_scale_demo.cpp ^
        /Fe:mega_scale_demo.exe ^
        /link opengl32.lib gdi32.lib user32.lib kernel32.lib
    if %errorlevel% equ 0 (
        echo Build successful! Running demo...
        echo.
        mega_scale_demo.exe
    ) else (
        echo Build failed with MSVC
    )
    goto :end
)

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Using clang++ compiler...
    clang++ -std=c++20 -O3 -Wall -Wextra -I. -Iinclude ^
        src/mega_scale_engine.cpp ^
        src/mega_scale_demo.cpp ^
        -o mega_scale_demo.exe ^
        -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32
    if %errorlevel% equ 0 (
        echo Build successful! Running demo...
        echo.
        mega_scale_demo.exe
    ) else (
        echo Build failed with clang++
    )
    goto :end
)

echo No C++ compiler found! Please install g++, MSVC, or clang++
echo.

:end
pause
