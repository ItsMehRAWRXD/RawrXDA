@echo off
REM =====================================================================
REM Build Pure ASM DirectX IDE
REM Compiles all components and links into standalone executable
REM =====================================================================

echo.
echo ========================================
echo   Building Pure ASM DirectX IDE
echo ========================================
echo.

REM Check for NASM
set "NASM_COMMAND=nasm"
where %NASM_COMMAND% >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "%ProgramData%\chocolatey\bin\nasm.exe" (
        echo [INFO] Found NASM in Chocolatey directory
        set "NASM_COMMAND=%ProgramData%\chocolatey\bin\nasm.exe"
    ) else (
        echo [ERROR] NASM not found in PATH
        echo Please install NASM from https://www.nasm.us/ and reopen this terminal (or run refreshenv)
        pause
        exit /b 1
    )
)

REM Check for linker (link.exe from Visual Studio or MinGW)
where link.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set LINKER=msvc
    echo [INFO] Using MSVC linker
) else (
    where ld.exe >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        set LINKER=mingw
        echo [INFO] Using MinGW linker
    ) else (
        echo [ERROR] No linker found (need link.exe or ld.exe)
        echo Please install Visual Studio Build Tools or MinGW
        pause
        exit /b 1
    )
)

REM Create directories
if not exist "build" mkdir build
if not exist "bin" mkdir bin

echo.
echo [1/5] Assembling main IDE module (dx_ide_main.asm)...
echo [1/5] Assembling main IDE module (dx_ide_main.asm)...
"%NASM_COMMAND%" -f win64 src\dx_ide_main.asm -o build\dx_ide_main.obj
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to assemble dx_ide_main.asm
    pause
    exit /b 1
)
echo       Done.

echo.
echo [2/5] Assembling file system module (file_system.asm)...
echo [2/5] Assembling file system module (file_system.asm)...
"%NASM_COMMAND%" -f win64 src\file_system.asm -o build\file_system.obj
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to assemble file_system.asm
    pause
    exit /b 1
)
echo       Done.

echo.
echo [3/5] Assembling chat pane module (chat_pane.asm)...
echo [3/5] Assembling chat pane module (chat_pane.asm)...
"%NASM_COMMAND%" -f win64 src\chat_pane.asm -o build\chat_pane.obj
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to assemble chat_pane.asm
    pause
    exit /b 1
)
echo       Done.

echo.
echo [4/5] Linking executable...

if "%LINKER%"=="msvc" (
    REM MSVC linker
    link.exe /ENTRY:WinMain /SUBSYSTEM:WINDOWS ^
        /OUT:bin\nasm_ide_dx.exe ^
        build\dx_ide_main.obj ^
        build\file_system.obj ^
        build\chat_pane.obj ^
        kernel32.lib user32.lib gdi32.lib comdlg32.lib ^
        d3d11.lib dxgi.lib d2d1.lib dwrite.lib
) else (
    REM MinGW linker
    ld.exe -o bin\nasm_ide_dx.exe ^
        build\dx_ide_main.obj ^
        build\file_system.obj ^
        build\chat_pane.obj ^
        -lkernel32 -luser32 -lgdi32 -lcomdlg32 ^
        -ld3d11 -ldxgi -ld2d1 -ldwrite
)

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    echo.
    echo Common issues:
    echo   - Missing Windows SDK (install Visual Studio Build Tools)
    echo   - Missing DirectX SDK
    echo   - Incorrect library paths
    echo.
    pause
    exit /b 1
)
echo       Done.

echo.
echo [5/5] Creating launcher script...
echo @echo off > bin\run_ide.bat
echo echo Starting Pure ASM DirectX IDE... >> bin\run_ide.bat
echo start nasm_ide_dx.exe >> bin\run_ide.bat
echo       Done.

echo.
echo ========================================
echo   Build Complete!
echo ========================================
echo.
echo Executable: bin\nasm_ide_dx.exe
echo Launcher:   bin\run_ide.bat
echo.
echo Features:
echo   [x] Pure ASM implementation (no C/C++/JS)
echo   [x] DirectX11 hardware-accelerated rendering
echo   [x] Full text editor with syntax highlighting
echo   [x] Chat pane for AI agent communication
echo   [x] File opener/saver with native dialogs
echo   [x] Shared file buffer for front/backend
echo.
echo To run: cd bin ^&^& nasm_ide_dx.exe
echo.
pause
