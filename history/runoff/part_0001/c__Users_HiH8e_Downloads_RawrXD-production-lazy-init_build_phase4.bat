@echo off
REM ============================================================================
REM Phase 4 Build Script - Settings Dialog
REM ============================================================================
REM Builds the Phase 4 settings dialog components
REM ============================================================================

setlocal

REM Set paths
set ML64_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set LINK_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
set SDK_PATH="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
set CRT_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"

REM Set source directory
set SRC_DIR=src\masm\final-ide

REM Create output directory
if not exist build\phase4 mkdir build\phase4

REM Build registry persistence layer
echo Building registry_persistence.asm...
%ML64_PATH% /c /Fo build\phase4\registry_persistence.obj %SRC_DIR%\registry_persistence.asm
if %errorlevel% neq 0 (
    echo Error building registry_persistence.asm
    exit /b 1
)

REM Build settings dialog
echo Building qt6_settings_dialog.asm...
%ML64_PATH% /c /Fo build\phase4\qt6_settings_dialog.obj %SRC_DIR%\qt6_settings_dialog.asm
if %errorlevel% neq 0 (
    echo Error building qt6_settings_dialog.asm
    exit /b 1
)

REM Link Phase 4 components
echo Linking Phase 4 components...
%LINK_PATH% /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
    build\phase4\registry_persistence.obj ^
    build\phase4\qt6_settings_dialog.obj ^
    kernel32.lib user32.lib gdi32.lib comctl32.lib advapi32.lib ^
    /OUT:build\phase4\phase4_test.exe

if %errorlevel% neq 0 (
    echo Error linking Phase 4 components
    exit /b 1
)

echo Phase 4 build completed successfully!
echo Executable: build\phase4\phase4_test.exe

endlocal