@echo off
echo Building OpenGL Simple Demo - Turn-Key Solution
echo ================================================

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    echo Trying with Visual Studio...
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %ERRORLEVEL% neq 0 (
        echo Both CMake configurations failed!
        pause
        exit /b 1
    )
)

REM Build the project
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ================================================
echo Build completed successfully!
echo.
echo Executables created:
if exist bin\Release\MultiverseEngine.exe echo   - bin\Release\MultiverseEngine.exe
if exist bin\MultiverseEngine.exe echo   - bin\MultiverseEngine.exe
if exist Release\MultiverseEngine.exe echo   - Release\MultiverseEngine.exe
echo.
echo To run the simple OpenGL demo:
echo   cd build
echo   .\bin\Release\MultiverseEngine.exe
echo.
pause
