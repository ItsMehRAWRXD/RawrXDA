@echo off
REM ============================================================================
REM RawrXD Production Build Script
REM Zero-Dependency x64 MASM Assembly Build System
REM ============================================================================
setlocal enabledelayedexpansion

echo.
echo ============================================
echo    RawrXD Production Build System
echo ============================================
echo.

REM Find Visual Studio Developer Environment
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSINSTALL=%%i"
    if exist "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" (
        call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        echo [OK] Visual Studio environment loaded
    )
) else (
    REM Try common paths
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        echo [OK] VS2022 Community environment loaded
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        echo [OK] VS2022 Professional environment loaded
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        echo [OK] VS2022 Enterprise environment loaded
    ) else (
        echo [ERROR] Visual Studio not found. Please install VS with C++ workload.
        exit /b 1
    )
)

REM Verify ml64.exe is available
where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64.exe not found in PATH
    echo         Install Visual Studio with "Desktop development with C++" workload
    exit /b 1
)
echo [OK] ml64.exe found

REM Set working directory
cd /d "%~dp0"
echo [OK] Working directory: %CD%

REM Create output directory
if not exist "build" mkdir build
echo [OK] Build directory ready

REM ============================================================================
REM Build Configuration
REM ============================================================================
set BUILD_TARGET=%1
if "%BUILD_TARGET%"=="" set BUILD_TARGET=all

echo.
echo Build Target: %BUILD_TARGET%
echo.

REM ============================================================================
REM Build: Standalone IDE
REM ============================================================================
if "%BUILD_TARGET%"=="ide" goto :build_ide
if "%BUILD_TARGET%"=="all" goto :build_all
goto :build_%BUILD_TARGET%

:build_all
:build_ide
echo [BUILD] RawrXD_Sovereign_Monolith.asm
ml64.exe /c /Fo build\RawrXD_Sovereign_Monolith.obj RawrXD_Sovereign_Monolith.asm
if errorlevel 1 (
    echo [FAIL] Assembly failed: RawrXD_Sovereign_Monolith.asm
    exit /b 1
)
echo [OK] Assembled: RawrXD_Sovereign_Monolith.obj

link.exe /ENTRY:Start /SUBSYSTEM:WINDOWS /NODEFAULTLIB /OUT:build\RawrXD_IDE.exe build\RawrXD_Sovereign_Monolith.obj
if errorlevel 1 (
    echo [FAIL] Link failed: RawrXD_IDE.exe
    exit /b 1
)
echo [OK] Linked: build\RawrXD_IDE.exe
if "%BUILD_TARGET%"=="ide" goto :done
goto :build_pe

REM ============================================================================
REM Build: PE Writer Library
REM ============================================================================
:build_pe
echo [BUILD] RawrXD_Monolithic_PE_Emitter.asm
ml64.exe /c /Fo build\RawrXD_PE_Emitter.obj pe_writer_production\RawrXD_Monolithic_PE_Emitter.asm
if errorlevel 1 (
    echo [FAIL] Assembly failed: RawrXD_Monolithic_PE_Emitter.asm
    exit /b 1
)
echo [OK] Assembled: RawrXD_PE_Emitter.obj
if "%BUILD_TARGET%"=="pe" goto :done
goto :build_codex

REM ============================================================================
REM Build: Binary Analyzer (uses EXTERN - needs kernel32.lib)
REM ============================================================================
:build_codex
echo [BUILD] RawrCodex.asm
ml64.exe /c /Fo build\RawrCodex.obj asm\RawrCodex.asm
if errorlevel 1 (
    echo [FAIL] Assembly failed: RawrCodex.asm
    exit /b 1
)
echo [OK] Assembled: RawrCodex.obj

REM Build RawrCodex standalone EXE (with kernel32.lib)
echo [LINK] RawrCodex standalone
link.exe /SUBSYSTEM:CONSOLE /OUT:build\RawrCodex.exe build\RawrCodex.obj kernel32.lib
if errorlevel 1 (
    echo [WARN] RawrCodex link failed - may need entry point
) else (
    echo [OK] Linked: build\RawrCodex.exe
)

if "%BUILD_TARGET%"=="codex" goto :done
goto :build_full

REM ============================================================================
REM Build: Full System
REM ============================================================================
:build_full
echo [LINK] Full system with all modules
link.exe /ENTRY:Start /SUBSYSTEM:WINDOWS /NODEFAULTLIB /OUT:build\RawrXD_Full.exe ^
    build\RawrXD_Sovereign_Monolith.obj ^
    build\RawrXD_PE_Emitter.obj ^
    build\RawrCodex.obj
if errorlevel 1 (
    echo [WARN] Full link failed - modules may have duplicate symbols
    echo        Building standalone IDE instead...
) else (
    echo [OK] Linked: build\RawrXD_Full.exe
)
goto :done

:done
echo.
echo ============================================
echo    Build Complete
echo ============================================
echo.
echo Outputs:
if exist "build\RawrXD_IDE.exe" echo   - build\RawrXD_IDE.exe (Standalone IDE)
if exist "build\RawrXD_Full.exe" echo   - build\RawrXD_Full.exe (Full System)
if exist "build\RawrXD_Sovereign_Monolith.obj" echo   - build\RawrXD_Sovereign_Monolith.obj
if exist "build\RawrXD_PE_Emitter.obj" echo   - build\RawrXD_PE_Emitter.obj
if exist "build\RawrCodex.obj" echo   - build\RawrCodex.obj
echo.

endlocal
exit /b 0
