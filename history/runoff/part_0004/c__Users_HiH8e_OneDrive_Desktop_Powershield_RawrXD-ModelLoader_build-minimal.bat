@echo off
REM Ultra-minimal working compilation
REM Just main.cpp with no external dependencies

setlocal enabledelayedexpansion

if not exist "build\bin" mkdir "build\bin"

echo Compiling RawrXD Model Loader (Minimal Build)...

g++.exe ^
    -O2 ^
    -std=c++20 ^
    -Wall ^
    -o build\bin\RawrXD-ModelLoader.exe ^
    src\main-minimal.cpp

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo [SUCCESS] Executable created: build\bin\RawrXD-ModelLoader.exe
dir build\bin\RawrXD-ModelLoader.exe

endlocal
