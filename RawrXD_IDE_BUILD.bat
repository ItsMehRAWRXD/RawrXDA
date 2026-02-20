@echo off
REM RawrXD Unified MASM64 IDE Build Script
REM Assembles and links the unified offensive security toolkit

echo ========================================
echo RawrXD Unified MASM64 IDE Build
echo ========================================
echo.

REM Check for ml64.exe
where ml64.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: ml64.exe not found. Please run from Visual Studio x64 Native Tools Command Prompt.
    pause
    exit /b 1
)

REM Assemble
echo [1/2] Assembling RawrXD_IDE_unified.asm...
ml64.exe /c /Zi /Fo"RawrXD_IDE_unified.obj" "RawrXD_IDE_unified.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Assembly failed.
    pause
    exit /b 1
)

REM Link
echo [2/2] Linking RawrXD_IDE_unified.obj...
link.exe /SUBSYSTEM:CONSOLE /ENTRY:_start_entry /LARGEADDRESSAWARE:NO /OUT:"RawrXD_IDE_unified.exe" "RawrXD_IDE_unified.obj" kernel32.lib user32.lib advapi32.lib shell32.lib
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Linking failed.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Complete!
echo Output: RawrXD_IDE_unified.exe
echo ========================================
echo.
echo Usage:
echo   GUI Mode: RawrXD_IDE_unified.exe
echo   CLI Mode: RawrXD_IDE_unified.exe -compile
echo             RawrXD_IDE_unified.exe -encrypt
echo             RawrXD_IDE_unified.exe -inject
echo             RawrXD_IDE_unified.exe -uac
echo             RawrXD_IDE_unified.exe -persist
echo             RawrXD_IDE_unified.exe -sideload
echo             RawrXD_IDE_unified.exe -avscan
echo             RawrXD_IDE_unified.exe -entropy
echo             RawrXD_IDE_unified.exe -stubgen
echo             RawrXD_IDE_unified.exe -trace
echo.
pause
