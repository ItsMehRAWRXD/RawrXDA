@echo off
echo =====================================================================
echo Professional NASM IDE - DirectX Build System
echo Building gorgeous DirectX 11 text editor...
echo =====================================================================

REM Create build directory if it doesn't exist
if not exist "build" mkdir build
if not exist "bin" mkdir bin

echo [1/3] Assembling main DirectX IDE...
nasm -f win64 -o build\dx_ide_main.obj src\dx_ide_main.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble dx_ide_main.asm
    pause
    exit /b 1
)

echo [2/3] Assembling text editor engine...
nasm -f win64 -o build\text_editor_engine.obj src\text_editor_engine.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble text_editor_engine.asm
    pause
    exit /b 1
)

echo [3/3] Linking Professional NASM IDE...
gcc -m64 -mwindows -o bin\professional_nasm_ide.exe ^
    build\dx_ide_main.obj ^
    build\text_editor_engine.obj ^
    -lkernel32 -luser32 -lgdi32 -ld3d11 -ldxgi -ld2d1 -ldwrite

if errorlevel 1 (
    echo ERROR: Failed to link Professional NASM IDE
    pause
    exit /b 1
)

echo =====================================================================
echo SUCCESS! Professional NASM IDE with DirectX 11 text rendering built!
echo =====================================================================
echo.
echo Executable: bin\professional_nasm_ide.exe
echo.
echo Features implemented:
echo   ✅ DirectX 11 hardware-accelerated rendering
echo   ✅ DirectWrite text engine with font support  
echo   ✅ Direct2D graphics pipeline
echo   ✅ Text editor with cursor management
echo   ✅ Professional NASM syntax highlighting
echo.
echo Ready to run gorgeous DirectX text editor!
echo.
pause