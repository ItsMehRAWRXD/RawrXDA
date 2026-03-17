@echo off
cd /d "D:\cursor-multi-ai-extension\glassquill"
echo ========================================
echo   GlassQuill Marketplace + AI Builder
echo ========================================
echo.

rem Check if we're in the right directory
if not exist "src\glassquill.cpp" (
    echo ✗ ERROR: glassquill.cpp not found!
    echo.
    echo Make sure you're running this from:
    echo D:\cursor-multi-ai-extension\glassquill\
    echo.
    pause
    exit /b 1
)

echo ✓ Found source file: src\glassquill.cpp

rem Try to find and setup Visual Studio environment
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    echo ✓ Found Visual Studio Installer
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
    
    if defined VS_PATH (
        echo ✓ Found Visual Studio at: %VS_PATH%
        call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
        if !ERRORLEVEL! EQU 0 (
            echo ✓ Visual Studio environment loaded
            goto :build
        )
    )
)

rem Fallback: Try common VS paths
for %%V in (2022 2019) do (
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvars64.bat"
    if exist "!VCVARS!" (
        echo ✓ Found Visual Studio %%V Community
        call "!VCVARS!" >nul 2>nul
        if !ERRORLEVEL! EQU 0 (
            echo ✓ Visual Studio environment loaded
            goto :build
        )
    )
    
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\%%V\Professional\VC\Auxiliary\Build\vcvars64.bat"
    if exist "!VCVARS!" (
        echo ✓ Found Visual Studio %%V Professional  
        call "!VCVARS!" >nul 2>nul
        if !ERRORLEVEL! EQU 0 (
            echo ✓ Visual Studio environment loaded
            goto :build
        )
    )
)

rem If we get here, no Visual Studio found
echo ✗ ERROR: Visual Studio C++ compiler not found!
echo.
echo Please install Visual Studio 2019 or 2022 with C++ support, or
echo run this from "x64 Native Tools Command Prompt for VS"
echo.
pause
exit /b 1

:build
echo.
echo Building GlassQuill with Marketplace + AI features...

cd src
cl /EHsc /O2 /std:c++17 glassquill.cpp /Fe:../GlassQuill-MarketplaceAI.exe /link user32.lib gdi32.lib opengl32.lib dwmapi.lib shell32.lib ole32.lib ws2_32.lib
cd ..

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo ✓ BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Executable created: GlassQuill-MarketplaceAI.exe
    echo.
    echo NEW FEATURES:
    echo • Chameleon color-changing text effect (F4)
    echo • Marketplace extension browser (Ctrl+M)  
    echo • AI code completion via Ollama (Ctrl+Space)
    echo • Extension auto-installation system
    echo • Real-time transparency controls
    echo • Professional editor layout with line numbers
    echo.
    echo CONTROLS:
    echo • F4 = Toggle chameleon effect
    echo • Ctrl+M = Browse marketplace and install extension
    echo • Ctrl+Space = AI code completion (needs Ollama running)
    echo • ESC = Exit
    echo • Mouse wheel = Scroll
    echo.
    echo To start Ollama server: ollama serve
    echo Then run: GlassQuill-MarketplaceAI.exe
    echo.
) else (
    echo.
    echo ✗ BUILD FAILED!
    echo.
    echo Check for compilation errors above.
    echo Make sure all required libraries are available.
)

pause