@echo off
echo Building Wildlife System Demo...

REM Create build directory
if not exist build_wildlife mkdir build_wildlife
cd build_wildlife

REM Compile the wildlife demo
g++ -std=c++17 -O2 -o wildlife_demo.exe ../src/wildlife_demo.cpp

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Executable location: build_wildlife\wildlife_demo.exe
    echo.
    echo To run the wildlife demo:
    echo cd build_wildlife
    echo wildlife_demo.exe
    echo.
    echo Features:
    echo - 20 different animal types
    echo - Wild horses that can be ridden
    echo - Animal behavior and AI
    echo - Pack and herd behavior
    echo - Hunting and predation
    echo - Animal taming and training
    echo - Environmental interactions
    echo - Advanced wildlife mechanics
    echo.
) else (
    echo.
    echo Build failed!
    echo Check the error messages above.
    echo.
)

pause
