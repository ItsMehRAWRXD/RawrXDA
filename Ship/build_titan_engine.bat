@echo off
REM ==============================================================================
REM RawrXD Titan Engine Build Script
REM Assembles MASM64 and links DLL with zero external dependencies
REM ==============================================================================

setlocal enabledelayedexpansion

set ML64_PATH=C:\masm32\bin\ml64.exe
set LINK_PATH=C:\masm32\bin\link.exe
set LIB_PATH=C:\masm32\lib64

if not exist "%ML64_PATH%" (
    echo Error: ml64.exe not found at %ML64_PATH%
    echo Please install MASM32 SDK
    exit /b 1
)

echo ========================================
echo RawrXD Titan Engine Build
echo ========================================
echo.

REM Assembly phase
echo [1/3] Assembling RawrXD_Titan_Engine.asm...
"%ML64_PATH%" /c /Zi /D"PRODUCTION=1" /I"%LIB_PATH%" RawrXD_Titan_Engine.asm
if errorlevel 1 (
    echo Assembly failed!
    exit /b 1
)
echo Assembly complete!
echo.

REM Link phase
echo [2/3] Linking RawrXD_Titan_Engine.dll...
"%LINK_PATH%" ^
    /DLL ^
    /SUBSYSTEM:WINDOWS ^
    /ENTRY:DllMain ^
    /MACHINE:X64 ^
    /NODEFAULTLIB ^
    /OUT:RawrXD_Titan_Engine.dll ^
    /IMPLIB:RawrXD_Titan_Engine.lib ^
    /LIBPATH:"%LIB_PATH%" ^
    /DEBUG ^
    /OPT:REF ^
    /OPT:ICF ^
    kernel32.lib ^
    ntdll.lib ^
    RawrXD_Titan_Engine.obj
    
if errorlevel 1 (
    echo Link failed!
    exit /b 1
)
echo Link complete!
echo.

REM Verification
echo [3/3] Verifying build output...
if exist RawrXD_Titan_Engine.dll (
    for %%F in (RawrXD_Titan_Engine.dll) do (
        echo DLL created: %%~fF
        echo Size: %%~zF bytes
    )
    echo.
    echo Build successful!
    echo.
    echo Generated files:
    echo   - RawrXD_Titan_Engine.dll (runtime DLL)
    echo   - RawrXD_Titan_Engine.lib (import library)
    echo   - RawrXD_Titan_Engine.obj (object file)
    echo   - RawrXD_Titan_Engine.pdb (debug symbols)
    exit /b 0
) else (
    echo Build failed - DLL not created
    exit /b 1
)
