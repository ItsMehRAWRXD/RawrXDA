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

REM Resolve NASM command using PATH or common install locations
set "NASM_CMD="
for /f "usebackq delims=" %%I in (`where nasm 2^>nul`) do set "NASM_CMD=%%I"

if not defined NASM_CMD (
    if exist "%ProgramFiles%\NASM\nasm.exe" set "NASM_CMD=%ProgramFiles%\NASM\nasm.exe"
)

if not defined NASM_CMD (
    if exist "%ProgramFiles(x86)%\NASM\nasm.exe" set "NASM_CMD=%ProgramFiles(x86)%\NASM\nasm.exe"
)

if not defined NASM_CMD (
    echo [ERROR] NASM not found in PATH or %ProgramFiles%\NASM
    echo Please install NASM from https://www.nasm.us/ or add its bin folder to PATH
    pause
    exit /b 1
)

echo [OK] NASM command: %NASM_CMD%
"%NASM_CMD%" -v
echo.

REM Create build directories
if not exist build mkdir build
if not exist bin mkdir bin

echo [1/4] Assembling dx_ide_main.asm...
"%NASM_CMD%" -f win64 src\dx_ide_main.asm -o build\dx_ide_main.obj
if errorlevel 1 (
    echo [ERROR] Assembly failed for dx_ide_main.asm
    pause
    exit /b 1
)
echo [OK] dx_ide_main.obj created

echo [2/4] Assembling file_system.asm...
"%NASM_CMD%" -f win64 src\file_system.asm -o build\file_system.obj
if errorlevel 1 (
    echo [ERROR] Assembly failed for file_system.asm
    pause
    exit /b 1
)
echo [OK] file_system.obj created

echo [3/4] Assembling chat_pane.asm...
"%NASM_CMD%" -f win64 src\chat_pane.asm -o build\chat_pane.obj
if errorlevel 1 (
    echo [ERROR] Assembly failed for chat_pane.asm
    pause
    exit /b 1
)
echo [OK] chat_pane.obj created

echo [4/4] Linking executable...

if exist bin\nasm_ide_dx.exe (
    del /f /q bin\nasm_ide_dx.exe >nul 2>&1
)

REM Try MSVC linker first
where link >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Using MSVC linker
    link /ENTRY:WinMain /SUBSYSTEM:WINDOWS /OUT:bin\nasm_ide_dx.exe ^
        build\dx_ide_main.obj ^
        kernel32.lib user32.lib gdi32.lib comdlg32.lib
    
    if %errorlevel% neq 0 (
        echo [ERROR] Linking failed
        pause
        exit /b 1
    )
) else (
    REM Try MinGW linker with full path
    if exist "C:\ProgramData\mingw64\mingw64\bin\gcc.exe" (
        echo [INFO] Using MinGW GCC linker
        C:\ProgramData\mingw64\mingw64\bin\gcc.exe -m64 -mwindows -o bin\nasm_ide_dx.exe ^
            build\dx_ide_main.obj ^
            -lkernel32 -luser32 -lgdi32 -lcomdlg32

        if errorlevel 1 (
            echo [ERROR] Linking failed
            pause
            exit /b 1
        )
    ) else (
        echo [ERROR] No linker found
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
