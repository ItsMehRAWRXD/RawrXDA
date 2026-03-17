@echo off
REM Build script for RawrXD Cloud Integration
REM Compiles hybrid cloud manager, HF client, and example

setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================
echo RawrXD Cloud Integration Build Script
echo ============================================

REM Check for compiler
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo Error: MSVC compiler not found
    echo Please run from Visual Studio Developer Command Prompt
    exit /b 1
)

REM Create output directory
if not exist bin mkdir bin
if not exist obj mkdir obj

echo.
echo Compiling cloud integration system...
echo.

REM Compile source files
set COMPILE_FLAGS=/std:c++17 /W4 /EHsc /Iinclude /Fd:obj\ /Fo:obj\
set COMPILE_FLAGS=!COMPILE_FLAGS! /D_CRT_SECURE_NO_WARNINGS /D_WINDOWS

echo [1/4] Compiling hybrid_cloud_manager.cpp...
cl.exe !COMPILE_FLAGS! hybrid_cloud_manager.cpp
if errorlevel 1 goto :error

echo [2/4] Compiling win_http_client.cpp...
cl.exe !COMPILE_FLAGS! win_http_client.cpp
if errorlevel 1 goto :error

echo [3/4] Compiling cloud_integration_example.cpp...
cl.exe !COMPILE_FLAGS! cloud_integration_example.cpp
if errorlevel 1 goto :error

echo [4/4] Linking...
link.exe /OUT:bin\cloud_integration.exe ^
    obj\hybrid_cloud_manager.obj ^
    obj\win_http_client.obj ^
    obj\cloud_integration_example.obj ^
    winhttp.lib
if errorlevel 1 goto :error

echo.
echo ============================================
echo Build successful!
echo ============================================
echo.
echo Output: bin\cloud_integration.exe
echo.
echo To run:
echo   bin\cloud_integration.exe
echo.
goto :end

:error
echo.
echo ============================================
echo Build failed!
echo ============================================
exit /b 1

:end
endlocal
