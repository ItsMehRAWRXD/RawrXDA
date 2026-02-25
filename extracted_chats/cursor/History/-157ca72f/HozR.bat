@echo off
setlocal enabledelayedexpansion
echo ========================================
echo NASM ASSEMBLY COMPILATION - FINAL
echo ========================================

REM Check for NASM in multiple locations
set "NASM_CMD="
echo Checking for NASM...

if exist "C:\Program Files\NASM\nasm.exe" (
    echo Found: C:\Program Files\NASM\nasm.exe
    set "NASM_CMD=C:\Program Files\NASM\nasm.exe"
    goto found_nasm
)

if exist "C:\Program Files (x86)\NASM\nasm.exe" (
    echo Found: C:\Program Files (x86)\NASM\nasm.exe
    set "NASM_CMD=C:\Program Files (x86)\NASM\nasm.exe"
    goto found_nasm
)

REM Check if nasm is in PATH
nasm -v >nul 2>&1
if %errorlevel% equ 0 (
    echo Found: nasm in PATH
    set "NASM_CMD=nasm"
    goto found_nasm
)

echo ERROR: NASM not found in any location!
echo Please install NASM from https://www.nasm.us/
goto end

:found_nasm
echo Using NASM: !NASM_CMD!

REM Create bin directory
if not exist bin mkdir bin

echo.
echo Compiling pure_assembly_directx_studio.asm...
"!NASM_CMD!" -f win64 pure_assembly_directx_studio.asm -o bin\directx_studio.obj

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS: Assembly completed!
    for %%A in (bin\directx_studio.obj) do echo Object file size: %%~zA bytes

    REM Try GCC first
    gcc --version >nul 2>&1
    if %errorlevel% equ 0 (
        echo.
        echo Trying GCC linker...
        gcc bin\directx_studio.obj -o bin\directx_studio.exe -lkernel32 -luser32 -lgdi32 -ld3d11 -ld3dcompiler -ldxgi
        if %errorlevel% equ 0 (
            echo.
            echo SUCCESS: GCC linking completed!
            for %%A in (bin\directx_studio.exe) do echo Executable size: %%~zA bytes
            goto success
        )
    )

    REM Try Microsoft Linker
    link /? >nul 2>&1
    if %errorlevel% equ 0 (
        echo.
        echo Trying Microsoft Linker...
        link /SUBSYSTEM:WINDOWS /ENTRY:WinMain bin\directx_studio.obj /OUT:bin\directx_studio.exe kernel32.lib user32.lib gdi32.lib d3d11.lib d3dcompiler.lib dxgi.lib
        if %errorlevel% equ 0 (
            echo.
            echo SUCCESS: Microsoft linking completed!
            for %%A in (bin\directx_studio.exe) do echo Executable size: %%~zA bytes
            goto success
        )
    )

    echo.
    echo WARNING: No linker succeeded - object file only
    goto obj_only

    :success
    echo.
    echo ========================================
    echo COMPILATION SUCCESSFUL!
    echo Executable: bin\directx_studio.exe
    echo ========================================
    goto end

    :obj_only
    echo.
    echo ========================================
    echo ASSEMBLY SUCCESSFUL!
    echo Object file: bin\directx_studio.obj
    echo Install a linker to create executable
    echo ========================================

) else (
    echo.
    echo ERROR: Assembly failed!
)

:end
echo.
pause
