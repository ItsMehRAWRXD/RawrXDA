@echo off
echo Starting Professional NASM IDE - NASM-Only Mode
echo.

REM Build core ASM modules (assumes NASM and GCC/LD in PATH)
if not exist "build" mkdir build
if not exist "lib" mkdir lib

echo [1] Running build-asm-core.bat
call build-asm-core.bat
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] build-asm-core.bat failed. Aborting.
    pause
    exit /b 1
)

echo [2] Assemble and link main IDE executable
nasm -f win64 src\nasm_ide_integration.asm -o build\nasm_ide_integration.obj
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] NASM assembly failed
    pause
    exit /b 1
)

gcc -o bin\nasm_ide.exe build\nasm_ide_integration.obj -luser32 -lkernel32
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] Linking failed. Ensure MinGW/GCC is installed and in PATH.
    pause
    exit /b 1
)

echo Running NASM IDE executable...
bin\nasm_ide.exe

echo NASM-only IDE exited.
pause