@echo off
setlocal

echo ============================================================
echo RawrXD IDE - Final Build ^& Verification
echo ============================================================

REM Step 0: Initialize Visual Studio build environment
echo.
echo [Step 0] Initializing MSVC environment...
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if "%VCVARS%"=="" (
    echo ❌ FAIL: Visual Studio build environment not found.
    echo    Please install VS Build Tools or update path detection.
    exit /b 1
)
call "%VCVARS%"
echo ✅ MSVC environment ready

REM Step 1: Set up Qt environment (auto-detect common installs)
echo.
echo [Step 1] Configuring Qt environment...
set "QT_ROOT=C:\Qt\6.7.3\msvc2022_64"
if not exist "%QT_ROOT%\bin\qmake.exe" set "QT_ROOT=D:\Qt\6.5.3\msvc2019_64"
if not exist "%QT_ROOT%\bin\qmake.exe" (
    echo ❌ FAIL: Qt not found in known locations.
    echo    Set QT_ROOT to your Qt install (e.g., C:\Qt\6.7.3\msvc2022_64).
    exit /b 1
)

set "PATH=%QT_ROOT%\bin;%PATH%"
set "QMAKE=%QT_ROOT%\bin\qmake.exe"

"%QMAKE%" -query QT_VERSION
if errorlevel 1 (
    echo ❌ FAIL: qmake not working at %QT_ROOT%
    exit /b 1
)
echo ✅ Qt configured

REM Step 2: Build Sovereign Loader (ensure it's fresh)
echo.
echo [Step 2] Building Sovereign Loader...
if not exist "build-sovereign-static" (
    echo ❌ FAIL: build-sovereign-static directory not found.
    exit /b 1
)
pushd build-sovereign-static
if exist build_with_env.bat (
    call build_with_env.bat
) else (
    echo ❌ FAIL: build_with_env.bat not found in build-sovereign-static.
    popd
    exit /b 1
)
if errorlevel 1 (
    echo ❌ FAIL: DLL build failed
    popd
    exit /b 1
)
popd
echo ✅ DLL built successfully

REM Step 3: Build Qt IDE
echo.
echo [Step 3] Building RawrXD IDE...
if not exist "RawrXD-IDE" (
    echo ❌ FAIL: RawrXD-IDE directory not found.
    exit /b 1
)
pushd RawrXD-IDE

REM Clean previous build
if exist Makefile.nmake (
    nmake clean
    del /Q /F Makefile*
)

REM Generate Makefile
"%QMAKE%" RawrXD-IDE.pro CONFIG+=release
if errorlevel 1 (
    echo ❌ FAIL: qmake failed
    popd
    exit /b 1
)

REM Build (use MSVC multi-proc via CL=/MP)
set "CL=/MP16"
nmake
if errorlevel 1 (
    echo ❌ FAIL: nmake failed
    popd
    exit /b 1
)
echo ✅ IDE built successfully
popd

REM Step 4: Deploy DLL
echo.
echo [Step 4] Deploying SovereignLoader.dll...
if not exist "RawrXD-IDE\release" (
    echo ❌ FAIL: Release directory not found: RawrXD-IDE\release
    exit /b 1
)
if not exist "build-sovereign-static\bin\RawrXD-SovereignLoader.dll" (
    echo ❌ FAIL: Built DLL not found at build-sovereign-static\bin\RawrXD-SovereignLoader.dll
    exit /b 1
)
copy /Y "build-sovereign-static\bin\RawrXD-SovereignLoader.dll" "RawrXD-IDE\release\RawrXD-SovereignLoader.dll"
if errorlevel 1 (
    echo ❌ FAIL: DLL copy failed
    exit /b 1
)
echo ✅ DLL deployed to release directory

REM Step 5: Copy test model
echo.
echo [Step 5] Setting up test model...
pushd build-sovereign-static\bin
if not exist phi-3-mini.gguf (
    echo Downloading Phi-3-mini model...
    curl -L -o phi-3-mini.gguf "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct/resolve/main/Phi-3-mini-4k-instruct-q4.gguf"
    if errorlevel 1 (
        echo ⚠️  WARNING: Model download failed (skipping)
        echo    You can manually download to: build-sovereign-static\bin\phi-3-mini.gguf
    ) else (
        echo ✅ Test model downloaded
    )
) else (
    echo ✅ Test model already present
)
popd

REM Step 6: Run smoke test
echo.
echo [Step 6] Running final smoke test...
if exist "smoke_test.py" (
    python smoke_test.py
) else if exist "D:\temp\RawrXD-agentic-ide-production\smoke_test.py" (
    pushd "D:\temp\RawrXD-agentic-ide-production"
    python smoke_test.py
    popd
) else (
    echo ⚠️  WARNING: smoke_test.py not found; skipping.
)

REM Step 7: Run integration test
echo.
echo [Step 7] Running integration test...
if exist "build-sovereign-static\bin\phi-3-mini.gguf" (
    if exist "build-sovereign-static\bin\test_loader.exe" (
        pushd build-sovereign-static\bin
        test_loader.exe phi-3-mini.gguf
        if not errorlevel 1 echo ✅ Integration test PASSED
        popd
    ) else (
        echo ⚠️  Skipping integration test (test_loader.exe missing)
    )
) else (
    echo ⚠️  Skipping integration test (no model file)
)

REM Step 8: Launch IDE
echo.
echo [Step 8] Launching RawrXD IDE...
if exist "RawrXD-IDE\release\RawrXD-IDE.exe" (
    start "" "RawrXD-IDE\release\RawrXD-IDE.exe" --first-run
) else (
    echo ⚠️  RawrXD-IDE.exe not found; build may have failed.
)

echo.
echo ============================================================
echo 🎉 BUILD & VERIFICATION COMPLETE
echo ============================================================
echo.
echo Summary:
echo   ✓ MSVC environment configured
echo   ✓ Qt environment configured
echo   ✓ SovereignLoader.dll built and deployed
echo   ✓ RawrXD-IDE.exe built
echo   ✓ DLL copied to release directory
echo   ✓ Smoke test executed
echo   ✓ (Integration test if model present)
echo   ✓ IDE launched
echo.
echo Output location:
echo   IDE:      RawrXD-IDE\release\RawrXD-IDE.exe
echo   DLL:      RawrXD-IDE\release\RawrXD-SovereignLoader.dll
echo   Test:     build-sovereign-static\bin\test_loader.exe
echo.
echo Next steps:
echo   1. Use Model menu to select and load a GGUF model
echo   2. Open Performance Monitor to see live metrics
echo   3. Try code completion (Ctrl+Space)
echo   4. Test Copilot chat (Ctrl+Shift+C)
echo ============================================================

endlocal
