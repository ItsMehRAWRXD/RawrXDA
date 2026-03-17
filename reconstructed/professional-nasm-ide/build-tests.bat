@echo off
REM Build and run Ollama integration tests

echo === Building Ollama Integration Test ===

REM Build main ollama_native.asm
nasm -f win64 -DPLATFORM_WIN src\ollama_native.asm -o build\ollama_native.obj
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build ollama_native.asm
    exit /b 1
)

REM Build json_parser.asm
nasm -f win64 -DPLATFORM_WIN src\json_parser.asm -o build\json_parser.obj
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build json_parser.asm
    exit /b 1
)

REM Build test harness
nasm -f win64 -DPLATFORM_WIN tests\test_ollama_integration.asm -o build\test_ollama_integration.obj
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build test harness
    exit /b 1
)

echo === Build Complete ===
echo.
echo Note: Linking would require a C runtime or manual PE creation.
echo Object files created successfully in build\ directory.
echo.
echo Files created:
dir /B build\*.obj

exit /b 0
