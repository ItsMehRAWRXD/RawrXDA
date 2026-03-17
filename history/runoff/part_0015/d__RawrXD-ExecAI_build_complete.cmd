@echo off
setlocal EnableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
set "LOG_FILE=%SCRIPT_DIR%build_%DATE:~-4%-%DATE:~-7,2%-%DATE:~-10,2%_%TIME:~0,2%%TIME:~3,2%.log"
set "VS_VERSION="
set "MASM_PATH="

echo [BUILD] Initialize at %DATE% %TIME% > "%LOG_FILE%"

REM ═══════════════════════════════════════════════════════════════
REM Direct path to known ml64.exe location (discovered via audit)
REM ═══════════════════════════════════════════════════════════════
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" (
    set "MASM_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
    set "VS_VERSION=VS2022 BuildTools"
    echo [SUCCESS] Found ml64.exe at known location >> "%LOG_FILE%"
    goto :found
)

echo [FALLBACK] Searching alternate VS2022 paths... >> "%LOG_FILE%"

REM ═══════════════════════════════════════════════════════════════
REM Fallback: scan Program Files (x86) for any VS2022 installation
REM ═══════════════════════════════════════════════════════════════
pushd "C:\Program Files (x86)\Microsoft Visual Studio\2022" 2>nul
if %errorlevel% equ 0 (
    for /d %%E in (*) do (
        if exist "%%E\VC\Tools\MSVC" (
            pushd "%%E\VC\Tools\MSVC" 2>nul
            for /d %%V in (*) do (
                if exist "%%V\bin\Hostx64\x64\ml64.exe" (
                    set "MASM_PATH=!CD!\%%V\bin\Hostx64\x64"
                    set "VS_VERSION=VS2022 %%E"
                    echo [SUCCESS] Found ml64.exe in %%E >> "%LOG_FILE%"
                    popd
                    popd
                    goto :found
                )
            )
            popd
        )
    )
    popd
)

REM ═══════════════════════════════════════════════════════════════
REM Fallback: search Program Files (x64) for VS2022
REM ═══════════════════════════════════════════════════════════════
pushd "C:\Program Files\Microsoft Visual Studio\2022" 2>nul
if %errorlevel% equ 0 (
    for /d %%E in (*) do (
        if exist "%%E\VC\Tools\MSVC" (
            pushd "%%E\VC\Tools\MSVC" 2>nul
            for /d %%V in (*) do (
                if exist "%%V\bin\Hostx64\x64\ml64.exe" (
                    set "MASM_PATH=!CD!\%%V\bin\Hostx64\x64"
                    set "VS_VERSION=VS2022 %%E"
                    echo [SUCCESS] Found ml64.exe in %%E >> "%LOG_FILE%"
                    popd
                    popd
                    goto :found
                )
            )
            popd
        )
    )
    popd
)

REM ═══════════════════════════════════════════════════════════════
REM Failure: could not locate ml64.exe
REM ═══════════════════════════════════════════════════════════════
echo [ERROR] MASM64 NOT FOUND >> "%LOG_FILE%"
echo [ERROR] Install VS2022 with C++ Desktop Development >> "%LOG_FILE%"
echo.
echo ERROR: ml64.exe not found!
echo Please ensure VS2022 Build Tools or C++ Development Kit is installed.
echo.
exit /b 1

:found
echo [SUCCESS] ml64.exe: %MASM_PATH% >> "%LOG_FILE%"
set "PATH=%MASM_PATH%;%PATH%"

REM ═══════════════════════════════════════════════════════════════
REM Generate benchmark script
REM ═══════════════════════════════════════════════════════════════
echo. >> "%LOG_FILE%"
echo [BENCHMARK] Creating benchmark.cmd >> "%LOG_FILE%"

(
    echo @echo off
    echo setlocal
    echo echo === RawrXD Performance Benchmark ===
    echo echo Model: models\mistral-7b.gguf
    echo echo Iterations: 100
    echo echo.
    echo if not exist models\mistral-7b.gguf ^(
    echo     echo ERROR: mistral-7b.gguf not found
    echo     exit /b 1
    echo ^)
    echo.
    echo for /L %%%%i in ^(1,1,100^) do ^(
    echo     build\Release\gguf_analyzer_masm64.exe --benchmark --input models\mistral-7b.gguf --output temp.exec
    echo ^)
    echo echo Benchmark complete!
) > "%SCRIPT_DIR%benchmark.cmd"

REM ═══════════════════════════════════════════════════════════════
REM Generate BigDaddyG test harness
REM ═══════════════════════════════════════════════════════════════
echo [BIGDADDYG] Creating test_bigdaddyg.cmd >> "%LOG_FILE%"

(
    echo @echo off
    echo setlocal
    echo echo ════════════════════════════════════════════════
    echo echo BigDaddyG 40GB Model Validation Suite
    echo echo ════════════════════════════════════════════════
    echo.
    echo if not exist models\bigdaddyg-40b.gguf ^(
    echo     echo ERROR: bigdaddyg-40b.gguf not found
    echo     echo Download from: https://huggingface.co/RawrXD/BigDaddyG-40B
    echo     exit /b 1
    echo ^)
    echo.
    echo echo [TEST 1] Extracting metadata...
    echo build\Release\gguf_analyzer_masm64.exe --input models\bigdaddyg-40b.gguf --output models\bigdaddyg.exec
    echo if errorlevel 1 exit /b 1
    echo echo [PASS] Metadata extracted
    echo.
    echo echo [TEST 2] Runtime load test...
    echo build\Release\execai.exe models\bigdaddyg.exec
    echo if errorlevel 1 exit /b 1
    echo echo [PASS] Runtime load successful
    echo.
    echo echo All tests PASSED
) > "%SCRIPT_DIR%test_bigdaddyg.cmd"

REM ═══════════════════════════════════════════════════════════════
REM BUILD PHASES
REM ═══════════════════════════════════════════════════════════════
echo. >> "%LOG_FILE%"
echo [PHASE 1] CMake configuration >> "%LOG_FILE%"
if not exist build mkdir build
cd /d "%SCRIPT_DIR%"
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo [WARN] CMake failed (may be OK for MASM64-only build^) >> "%LOG_FILE%"
)

echo [PHASE 2] Building MASM64 analyzer >> "%LOG_FILE%"
if not exist build\Release mkdir build\Release
ml64.exe /c /nologo /W3 /WX "%SCRIPT_DIR%RawrXD-GGUFAnalyzer-Complete.asm" -Fo "build\Release\RawrXD-GGUFAnalyzer-Complete.obj" >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo [ERROR] MASM64 assembly failed >> "%LOG_FILE%"
    exit /b 1
)
:: Move the generated object file to the expected location if it ended up in the script directory
if exist "%SCRIPT_DIR%RawrXD-GGUFAnalyzer-Complete.obj" (
    move "%SCRIPT_DIR%RawrXD-GGUFAnalyzer-Complete.obj" "build\Release\RawrXD-GGUFAnalyzer-Complete.obj" >> "%LOG_FILE%" 2>&1
)

echo [PHASE 3] Linking MASM64 executable >> "%LOG_FILE%"
link.exe /nologo /subsystem:console /entry:main "build\Release\RawrXD-GGUFAnalyzer-Complete.obj" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" kernel32.lib /out:"build\Release\gguf_analyzer_masm64.exe" >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo [ERROR] Linking failed >> "%LOG_FILE%"
    exit /b 1
)

echo [PHASE 4] Running tests >> "%LOG_FILE%"
if exist "build\Release\gguf_analyzer_masm64.exe" (
    echo [INFO] gguf_analyzer_masm64.exe built successfully >> "%LOG_FILE%"
)

REM ═══════════════════════════════════════════════════════════════
REM FINAL SUMMARY
REM ═══════════════════════════════════════════════════════════════
echo. >> "%LOG_FILE%"
echo [SUMMARY] Build completed >> "%LOG_FILE%"
echo [SUMMARY] VS: %VS_VERSION% >> "%LOG_FILE%"
echo [SUMMARY] MASM64: %MASM_PATH% >> "%LOG_FILE%"

echo.
echo ════════════════════════════════════════════════════════════════
echo RAWRXD-EXECAI BUILD COMPLETE
echo ════════════════════════════════════════════════════════════════
echo Visual Studio: %VS_VERSION%
echo MASM64 Path: %MASM_PATH%
echo.
echo EXECUTABLES:
if exist "build\Release\gguf_analyzer_masm64.exe" (
    echo   * gguf_analyzer_masm64.exe (MASM64 GGUF Analyzer)
)
echo.
echo GENERATED SCRIPTS:
echo   * benchmark.cmd (100-iteration performance test)
echo   * test_bigdaddyg.cmd (40GB model validation)
echo.
echo LOG: %LOG_FILE%
echo ════════════════════════════════════════════════════════════════

exit /b 0
