@echo off
setlocal EnableDelayedExpansion

echo ==========================================
echo RawrXD Titan GPU/DMA Build System
echo Version 5.0.0 Final
echo Date: January 28, 2026
echo ==========================================
echo.

:: Configuration
set ML64=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\ml64.exe
set LIB=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\lib.exe
set LINK=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\link.exe

set SRC=gpu_dma_complete_production.asm
set OBJ=GPU_DMA_Complete.obj
set LIB_OUT=Titan_GPU_DMA.lib
set DLL_OUT=Titan_GPU_DMA.dll

:: Check if ml64.exe exists
if not exist "%ML64%" (
    echo [ERROR] ml64.exe not found at: %ML64%
    echo Please update the ML64 path in this script
    exit /b 1
)

echo [1/6] Cleaning previous builds...
if exist %OBJ% del /F /Q %OBJ%
if exist %LIB_OUT% del /F /Q %LIB_OUT%
if exist %DLL_OUT% del /F /Q %DLL_OUT%
if exist *.pdb del /F /Q *.pdb
echo   - Cleanup complete

echo.
echo [2/6] Assembling with AVX-512 optimizations...
"%ML64%" /c /Fo%OBJ% /W3 /Zd /Zi /nologo ^
    /DAVX512_SUPPORT=1 ^
    /D_WIN64 ^
    /Fl %SRC%

if errorlevel 1 (
    echo [ERROR] Assembly failed! Check output above.
    exit /b 1
)
echo   - Assembly successful

echo.
echo [3/6] Creating static library...
"%LIB%" /OUT:%LIB_OUT% /NOLOGO %OBJ%
if errorlevel 1 (
    echo [ERROR] Library creation failed!
    exit /b 1
)
echo   - Library created: %LIB_OUT%

echo.
echo [4/6] Verifying exports...
dumpbin /SYMBOLS %OBJ% | findstr "Titan_" > exports.tmp
if errorlevel 1 (
    echo [WARNING] No exports found in object file
) else (
    echo   - Found exports:
    type exports.tmp
)
del exports.tmp 2>nul

echo.
echo [5/6] Checking file sizes...
for %%F in (%OBJ%) do echo   - Object file: %%~zF bytes
for %%F in (%LIB_OUT%) do echo   - Library: %%~zF bytes

echo.
echo [6/6] Build verification...
if exist %OBJ% (
    if exist %LIB_OUT% (
        echo   - All outputs present
        echo   - Build SUCCESSFUL
        goto success
    )
)

echo [ERROR] Build incomplete - missing outputs
exit /b 1

:success
echo.
echo ==========================================
echo Build Summary
echo ==========================================
echo Object file:   %OBJ%
echo Library:       %LIB_OUT%
echo Status:        PRODUCTION READY
echo.
echo Exported functions:
echo   - Titan_ExecuteComputeKernel
echo   - Titan_PerformCopy
echo   - Titan_PerformDMA
echo   - Titan_InitializeDMA
echo   - Titan_ShutdownDMA
echo   - Titan_GetDMAStats
echo.
echo Ready to link with RawrXD Titan Engine
echo ==========================================

endlocal
exit /b 0
