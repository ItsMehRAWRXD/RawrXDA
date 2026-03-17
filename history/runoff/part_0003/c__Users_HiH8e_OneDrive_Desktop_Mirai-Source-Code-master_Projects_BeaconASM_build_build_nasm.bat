@echo off
REM ============================================================================
REM NASM Build Script - Windows x64
REM ============================================================================

echo.
echo ================================================
echo   NASM Assembly Build System
echo ================================================
echo.

set "PROJECT_ROOT=%~dp0.."
set "SRC_DIR=%PROJECT_ROOT%\src\nasm"
set "BIN_DIR=%PROJECT_ROOT%\bin"
set "BUILD_DIR=%PROJECT_ROOT%\build\temp"

REM Check for NASM installation
set "NASM_PATH=C:\Program Files\NASM\nasm.exe"
if not exist "%NASM_PATH%" (
    echo ERROR: NASM not found at %NASM_PATH%
    echo Please install NASM from https://www.nasm.us/
    exit /b 1
)

REM Find Visual Studio linker
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    if errorlevel 1 (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
        if errorlevel 1 (
            echo ERROR: Visual Studio not found!
            echo Please install Visual Studio Build Tools or Community Edition
            exit /b 1
        )
    )
)

REM Setup Visual Studio environment for linker
set "VS_LINK_PATH=D:\Microsoft Visual Studio 2022\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
set "VS_LIB_PATH=D:\Microsoft Visual Studio 2022\VC\Tools\MSVC\14.44.35207\lib\x64"
set "SDK_LIB_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
set "UCRT_LIB_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"

REM Add to PATH
set "PATH=%VS_LINK_PATH%;%PATH%"
set "LIB=%VS_LIB_PATH%;%SDK_LIB_PATH%;%UCRT_LIB_PATH%;%LIB%"

REM Create directories
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [1/3] Assembling with NASM...
"%NASM_PATH%" -f win64 "%SRC_DIR%\hello.asm" -o "%BUILD_DIR%\hello.obj"
if errorlevel 1 (
    echo ERROR: NASM assembly failed!
    echo Check that the source file exists: %SRC_DIR%\hello.asm
    exit /b 1
)

echo [2/3] Linking...
link "%BUILD_DIR%\hello.obj" ^
    /SUBSYSTEM:CONSOLE ^
    /ENTRY:main ^
    /OUT:"%BIN_DIR%\hello_nasm.exe" ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" ^
    kernel32.lib
if errorlevel 1 (
    echo ERROR: Linking failed!
    exit /b 1
)

echo [3/3] Build complete!
echo.
echo ✅ Output: %BIN_DIR%\hello_nasm.exe
echo.
echo Running executable...
echo ================================================
"%BIN_DIR%\hello_nasm.exe"
echo ================================================
echo.

exit /b 0
