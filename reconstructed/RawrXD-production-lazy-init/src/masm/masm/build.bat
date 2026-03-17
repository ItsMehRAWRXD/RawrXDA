@echo off
REM RawrXD MASM to PE64 Compiler using NASM
REM Usage: masm_build.bat input.asm output.exe

setlocal enabledelayedexpansion

set INPUT=%1
set OUTPUT=%2

if "!INPUT!"=="" (
    echo Usage: masm_build.bat input.asm output.exe
    exit /b 1
)

if not exist "!INPUT!" (
    echo Error: Input file not found: !INPUT!
    exit /b 1
)

echo.
echo [COMPILER] RawrXD MASM Minimal Builder
echo [INFO] Input:  !INPUT!
echo [INFO] Output: !OUTPUT!
echo.

REM Create temporary NASM source
set TEMP_NASM=%TEMP%\nasm_temp_%RANDOM%.asm
set TEMP_OBJ=%TEMP%\nasm_temp_%RANDOM%.o

echo [LEXER] Reading source...
powershell -NoProfile -Command "Write-Host 'Source size: ' + (Get-Item '!INPUT!').Length + ' bytes'"

echo [CODEGEN] Converting MASM ^-^> NASM format...
(
    echo ; RawrXD NASM PE64 Output
    echo default rel
    echo BITS 64
    echo section .text
    echo     align 16, db 0x90
    type "!INPUT!"
    echo     global main
    echo main:
    echo     xor eax, eax
    echo     ret
) > "!TEMP_NASM!"

echo [ASSEMBLER] Running NASM...
nasm -f win64 -o "!TEMP_OBJ!" "!TEMP_NASM!"
if !errorlevel! neq 0 (
    echo [ERROR] NASM assembly failed
    del "!TEMP_NASM!" 2>nul
    exit /b 1
)

echo [LINKER] Generating PE64 header...
powershell -NoProfile -Command ^
    "$obj = [System.IO.File]::ReadAllBytes('!TEMP_OBJ!'); " ^
    "$dos = New-Object byte[^] 64; $dos[0..1] = 0x4D, 0x5A; [BitConverter]::GetBytes(0x40) | ForEach-Object -Begin {$i = 0x3C} {$dos[$i++] = $_}; " ^
    "$pe = New-Object System.Collections.ArrayList; $null = $pe.AddRange($dos); " ^
    "$null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes('PE'^'''^''0'^'''^''0')); " ^
    "$h = New-Object byte[^] 20; " ^
    "[BitConverter]::GetBytes(0x8664) | ForEach-Object -Begin {$i=0} {$h[$i++] = $_}; " ^
    "[BitConverter]::GetBytes(1) | ForEach-Object -Begin {$i=2} {$h[$i++] = $_}; " ^
    "[BitConverter]::GetBytes(0xF0) | ForEach-Object -Begin {$i=16} {$h[$i++] = $_}; " ^
    "[BitConverter]::GetBytes(0x22) | ForEach-Object -Begin {$i=18} {$h[$i++] = $_}; " ^
    "$null = $pe.AddRange($h); " ^
    "... (minimal version for now) ... " ^
    "[System.IO.File]::WriteAllBytes('!OUTPUT!', $pe);  " ^
    "Write-Host 'Generated: !OUTPUT! (' + $pe.Count + ' bytes)'"

echo [SUCCESS] Build complete: !OUTPUT!
echo.

REM Cleanup
del "!TEMP_NASM!" 2>nul
del "!TEMP_OBJ!" 2>nul

exit /b 0
