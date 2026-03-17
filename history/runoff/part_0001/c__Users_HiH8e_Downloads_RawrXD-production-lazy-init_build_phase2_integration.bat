@echo off
REM ============================================================================
REM build_phase2_integration.bat
REM Build script for Phase 2 integration test
REM ============================================================================

echo ╔════════════════════════════════════════════════════════════════════════╗
echo ║         Building Phase 2 Integration Test (Pure MASM x64)            ║
echo ╚════════════════════════════════════════════════════════════════════════╝
echo.

REM Navigate to MASM directory
cd /d "%~dp0"

REM Set paths
set MASM_DIR=src\masm\final-ide
set BUILD_DIR=build_phase2
set OUTPUT_EXE=phase2_integration_test.exe

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [1/6] Assembling menu_system.asm...
ml64 /c /Zi /Fo"%BUILD_DIR%\menu_system.obj" "%MASM_DIR%\menu_system.asm"
if errorlevel 1 goto :error

echo [2/6] Assembling masm_theme_system_complete.asm...
ml64 /c /Zi /Fo"%BUILD_DIR%\masm_theme_system_complete.obj" "%MASM_DIR%\masm_theme_system_complete.asm"
if errorlevel 1 goto :error

echo [3/6] Assembling masm_file_browser_complete.asm...
ml64 /c /Zi /Fo"%BUILD_DIR%\masm_file_browser_complete.obj" "%MASM_DIR%\masm_file_browser_complete.asm"
if errorlevel 1 goto :error

echo [4/6] Assembling phase2_integration.asm...
ml64 /c /Zi /Fo"%BUILD_DIR%\phase2_integration.obj" "%MASM_DIR%\phase2_integration.asm"
if errorlevel 1 goto :error

echo [5/6] Assembling phase2_test_main.asm...
ml64 /c /Zi /Fo"%BUILD_DIR%\phase2_test_main.obj" "%MASM_DIR%\phase2_test_main.asm"
if errorlevel 1 goto :error

echo [6/6] Linking executable...
link /SUBSYSTEM:WINDOWS ^
     /OUT:"%BUILD_DIR%\%OUTPUT_EXE%" ^
     /DEBUG ^
     "%BUILD_DIR%\menu_system.obj" ^
     "%BUILD_DIR%\masm_theme_system_complete.obj" ^
     "%BUILD_DIR%\masm_file_browser_complete.obj" ^
     "%BUILD_DIR%\phase2_integration.obj" ^
     "%BUILD_DIR%\phase2_test_main.obj" ^
     kernel32.lib user32.lib gdi32.lib ^
     comctl32.lib shell32.lib shlwapi.lib ^
     ole32.lib advapi32.lib msvcrt.lib

if errorlevel 1 goto :error

echo.
echo ╔════════════════════════════════════════════════════════════════════════╗
echo ║                      ✅ BUILD SUCCESSFUL ✅                            ║
echo ╚════════════════════════════════════════════════════════════════════════╝
echo.
echo Executable: %BUILD_DIR%\%OUTPUT_EXE%
echo.
echo Run with: %BUILD_DIR%\%OUTPUT_EXE%
echo.
goto :end

:error
echo.
echo ╔════════════════════════════════════════════════════════════════════════╗
echo ║                       ❌ BUILD FAILED ❌                               ║
echo ╚════════════════════════════════════════════════════════════════════════╝
echo.
exit /b 1

:end
