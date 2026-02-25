@echo off
setlocal enabledelayedexpansion

echo Looking for Visual Studio...

REM Try to find VS using common paths
set "VSYEAR=2022 2019 2017"
set "VSEDITION=Community Professional Enterprise BuildTools"
set "FOUND=0"

for %%Y in (%VSYEAR%) do (
    for %%E in (%VSEDITION%) do (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!VCVARS!" (
            echo Found Visual Studio %%Y %%E
            call "!VCVARS!" >nul 2>&1
            set "FOUND=1"
            goto :compile
        )
    )
)

:compile
if "%FOUND%"=="0" (
    echo ERROR: Visual Studio not found!
    echo Please install Visual Studio Build Tools or use Developer Command Prompt.
    echo.
    echo Alternatively, download portable MinGW and place it in toolchains\mingw64\
    pause
    exit /b 1
)

echo Compiling ScreenPilotPP.exe...
cl /nologo /EHsc /std:c++20 /W3 src\main.cpp /Fe:ScreenPilotPP.exe user32.lib gdi32.lib comctl32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build SUCCESS! Fixed version with proper colors created.
    echo Starting ScreenPilotPP.exe...
    start ScreenPilotPP.exe
) else (
    echo.
    echo Build FAILED!
)

pause

