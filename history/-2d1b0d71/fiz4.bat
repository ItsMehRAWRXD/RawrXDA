@echo off
REM Quick build script for Phase 2 pure MASM IDE test
REM This demonstrates what we have in pure MASM

echo ========================================
echo  Building Pure MASM Phase 2 IDE
echo  Components: Menu + Theme + File Browser
echo ========================================
echo.

cd /d "%~dp0\src\masm\final-ide"

if not exist "build_phase2" mkdir build_phase2

echo [1/5] Assembling menu_system.asm...
ml64 /c /Zi /Fo"build_phase2\menu_system.obj" "menu_system.asm" 2>&1 | findstr /V "Assembling"
if errorlevel 1 (
    echo ERROR: menu_system.asm failed to assemble
    echo Check for syntax errors in menu_system.asm
    pause
    exit /b 1
)

echo [2/5] Assembling masm_theme_system_complete.asm...
ml64 /c /Zi /Fo"build_phase2\masm_theme_system_complete.obj" "masm_theme_system_complete.asm" 2>&1 | findstr /V "Assembling"
if errorlevel 1 (
    echo ERROR: masm_theme_system_complete.asm failed to assemble
    echo This file may need Windows SDK includes
    pause
    exit /b 1
)

echo [3/5] Assembling masm_file_browser_complete.asm...
ml64 /c /Zi /Fo"build_phase2\masm_file_browser_complete.obj" "masm_file_browser_complete.asm" 2>&1 | findstr /V "Assembling"
if errorlevel 1 (
    echo ERROR: masm_file_browser_complete.asm failed to assemble
    pause
    exit /b 1
)

echo [4/5] Assembling phase2_integration.asm...
ml64 /c /Zi /Fo"build_phase2\phase2_integration.obj" "phase2_integration.asm" 2>&1 | findstr /V "Assembling"
if errorlevel 1 (
    echo ERROR: phase2_integration.asm failed to assemble
    pause
    exit /b 1
)

echo [5/5] Assembling phase2_test_main.asm...
ml64 /c /Zi /Fo"build_phase2\phase2_test_main.obj" "phase2_test_main.asm" 2>&1 | findstr /V "Assembling"
if errorlevel 1 (
    echo ERROR: phase2_test_main.asm failed to assemble
    pause
    exit /b 1
)

echo.
echo [LINK] Linking phase2_ide_test.exe...
link /SUBSYSTEM:WINDOWS ^
     /OUT:"build_phase2\phase2_ide_test.exe" ^
     /DEBUG ^
     /ENTRY:mainCRTStartup ^
     "build_phase2\menu_system.obj" ^
     "build_phase2\masm_theme_system_complete.obj" ^
     "build_phase2\masm_file_browser_complete.obj" ^
     "build_phase2\phase2_integration.obj" ^
     "build_phase2\phase2_test_main.obj" ^
     kernel32.lib user32.lib gdi32.lib ^
     comctl32.lib shell32.lib shlwapi.lib ^
     ole32.lib advapi32.lib msvcrt.lib 2>&1 | findstr /V "library"

if errorlevel 1 (
    echo.
    echo ERROR: Linking failed
    echo Check for missing exports or unresolved symbols
    pause
    exit /b 1
)

echo.
echo ========================================
echo  BUILD SUCCESSFUL!
echo ========================================
echo  Executable: build_phase2\phase2_ide_test.exe
echo  Size: 
dir "build_phase2\phase2_ide_test.exe" | findstr "phase2"
echo.
echo  Pure MASM Components:
echo   - Menu System: 645 LOC
echo   - Theme System: 900+ LOC  
echo   - File Browser: 1,200+ LOC
echo   - Integration: 500+ LOC
echo   Total: 3,245+ lines pure x64 assembly
echo.
echo  Run with: .\build_phase2\phase2_ide_test.exe
echo ========================================
