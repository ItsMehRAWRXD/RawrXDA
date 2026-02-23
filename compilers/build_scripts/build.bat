@echo off
REM build.bat - Build script for AgentHotPatcher on Windows

REM Create build directory
mkdir build
cd build

REM Configure with CMake
cmake ..

REM Build the project
cmake --build . --config Release

echo Build completed successfully!
echo Run tests with:
echo   .\test\Release\TestAgentHotPatcher.exe
echo   .\test_qt\Release\TestAgentHotPatcherQtTest.exe