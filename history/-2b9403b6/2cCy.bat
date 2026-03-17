@echo off
REM ============================================================================
REM PE Generator ASM Compilation Script
REM ============================================================================

setlocal enabledelayedexpansion

REM Try to find and setup Visual Studio environment
if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    echo [OK] VS 2022 Enterprise environment loaded
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    echo [OK] VS 2022 Enterprise environment loaded
) else (
    echo [ERROR] Visual Studio not found
    exit /b 1
)

REM Create directories
if not exist "obj" mkdir obj
if not exist "lib" mkdir lib
if not exist "bin" mkdir bin

REM Compile PROD version
echo.
echo ============================================================================
echo [STEP 1] Compiling RawrXD_PE_Generator_PROD.asm...
echo ============================================================================
ml64 /c /nologo /W3 /Zd /Zi /Fo"obj\PeGen_PROD.obj" RawrXD_PE_Generator_PROD.asm
if errorlevel 1 (
    echo [FAIL] PROD compilation failed
    exit /b 1
) else (
    echo [OK] PROD compilation successful - obj\PeGen_PROD.obj created
)

REM Compile FULL version
echo.
echo ============================================================================
echo [STEP 2] Compiling RawrXD_PE_Generator_FULL.asm...
echo ============================================================================
ml64 /c /nologo /W3 /Zd /Zi /Fo"obj\PeGen_FULL.obj" RawrXD_PE_Generator_FULL.asm
if errorlevel 1 (
    echo [FAIL] FULL compilation failed
    exit /b 1
) else (
    echo [OK] FULL compilation successful - obj\PeGen_FULL.obj created
)

REM Create static library from PROD (primary)
echo.
echo ============================================================================
echo [STEP 3] Creating RawrXD_PeGen.lib from PROD object...
echo ============================================================================
lib /nologo /out:"lib\RawrXD_PeGen.lib" "obj\PeGen_PROD.obj"
if errorlevel 1 (
    echo [FAIL] Library creation failed
    exit /b 1
) else (
    echo [OK] Library created successfully - lib\RawrXD_PeGen.lib
)

REM Create static library from FULL
echo.
echo ============================================================================
echo [STEP 4] Creating RawrXD_PeGen_FULL.lib from FULL object...
echo ============================================================================
lib /nologo /out:"lib\RawrXD_PeGen_FULL.lib" "obj\PeGen_FULL.obj"
if errorlevel 1 (
    echo [WARNING] Full library creation failed - continuing with PROD only
) else (
    echo [OK] Full library created - lib\RawrXD_PeGen_FULL.lib
)

REM Show summary
echo.
echo ============================================================================
echo [SUMMARY] Build Complete
echo ============================================================================
dir /B "obj\*.obj"
echo.
dir /B "lib\*.lib"
echo.
echo [OK] All builds completed successfully!
echo.
exit /b 0
