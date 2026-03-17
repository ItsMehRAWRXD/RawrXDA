@echo off
echo Building working Native IDE with hotkeys...

cl /nologo /O2 src\working.cpp /Fe:NativeIDE-Working.exe kernel32.lib user32.lib gdi32.lib comctl32.lib

if exist NativeIDE-Working.exe (
    echo ✅ Native IDE built successfully!
    echo.
    echo 🔥 Hotkeys Available:
    echo   Ctrl+A - Agent Mode
    echo   Ctrl+O - Orchestra
    echo   Ctrl+G - Git Operations  
    echo   Ctrl+C - Copilot
    echo   F5     - Compile & Run
    echo.
    echo Starting IDE...
    start NativeIDE-Working.exe
) else (
    echo ❌ Build failed - check if Visual Studio is installed
)
pause