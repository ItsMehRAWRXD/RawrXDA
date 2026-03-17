@echo off
echo Testing GlassQuill build...
echo.

cd /d "D:\cursor-multi-ai-extension\glassquill"

REM Try to find Visual Studio
set "VSDIR="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo ERROR: Visual Studio 2022 not found!
    echo Please install Visual Studio 2022 with C++ support
    pause
    exit /b 1
)

echo Found Visual Studio at: %VSDIR%
echo Setting up build environment...

call "%VSDIR%" >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to setup VS environment
    pause
    exit /b 1
)

echo Building GlassQuill with Marketplace + AI...
cd src
cl /EHsc /O2 /std:c++17 glassquill.cpp /Fe:../GlassQuill-MarketplaceAI.exe /link user32.lib gdi32.lib opengl32.lib dwmapi.lib shell32.lib ole32.lib ws2_32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ================================
    echo BUILD SUCCESSFUL!
    echo ================================
    echo.
    echo Executable: GlassQuill-MarketplaceAI.exe
    echo.
    echo NEW FEATURES:
    echo • Chameleon color effects (F4)
    echo • Marketplace browser (Ctrl+M)
    echo • AI code completion (Ctrl+Space)
    echo • Extension auto-installer
    echo.
    echo CONTROLS:
    echo • F4 = Toggle chameleon
    echo • Ctrl+M = Marketplace browser
    echo • Ctrl+Space = AI completion
    echo • ESC = Exit
    echo.
) else (
    echo.
    echo BUILD FAILED!
    echo Check errors above.
)

cd ..
pause