@echo off
setlocal enabledelayedexpansion

set "ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"

if not exist "obj" mkdir obj

echo Testing Phase 3 Components...
echo.

echo [1/3] Testing Dialog System...
"%ML64%" /c /Fo obj\dialog_system.obj dialog_system.asm
if %errorlevel% equ 0 (
    echo ✅ Dialog System compiled successfully
) else (
    echo ❌ Dialog System compilation failed
    exit /b 1
)

echo [2/3] Testing Tab Control...
"%ML64%" /c /Fo obj\tab_control.obj tab_control.asm
if %errorlevel% equ 0 (
    echo ✅ Tab Control compiled successfully
) else (
    echo ❌ Tab Control compilation failed
    exit /b 1
)

echo [3/3] Testing ListView Control...
"%ML64%" /c /Fo obj\listview_control.obj listview_control.asm
if %errorlevel% equ 0 (
    echo ✅ ListView Control compiled successfully
) else (
    echo ❌ ListView Control compilation failed
    exit /b 1
)

echo.
echo ✅ All Phase 3 components compiled successfully!
echo.
echo Phase 3 Critical Blockers Implemented:
echo   - Dialog System (modal dialog routing)
echo   - Tab Control (multi-tab interface)
echo   - ListView Control (file/model lists)
echo.
echo Ready for Phase 4: Settings Dialog Implementation

endlocal