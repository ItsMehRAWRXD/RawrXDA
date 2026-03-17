@echo off
REM RawrXD Model Loader - Proper Compilation with VsDevCmd
REM This script compiles the full project correctly without CMake

setlocal enabledelayedexpansion

REM Setup VS environment
echo Setting up Visual Studio environment...
call "C:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64

REM Create output directory
if not exist "build\bin" mkdir "build\bin"

REM Compile sources
echo.
echo Compiling RawrXD Model Loader with Clang-CL...
echo.

clang-cl ^
    /O2 ^
    /EHsc ^
    /std:c++20 ^
    /W4 ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /D_WINDOWS ^
    /Iinclude ^
    src/main.cpp ^
    src/gguf_loader.cpp ^
    src/vulkan_compute.cpp ^
    src/hf_downloader.cpp ^
    src/gui.cpp ^
    src/api_server.cpp ^
    /link ^
    vulkan-1.lib ^
    ws2_32.lib ^
    winmm.lib ^
    imm32.lib ^
    ole32.lib ^
    oleaut32.lib ^
    winspool.lib ^
    advapi32.lib ^
    shell32.lib ^
    user32.lib ^
    gdi32.lib ^
    /OUT:build\bin\RawrXD-ModelLoader.exe

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Compilation failed with exit code %ERRORLEVEL%
    exit /b 1
)

echo.
echo [SUCCESS] Compilation complete!
echo Executable: build\bin\RawrXD-ModelLoader.exe
echo.

dir build\bin\RawrXD-ModelLoader.exe

endlocal
