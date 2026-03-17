@echo off
REM ============================================================================
REM build_phase2_x64.bat
REM Build Phase 2 Integration with proper x64 MASM syntax
REM ============================================================================

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║    Building Phase 2 Integration - Pure x64 MASM              ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

cd /d "%~dp0"

REM Create build directory
if not exist "build_phase2" mkdir build_phase2

REM Step 1: Assemble menu_system.asm
echo [1/5] Assembling menu_system.asm...
ml64 /c /Zi /Cp /Fo"build_phase2\menu_system.obj" "src\masm\final-ide\menu_system.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble menu_system.asm
    goto :error
)

REM Step 2: Assemble masm_theme_system_complete.asm
echo [2/5] Assembling masm_theme_system_complete.asm...
ml64 /c /Zi /Cp /Fo"build_phase2\masm_theme_system_complete.obj" "src\masm\final-ide\masm_theme_system_complete.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble masm_theme_system_complete.asm
    goto :error
)

REM Step 3: Assemble masm_file_browser_complete.asm
echo [3/5] Assembling masm_file_browser_complete.asm...
ml64 /c /Zi /Cp /Fo"build_phase2\masm_file_browser_complete.obj" "src\masm\final-ide\masm_file_browser_complete.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble masm_file_browser_complete.asm
    goto :error
)

REM Step 4: Assemble phase2_integration.asm
echo [4/5] Assembling phase2_integration.asm...
ml64 /c /Zi /Cp /Fo"build_phase2\phase2_integration.obj" "src\masm\final-ide\phase2_integration.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble phase2_integration.asm
    goto :error
)

REM Step 5: Assemble phase2_test_main.asm
echo [5/5] Assembling phase2_test_main.asm...
ml64 /c /Zi /Cp /Fo"build_phase2\phase2_test_main.obj" "src\masm\final-ide\phase2_test_main.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble phase2_test_main.asm
    goto :error
)

REM Step 6: Link all objects
echo.
echo [6/6] Linking phase2_integration_test.exe...
link /SUBSYSTEM:WINDOWS ^
     /OUT:"build_phase2\phase2_integration_test.exe" ^
     /DEBUG ^
     /ENTRY:mainCRTStartup ^
     "build_phase2\menu_system.obj" ^
     "build_phase2\masm_theme_system_complete.obj" ^
     "build_phase2\masm_file_browser_complete.obj" ^
     "build_phase2\phase2_integration.obj" ^
     "build_phase2\phase2_test_main.obj" ^
     kernel32.lib user32.lib gdi32.lib ^
     comctl32.lib shell32.lib shlwapi.lib ^
     ole32.lib advapi32.lib msvcrt.lib

if errorlevel 1 (
    echo ERROR: Failed to link executable
    goto :error
)

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║                  ✅ BUILD SUCCESSFUL ✅                        ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.
echo Executable: build_phase2\phase2_integration_test.exe
echo Size: 
dir "build_phase2\phase2_integration_test.exe" | find ".exe"
echo.
echo Test Command: build_phase2\phase2_integration_test.exe
echo.
goto :end

:error
echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║                    ❌ BUILD FAILED ❌                          ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.
echo Check the error messages above for details.
echo.
exit /b 1

:end
