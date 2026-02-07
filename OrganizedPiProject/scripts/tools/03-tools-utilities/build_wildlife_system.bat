@echo off
echo Building Wildlife System with Rhythm Integration...

:: Set compiler and flags
set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -I./include -I./src
set LIBS=-lglfw -lGL -lGLEW -lm

:: Create build directory
if not exist build mkdir build
cd build

:: Compile wildlife system components
echo Compiling wildlife system components...
%CXX% %CXXFLAGS% -c ../src/wildlife_generator.cpp -o wildlife_generator.o
if errorlevel 1 goto error

echo Compiling simple wildlife generator...
%CXX% %CXXFLAGS% -c ../src/simple_wildlife_demo.cpp -o simple_wildlife_demo.o
if errorlevel 1 goto error

echo Compiling integrated wildlife demo...
%CXX% %CXXFLAGS% -c ../src/wildlife_demo_integrated.cpp -o wildlife_demo_integrated.o
if errorlevel 1 goto error

echo Compiling map skeleton generator...
%CXX% %CXXFLAGS% -c ../src/map_skeleton_generator.cpp -o map_skeleton_generator.o
if errorlevel 1 goto error

echo Compiling map skeleton demo...
%CXX% %CXXFLAGS% -c ../src/map_skeleton_demo.cpp -o map_skeleton_demo.o
if errorlevel 1 goto error

:: Link wildlife demos
echo Linking simple wildlife demo...
%CXX% wildlife_generator.o simple_wildlife_demo.o %LIBS% -o simple_wildlife_demo.exe
if errorlevel 1 goto error

echo Linking integrated wildlife demo...
%CXX% wildlife_generator.o wildlife_demo_integrated.o %LIBS% -o wildlife_demo_integrated.exe
if errorlevel 1 goto error

echo Linking map skeleton demo...
%CXX% map_skeleton_generator.o map_skeleton_demo.o %LIBS% -o map_skeleton_demo.exe
if errorlevel 1 goto error

echo.
echo ===============================================
echo Wildlife System Build Complete!
echo ===============================================
echo.
echo Generated executables:
echo   - simple_wildlife_demo.exe
echo   - wildlife_demo_integrated.exe  
echo   - map_skeleton_demo.exe
echo.
echo Data files:
echo   - ../data/species_definitions.json
echo   - ../data/spawn_zones.json
echo.
echo To run the demos:
echo   ./simple_wildlife_demo.exe
echo   ./wildlife_demo_integrated.exe
echo   ./map_skeleton_demo.exe
echo.
echo ===============================================

goto end

:error
echo.
echo ===============================================
echo BUILD FAILED!
echo ===============================================
echo Check the error messages above.
echo Make sure you have the required dependencies:
echo   - GLFW3
echo   - OpenGL
echo   - GLEW
echo.
pause
exit /b 1

:end
cd ..
pause
