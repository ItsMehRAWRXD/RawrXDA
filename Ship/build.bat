@echo off
REM build.bat - MSVC Build Script for RawrXD Agent
REM Pure C++20 / Win32 - Zero Qt Dependencies

setlocal EnableDelayedExpansion

echo ================================================
echo RawrXD Agent Build System
echo ================================================
echo.

REM Find Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
    set "VSINSTALL=C:\VS2022Enterprise"
)

REM Set up environment
if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
) else (
    echo ERROR: Cannot find Visual Studio
    exit /b 1
)

REM Output directory
set "OUTDIR=build"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM Common compiler flags
set "CFLAGS=/std:c++20 /EHsc /W3 /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX"
set "LIBS=winhttp.lib ws2_32.lib comctl32.lib ole32.lib shell32.lib shlwapi.lib user32.lib gdi32.lib"

echo Building RawrXD Agent components...
echo.

REM Build Console Agent
echo [1/5] Building Console Agent...
cl %CFLAGS% /Fe"%OUTDIR%\RawrXD_Agent.exe" Integration.cpp /link %LIBS% /SUBSYSTEM:CONSOLE
if errorlevel 1 (
    echo FAILED: Console Agent
    goto :error
)
echo OK: RawrXD_Agent.exe

REM Copy as RawrXD_CLI.exe (same binary, full parity)
copy /Y "%OUTDIR%\RawrXD_Agent.exe" "%OUTDIR%\RawrXD_CLI.exe" >nul 2>&1
if exist "%OUTDIR%\RawrXD_CLI.exe" echo OK: RawrXD_CLI.exe (copy)

REM Build GUI Agent
echo [2/5] Building GUI Agent...
cl %CFLAGS% /Fe"%OUTDIR%\RawrXD_Agent_GUI.exe" Integration.cpp /link %LIBS% /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
if errorlevel 1 (
    echo FAILED: GUI Agent
    goto :error
)
echo OK: RawrXD_Agent_GUI.exe

REM Build Example Application
echo [3/5] Building Example Application...
cl %CFLAGS% /Fe"%OUTDIR%\RawrXD_Example.exe" Example_Application.cpp /link %LIBS% /SUBSYSTEM:CONSOLE
if errorlevel 1 (
    echo FAILED: Example Application
    goto :error
)
echo OK: RawrXD_Example.exe

REM Build Test Suite
echo [4/5] Building Test Suite...
if exist test_suite.cpp (
    cl %CFLAGS% /Fe"%OUTDIR%\RawrXD_Tests.exe" test_suite.cpp /link %LIBS% /SUBSYSTEM:CONSOLE
    if errorlevel 1 (
        echo FAILED: Test Suite
        goto :error
    )
    echo OK: RawrXD_Tests.exe
) else (
    echo SKIP: test_suite.cpp not found
)

REM Build Benchmark Suite
echo [5/5] Building Benchmark Suite...
if exist benchmark_suite.cpp (
    cl %CFLAGS% /Fe"%OUTDIR%\RawrXD_Benchmark.exe" benchmark_suite.cpp /link %LIBS% /SUBSYSTEM:CONSOLE
    if errorlevel 1 (
        echo FAILED: Benchmark Suite
        goto :error
    )
    echo OK: RawrXD_Benchmark.exe
) else (
    echo SKIP: benchmark_suite.cpp not found
)

REM Clean up object files
echo.
echo Cleaning up...
del /q *.obj 2>nul

echo.
echo ================================================
echo Build completed successfully!
echo Output: %OUTDIR%\
echo ================================================
echo.
dir /b "%OUTDIR%\*.exe"
echo.

goto :end

:error
echo.
echo ================================================
echo BUILD FAILED
echo ================================================
exit /b 1

:end
endlocal
