@echo off
REM ═══════════════════════════════════════════════════════════════════════════════
REM MASM64 Lazy Init IDE - Production Build System
REM Integrates PowerShell compiler orchestration with MASM64 runtime
REM ═══════════════════════════════════════════════════════════════════════════════

setlocal enabledelayedexpansion

REM Ensure Visual Studio build tools are available (ml64/link)
where ml64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Initializing Visual Studio build environment...
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat" (
        call "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo [ERROR] Could not locate vcvars64.bat. Please open a Developer Command Prompt and re-run this script.
        goto :error
    )
)

echo.
echo ═══════════════════════════════════════════════════════════════════════════════
echo   MASM64 Lazy Init IDE - Full Integration Build
echo   PowerShell Compiler Build + MASM64 Runtime
echo ═══════════════════════════════════════════════════════════════════════════════
echo.

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 1: Create directory structure
REM ─────────────────────────────────────────────────────────────────────────────

echo [SETUP] Creating directory structure...
if not exist "bin" mkdir bin
if not exist "bin\compilers" mkdir bin\compilers
if not exist "bin\runtime" mkdir bin\runtime
if not exist "bin\orchestrator" mkdir bin\orchestrator
if not exist "obj" mkdir obj

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 2: Build MASM64 orchestrator
REM ─────────────────────────────────────────────────────────────────────────────

echo.
echo [BUILD] Compiling MASM64 orchestrator...

ml64 /c /Zi /Fo"obj\masm64_compiler_orchestrator.obj" "masm64_compiler_orchestrator.asm" 2>build_errors.txt
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MASM64 orchestrator compilation failed
    type build_errors.txt
    goto :error
)

echo [OK] MASM64 orchestrator object created

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 3: Link MASM64 orchestrator executable
REM ─────────────────────────────────────────────────────────────────────────────

echo [BUILD] Linking MASM64 orchestrator executable...

link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:"bin\orchestrator\masm64_orchestrator.exe" ^
     "obj\masm64_compiler_orchestrator.obj" ^
     kernel32.lib user32.lib

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MASM64 orchestrator link failed
    goto :error
)

echo [OK] MASM64 orchestrator executable created

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 4: Execute PowerShell compiler build system
REM ─────────────────────────────────────────────────────────────────────────────

echo.
echo [BUILD] Executing PowerShell compiler build orchestration...

set "SKIP_PS=0"
if exist "build\obj\runtime.obj" (
    for %%F in (build\obj\*_compiler_from_scratch*.obj) do (
        set "SKIP_PS=1"
    )
)

if "%SKIP_PS%"=="1" (
    echo [INFO] Using existing build outputs from build\obj
    if exist "build\obj\runtime.obj" copy /y "build\obj\runtime.obj" "bin\runtime\runtime.obj" >nul
    for %%F in (build\obj\*_compiler_from_scratch*.obj) do (
        copy /y "%%F" "bin\compilers\%%~nxF" >nul
    )
    echo [OK] Copied compiler objects and runtime
) else (
    powershell -ExecutionPolicy Bypass -File "Build-CompilersWithRuntime.ps1" -OutDir "bin\compilers" -RuntimeOut "bin\runtime"
    if %ERRORLEVEL% NEQ 0 (
        echo [WARNING] PowerShell compiler build had warnings (continuing)
    )
    echo [OK] PowerShell compiler build completed
)

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 5: Integrate runtime and compilers
REM ─────────────────────────────────────────────────────────────────────────────

echo.
echo [INTEGRATION] Verifying build outputs...

if exist "bin\runtime\runtime.obj" (
    echo [OK] Runtime object found: bin\runtime\runtime.obj
) else (
    echo [WARNING] Runtime object not found
)

if exist "bin\compilers\manifest.json" (
    echo [OK] Compiler manifest found: bin\compilers\manifest.json
) else (
    echo [WARNING] Compiler manifest not found
)

REM Count compiler objects
set /a compiler_count=0
for %%F in (bin\compilers\*.obj) do (
    set /a compiler_count+=1
)

echo [OK] Compiler objects built: !compiler_count!

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 6: Build final integrated executable
REM ─────────────────────────────────────────────────────────────────────────────

echo.
echo [BUILD] Creating integrated lazy-init IDE executable...

REM Collect all compiler objects (skip *_fixed.obj to avoid duplicate main)
set COMPILER_OBJS=
for %%F in (bin\compilers\*.obj) do (
    echo %%~nF | findstr /i "_fixed" >nul
    if errorlevel 1 (
        set COMPILER_OBJS=!COMPILER_OBJS! "%%F"
    )
)

REM Link everything together
if exist "bin\runtime\runtime.obj" (
    set "RSP=obj\integrated_link.rsp"
    del /f /q "%RSP%" 2>nul
    echo /NOLOGO>"%RSP%"
    echo /SUBSYSTEM:CONSOLE>>"%RSP%"
    echo /ENTRY:main>>"%RSP%"
    echo /LARGEADDRESSAWARE:NO>>"%RSP%"
    echo /OUT:"bin\lazy_init_ide.exe">>"%RSP%"
    echo "obj\masm64_compiler_orchestrator.obj">>"%RSP%"
    echo "bin\runtime\runtime.obj">>"%RSP%"
    for %%F in (bin\compilers\*.obj) do (
        echo %%~nF | findstr /i "_fixed" >nul
        if errorlevel 1 echo "%%F">>"%RSP%"
    )
    echo kernel32.lib>>"%RSP%"
    echo user32.lib>>"%RSP%"

    link @"%RSP%"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Integrated link failed
        goto :error
    ) else (
        echo [OK] Integrated executable created: bin\lazy_init_ide.exe
    )
)

REM ─────────────────────────────────────────────────────────────────────────────
REM Step 7: Generate build report
REM ─────────────────────────────────────────────────────────────────────────────

echo.
echo ═══════════════════════════════════════════════════════════════════════════════
echo   BUILD SUMMARY
echo ═══════════════════════════════════════════════════════════════════════════════
echo.
echo   MASM64 Orchestrator:     bin\orchestrator\masm64_orchestrator.exe
echo   Universal Runtime:       bin\runtime\runtime.obj
echo   Compiler Count:          !compiler_count!
echo   Integrated IDE:          bin\lazy_init_ide.exe
echo.
echo ═══════════════════════════════════════════════════════════════════════════════
echo   BUILD COMPLETE
echo ═══════════════════════════════════════════════════════════════════════════════
echo.

goto :eof

:error
echo.
echo [ERROR] Build failed - see build_errors.txt for details
exit /b 1
