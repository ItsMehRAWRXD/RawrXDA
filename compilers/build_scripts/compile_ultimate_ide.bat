@echo off
:: Ultimate Multi-Language Assembly IDE Compilation Script
:: Using Pure Assembly Compilers

echo Compiling Ultimate Multi-Language Assembly IDE...
echo ================================================

:: Simple NASM check - use direct path
if exist "D:\RawrXD\compilers\nasm\nasm-2.16.01\nasm.exe" (
    set NASM_CMD=D:\RawrXD\compilers\nasm\nasm-2.16.01\nasm.exe
    goto :found_nasm
)

:: Check standard locations
if exist "C:\Program Files\NASM\nasm.exe" (
    set NASM_CMD="C:\Program Files\NASM\nasm.exe"
    goto :found_nasm
)

if exist "C:\Program Files (x86)\NASM\nasm.exe" (
    set NASM_CMD="C:\Program Files (x86)\NASM\nasm.exe"
    goto :found_nasm
)

echo ERROR: NASM not found. Please install NASM from https://www.nasm.us/
echo Looking for nasm.exe in:
echo   C:\Program Files\NASM\nasm.exe
echo   C:\Program Files (x86)\NASM\nasm.exe
echo   D:\RawrXD\compilers\nasm\nasm-2.16.01\nasm.exe
exit /b 1

:found_nasm

:: Create output directory if it doesn't exist
if not exist "bin" mkdir bin

echo.
echo Assembling ultimate multi-language IDE...
echo -----------------------------------------

:: Assemble the main IDE file
%NASM_CMD% -f win64 ultimate_multilang_ide.asm -o bin\ultimate_ide.obj
if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to assemble ultimate_multilang_ide.asm
    exit /b %ERRORLEVEL%
)

echo.
echo Creating Windows executable...
echo ------------------------------

:: Create DEF file for exports
echo LIBRARY ultimate_ide.dll > ultimate_ide.def
echo EXPORTS >> ultimate_ide.def
echo   WinMain >> ultimate_ide.def

:: Link with comprehensive Windows libraries
echo Linking with Windows API libraries...
link /subsystem:windows /entry:WinMain ^
     /nodefaultlib:libucrt.lib ^
     bin\ultimate_ide.obj ^
     kernel32.lib user32.lib gdi32.lib comdlg32.lib ^
     comctl32.lib shell32.lib advapi32.lib ^
     /out:bin\ultimate_ide.exe ^
     /def:ultimate_ide.def

if %ERRORLEVEL% neq 0 (
    echo.
    echo Attempting alternative linking approach...

    :: Try with debug symbols and different libraries
    link /debug /subsystem:windows /entry:WinMain ^
         bin\ultimate_ide.obj ^
         kernel32.lib user32.lib gdi32.lib comdlg32.lib ^
         comctl32.lib shell32.lib advapi32.lib ^
         msvcrt.lib legacy_stdio_definitions.lib ^
         /out:bin\ultimate_ide.exe

    if %ERRORLEVEL% neq 0 (
        echo.
        echo ERROR: All linking attempts failed.
        echo.
        echo Generating symbol information for debugging...
        dumpbin /symbols bin\ultimate_ide.obj > symbols_ide.txt
        echo Check symbols_ide.txt for undefined references.
        exit /b %ERRORLEVEL%
    )
)

echo.
echo Compilation successful!
echo =======================
echo.
echo The ultimate multi-language IDE has been compiled to:
echo bin\ultimate_ide.exe
echo.
echo Features included:
echo - 17 programming languages support
echo - Binary/Hex editor modes
echo - Extension system
echo - Transparency effects
echo - Complete GUI with tabbed interface
echo - Project management
echo - Advanced text editing
echo - Build system integration
echo.
echo To run the IDE:
echo bin\ultimate_ide.exe
echo.
echo Note: This is a 3,200+ line pure assembly implementation
echo       with no external dependencies beyond Windows API.

pause
