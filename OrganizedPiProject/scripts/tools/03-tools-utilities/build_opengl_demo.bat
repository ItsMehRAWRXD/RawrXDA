@echo off
echo Building OpenGL Demo with Simple Compilation
echo ============================================

REM Check if we have a C++ compiler
where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    where cl >nul 2>nul
    if %ERRORLEVEL% neq 0 (
        echo No C++ compiler found! Please install MinGW or Visual Studio.
        pause
        exit /b 1
    )
    set COMPILER=cl
    set COMPILE_FLAGS=/EHsc /I"include" /I"include\glad" /I"include\GLFW" /I"include\glm"
    set LINK_FLAGS=opengl32.lib user32.lib gdi32.lib shell32.lib
    set OUTPUT=opengl_demo.exe
) else (
    set COMPILER=g++
    set COMPILE_FLAGS=-std=c++17 -I"include" -I"include\glad" -I"include\GLFW" -I"include\glm" -O2
    set LINK_FLAGS=-lopengl32 -luser32 -lgdi32 -lshell32
    set OUTPUT=opengl_demo.exe
)

echo Using compiler: %COMPILER%
echo.

REM Create output directory
if not exist bin mkdir bin

REM Compile the simple OpenGL demo
echo Compiling simple OpenGL demo...
%COMPILER% %COMPILE_FLAGS% src\simple_opengl_demo.cpp src\glad.c src\glfw3.c -o bin\%OUTPUT% %LINK_FLAGS%

if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    echo.
    echo Make sure you have:
    echo   1. A C++ compiler (MinGW or Visual Studio)
    echo   2. OpenGL development libraries
    echo   3. All include files in the include directory
    pause
    exit /b 1
)

echo.
echo ================================================
echo Build completed successfully!
echo.
echo Executable created: bin\%OUTPUT%
echo.
echo To run the demo:
echo   bin\%OUTPUT%
echo.
echo Press any key to run the demo now...
pause >nul

REM Run the demo
echo Running OpenGL demo...
bin\%OUTPUT%

echo.
echo Demo finished. Press any key to exit...
pause >nul
