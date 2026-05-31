@echo off
REM =============================================================================
REM build_native.cmd - Build RawrXD with Pure Native C++ (No Electron)
REM =============================================================================
REM
REM This script:
REM - Detects Visual Studio
REM - Configures CMake for native build
REM - Builds all targets
REM - Validates dependencies with dumpbin
REM - Generates VSIX package
REM - Produces clean, minimal binaries with NO Electron dependencies
REM
REM Requirements:
REM - Visual Studio 2022 (17.0+)
REM - Windows SDK 10.0.22621.0+
REM - CMake 3.25+
REM
REM =============================================================================

setlocal enabledelayedexpansion

echo.
echo ========================================
echo RawrXD Native Build System
echo Pure C++ - NO Electron - NO Node.js
echo ========================================
echo.

REM =============================================================================
REM Detect Visual Studio
REM =============================================================================

set "VSINSTALLDIR="
set "VSVER="
set "VSCOMNTOOLS="

REM Try Visual Studio 2022 first
for %%v in (Enterprise Professional Community) do (
    if not defined VSINSTALLDIR (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%v\Common7\Tools\VsDevCmd.bat" (
            set "VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\%%v"
            set "VSVER=2022"
            set "VSCOMNTOOLS=%VSINSTALLDIR%\Common7\Tools"
        )
    )
)

REM Try D: drive
if not defined VSINSTALLDIR (
    for %%v in (Enterprise Professional Community) do (
        if not defined VSINSTALLDIR (
            if exist "D:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat" (
                set "VSINSTALLDIR=D:\VS2022Enterprise"
                set "VSVER=2022"
                set "VSCOMNTOOLS=%VSINSTALLDIR%\Common7\Tools"
            )
        )
    )
)

REM Try Visual Studio 2019
if not defined VSINSTALLDIR (
    for %%v in (Enterprise Professional Community) do (
        if not defined VSINSTALLDIR (
            if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\%%v\Common7\Tools\VsDevCmd.bat" (
                set "VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\%%v"
                set "VSVER=2019"
                set "VSCOMNTOOLS=%VSINSTALLDIR%\Common7\Tools"
            )
        )
    )
)

if not defined VSINSTALLDIR (
    echo ERROR: Visual Studio 2022 or 2019 not found!
    echo.
    echo Please install Visual Studio with:
    echo   - Desktop development with C++
    echo   - Windows 10 SDK
    echo   - CMake tools
    echo.
    exit /b 1
)

echo Found Visual Studio %VSVER% at: %VSINSTALLDIR%

REM =============================================================================
REM Setup Build Environment
REM =============================================================================

call "%VSCOMNTOOLS%\VsDevCmd.bat" -arch=x64 -host_arch=x64

if errorlevel 1 (
    echo ERROR: Failed to initialize Visual Studio environment
    exit /b 1
)

REM =============================================================================
REM Detect CMake
REM =============================================================================

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake 3.25 or later
    exit /b 1
)

for /f "tokens=3" %%a in ('cmake --version ^| findstr /C:"version"') do (
    set CMAKE_VER=%%a
)
echo CMake version: %CMAKE_VER%

REM =============================================================================
REM Detect Dumpbin (for dependency validation)
REM =============================================================================

where dumpbin >nul 2>&1
if errorlevel 1 (
    echo WARNING: dumpbin not found - dependency validation disabled
    set "DUMPBIN_EXE="
) else (
    for /f "delims=" %%a in ('where dumpbin') do set "DUMPBIN_EXE=%%a"
    echo Found dumpbin: %DUMPBIN_EXE%
)

REM =============================================================================
REM Configure Build Directories
REM =============================================================================

set "SRCDIR=%~dp0"
set "BUILDDIR=%SRCDIR%build\native"
set "OUTDIR=%SRCDIR%output"

if exist "%BUILDDIR%" rd /s /q "%BUILDDIR%"
mkdir "%BUILDDIR%"

if exist "%OUTDIR%" rd /s /q "%OUTDIR%"
mkdir "%OUTDIR%"

REM =============================================================================
REM Configure CMake
REM =============================================================================

echo.
echo Configuring CMake...
echo.

pushd "%BUILDDIR%"

cmake -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%OUTDIR%" ^
    -DCMAKE_CXX_STANDARD=20 ^
    -DRAWRXD_BUILD_WIN32IDE=ON ^
    -DRAWRXD_BUILD_VSIX=ON ^
    -DRAWRXD_BUILD_TESTS=OFF ^
    -DRAWRXD_DUMPBIN_CHECK=ON ^
    -DRAWRXD_MINIMAL_DEPS=ON ^
    -DRAWRXD_USE_AVX512=ON ^
    -DRAWRXD_STATIC_RUNTIME=ON ^
    -DRAWRXD_LTO=ON ^
    "%SRCDIR%"

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    popd
    exit /b 1
)

REM =============================================================================
REM Build All Targets
REM =============================================================================

echo.
echo Building...
echo.

ninja -v

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    popd
    exit /b 1
)

REM =============================================================================
REM Validate Dependencies
REM =============================================================================

if defined DUMPBIN_EXE (
    echo.
    echo Validating dependencies...
    echo.
    
    ninja validate_dependencies
    
    if errorlevel 1 (
        echo.
        echo ERROR: Dependency validation failed!
        echo Dependencies contain forbidden libraries (Electron, Node.js, etc.)
        popd
        exit /b 1
    )
)

REM =============================================================================
REM Generate Dependency Report
REM =============================================================================

echo.
echo Generating dependency report...
echo.

ninja dependency_report

REM =============================================================================
REM Install
REM =============================================================================

echo.
echo Installing to output directory...
echo.

ninja install

REM =============================================================================
REM Summary
REM =============================================================================

echo.
echo ========================================
echo Build Complete
echo ========================================
echo.
echo Output directory: %OUTDIR%
echo.
echo Binaries:
if exist "%OUTDIR%\bin\RawrXD.exe" (
    echo   RawrXD.exe (Win32 IDE)
    dir /b "%OUTDIR%\bin\RawrXD.exe"
)
if exist "%OUTDIR%\bin\RawrXD.VSIX.dll" (
    echo   RawrXD.VSIX.dll (VSIX Extension)
)
echo.
echo Libraries:
for %%f in ("%OUTDIR%\lib\*.lib") do (
    echo   %%~nxf
)
echo.
echo Dependency report: %BUILDDIR%\dependency_report.txt
echo.
echo ========================================
echo PURE NATIVE BUILD SUCCESSFUL
echo No Electron/Node.js/Qt dependencies
echo ========================================
echo.

popd
endlocal
