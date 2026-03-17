@echo off
REM RawrXD Model Loader - MinGW GCC Compilation
REM Uses MinGW64 GCC compiler which has a working toolchain

setlocal enabledelayedexpansion

if not exist "build\bin" mkdir "build\bin"

echo.
echo Compiling RawrXD Model Loader with MinGW GCC...
echo.

REM MinGW compiler
set COMPILER=g++.exe

%COMPILER% ^
    -O2 ^
    -std=c++20 ^
    -Wall ^
    -Wextra ^
    -D_WINDOWS ^
    -D_CRT_SECURE_NO_WARNINGS ^
    -Iinclude ^
    src/main.cpp ^
    src/gguf_loader.cpp ^
    src/vulkan_compute.cpp ^
    src/hf_downloader.cpp ^
    src/gui.cpp ^
    src/api_server.cpp ^
    -lvulkan-1 ^
    -lws2_32 ^
    -lwinmm ^
    -limm32 ^
    -lole32 ^
    -loleaut32 ^
    -lwinspool ^
    -ladvapi32 ^
    -lshell32 ^
    -luser32 ^
    -lgdi32 ^
    -o build\bin\RawrXD-ModelLoader.exe

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
