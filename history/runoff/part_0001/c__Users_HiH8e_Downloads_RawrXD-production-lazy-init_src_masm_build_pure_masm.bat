@echo off
REM ==========================================================================
REM build_pure_masm.bat - Pure MASM64 RawrXD Build Script
REM ==========================================================================
REM Assembles all .asm files and links to single .exe
REM No C++ compiler needed. Only ml64 + link required.
REM
REM Prerequisites:
REM   - MASM64 installed (ml64.exe in PATH)
REM   - Windows SDK linked (link.exe in PATH)
REM   - MASM32 development environment
REM ==========================================================================

setlocal enabledelayedexpansion

REM Configuration
set BUILD_DIR=build_masm_pure
set LIB_DIR=%BUILD_DIR%\lib
set BIN_DIR=%BUILD_DIR%\bin
setlocal enabledelayedexpansion

echo.
echo ===========================================================================
echo Building RawrXD IDE - Pure MASM64 (No C++ Dependencies)
echo ===========================================================================
echo.

set MASM32=C:\masm32
set INCLUDE=%MASM32%\include
set LIB=%MASM32%\lib

if not exist "%MASM32%" (
    echo ERROR: MASM32 not found at %MASM32%
    echo Please install MASM32 or adjust MASM32 variable.
    exit /b 1
)

REM Check for ml64
where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: ml64.exe not found in PATH
    echo Add MASM32 bin directory to PATH or install Windows SDK
    exit /b 1
)

REM Check for link
where link.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: link.exe not found in PATH
    exit /b 1
)

echo [*] MASM32 Path: %MASM32%
echo [*] ML64:       %MASM32%\bin\ml64.exe
echo.

REM Assemble all .asm files
echo [*] Assembling MASM64 source files...
echo.

set OBJ_FILES=

REM Core MASM files
for %%F in (ml_masm.asm unified_masm_hotpatch.asm agentic_masm.asm ui_masm.asm main_masm.asm) do (
    if exist "%%F" (
        echo.Assembling: %%F
        ml64.exe /c /coff /nologo /W3 %%F
        if errorlevel 1 (
            echo ERROR: Assembly failed for %%F
            exit /b 1
        )
        set "OBJ_FILES=!OBJ_FILES! %%~nF.obj"
    )
)

echo.
echo [+] Assembly complete. Generated objects:
for %%F in (*.obj) do (
    echo   - %%F
)
echo.

REM Link to executable
echo [*] Linking to RawrXD-Pure-MASM64.exe...
echo.

set LINK_LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib comdlg32.lib wininet.lib riched20.lib

link.exe /NOLOGO /SUBSYSTEM:WINDOWS ^
         /ENTRY:main /LARGEADDRESSAWARE ^
         /LIBPATH:"%LIB%" ^
         %LINK_LIBS% ^
         %OBJ_FILES% ^
         /OUT:RawrXD-Pure-MASM64.exe

if errorlevel 1 (
    echo.
    echo ERROR: Linker failed!
    echo Check link.exe output above for details.
    exit /b 1
)

echo.
echo ===========================================================================
echo [SUCCESS] Build complete!
echo ===========================================================================
echo.

if exist RawrXD-Pure-MASM64.exe (
    for %%A in (RawrXD-Pure-MASM64.exe) do (
        set SIZE=%%~zA
        echo Executable: RawrXD-Pure-MASM64.exe
        echo Size:       !SIZE! bytes
    )
    echo.
    echo To run:
    echo   RawrXD-Pure-MASM64.exe
    echo.
    echo Architecture: x64 (Pure MASM64 - No C++ Runtime)
    echo Model:       ministral-3:latest (via Ollama)
    echo.
) else (
    echo ERROR: RawrXD-Pure-MASM64.exe not created!
    exit /b 1
)

REM Clean up object files (optional)
echo Cleaning intermediate object files...
for %%F in (*.obj) do del %%F

echo.
echo Done!
pause

