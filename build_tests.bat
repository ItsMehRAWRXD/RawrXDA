@echo off
REM build_tests.bat - Build and run AgentHotPatcher tests on Windows

echo === Building AgentHotPatcher Test Suite ===

REM Create build directory
if not exist build_tests mkdir build_tests
cd build_tests

REM Configure with CMake
echo Configuring with CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release

REM Build the project
echo Building tests...
cmake --build . --config Release

echo === Running Tests ===

REM Run unit tests
echo Running unit tests...
.\Release\test_agent_hot_patcher.exe

REM Run integration tests
echo Running integration tests...
.\Release\test_agent_hot_patcher_integration.exe

echo === Test Suite Completed ===