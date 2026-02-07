@echo off
echo Building FPS Game Engine...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

REM Build the project
cmake --build . --config Release

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Executable location: build\bin\Release\FPSGameEngine.exe
    echo.
    echo To run the game:
    echo cd build\bin\Release
    echo FPSGameEngine.exe
    echo.
) else (
    echo.
    echo Build failed!
    echo Check the error messages above.
    echo.
)

pause