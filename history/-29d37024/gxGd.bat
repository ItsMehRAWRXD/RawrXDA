@echo off
REM =====================================================================
REM Build Pure ASM DirectX IDE
REM Assembles and links dx_ide_main.asm, file_system.asm, chat_pane.asm
REM =====================================================================

echo.
echo ========================================
echo   Building Pure ASM DirectX IDE
echo ========================================
echo.

REM Check for NASM
where nasm >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] NASM not found in PATH
    echo Please install NASM from https://www.nasm.us/
    pause
    exit /b 1
)

echo [OK] NASM found: 
nasm -v
echo.

REM Create build directories
if not exist build mkdir build
if not exist bin mkdir bin

echo [1/4] Assembling dx_ide_main.asm...
nasm -f win64 src\dx_ide_main.asm -o build\dx_ide_main.obj
if %errorlevel% neq 0 (
    echo [ERROR] Assembly failed for dx_ide_main.asm
    pause
    exit /b 1
)
echo [OK] dx_ide_main.obj created

echo [2/4] Assembling file_system.asm...
nasm -f win64 src\file_system.asm -o build\file_system.obj
if %errorlevel% neq 0 (
    echo [ERROR] Assembly failed for file_system.asm
    pause
    exit /b 1
)
echo [OK] file_system.obj created

echo [3/4] Assembling chat_pane.asm...
nasm -f win64 src\chat_pane.asm -o build\chat_pane.obj
if %errorlevel% neq 0 (
    echo [ERROR] Assembly failed for chat_pane.asm
    pause
    exit /b 1
)
echo [OK] chat_pane.obj created

echo [4/4] Linking executable...

REM Try MSVC linker first
where link >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Using MSVC linker
    link /ENTRY:WinMain /SUBSYSTEM:WINDOWS /OUT:bin\nasm_ide_dx.exe ^
        build\dx_ide_main.obj build\file_system.obj build\chat_pane.obj ^
        kernel32.lib user32.lib gdi32.lib comdlg32.lib ^
        d3d11.lib dxgi.lib d2d1.lib dwrite.lib
    
    if %errorlevel% neq 0 (
        echo [ERROR] Linking failed
        pause
        exit /b 1
    )
) else (
    REM Try MinGW linker
    where ld >nul 2>&1
    if %errorlevel% equ 0 (
        echo [INFO] Using MinGW linker
        ld -m i386pep -e WinMain --subsystem windows -o bin\nasm_ide_dx.exe ^
            build\dx_ide_main.obj build\file_system.obj build\chat_pane.obj ^
            -lkernel32 -luser32 -lgdi32 -lcomdlg32 ^
            -ld3d11 -ldxgi -ld2d1 -ldwrite
        
        if %errorlevel% neq 0 (
            echo [ERROR] Linking failed
            pause
            exit /b 1
        )
    ) else (
        echo [ERROR] No linker found (tried link.exe and ld)
        echo Please install Visual Studio Build Tools or MinGW-w64
        pause
        exit /b 1
    )
)

echo.
echo ========================================
echo   Build Complete!
echo ========================================
echo.
echo Executable: bin\nasm_ide_dx.exe
echo.
echo To run the IDE:
echo   cd bin
echo   nasm_ide_dx.exe
echo.
pause
