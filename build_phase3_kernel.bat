@echo off
REM Build Phase3_Agent_Kernel.dll with all real implementations
REM Usage: build_phase3_kernel.bat

setlocal enabledelayedexpansion

REM Check for Visual Studio environment
if not defined VCINSTALLDIR (
    echo Detecting Visual Studio installation...
    for /f "tokens=*" %%a in ('vswhere -latest -products * -requires Microsoft.VisualStudio.Product.Community -property installationPath') do (
        set "VSPATH=%%a"
    )
    if defined VSPATH (
        call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo Error: Visual Studio not found. Run from "Developer Command Prompt for VS"
        exit /b 1
    )
)

echo.
echo ============================================================
echo Building Phase3_Agent_Kernel.dll (REAL IMPLEMENTATIONS)
echo ============================================================
echo.

REM Create output directory if it doesn't exist
if not exist "D:\RawrXD\bin" mkdir "D:\RawrXD\bin"

REM Compile with optimizations and full error checking
ml64 /c /Fo:D:\RawrXD\obj\matrix_mul_avx512.obj D:\RawrXD\src\matrix_mul_avx512.asm

cl /O2 /Oy /LD /W4 /WX /Fe:D:\RawrXD\bin\Phase3_Agent_Kernel.dll ^
   /Fo:D:\RawrXD\obj\ ^
   D:\RawrXD\src\Phase3_Agent_Kernel.cpp ^
   D:\RawrXD\obj\matrix_mul_avx512.obj ^
   kernel32.lib user32.lib comdlg32.lib shell32.lib ole32.lib ^
   /LINK /SUBSYSTEM:WINDOWS /DLL

REM Check build result
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo ✅ BUILD SUCCESS
    echo ============================================================
    echo DLL created: D:\RawrXD\bin\Phase3_Agent_Kernel.dll
    echo.
    
    REM Show DLL info
    dir /b D:\RawrXD\bin\Phase3_Agent_Kernel.dll
    echo.
    
    REM List exported functions
    echo Exported functions:
    dumpbin /exports D:\RawrXD\bin\Phase3_Agent_Kernel.dll | findstr /V "^$"
    
    exit /b 0
) else (
    echo.
    echo ============================================================
    echo ❌ BUILD FAILED (Error Code: %ERRORLEVEL%)
    echo ============================================================
    exit /b 1
)
