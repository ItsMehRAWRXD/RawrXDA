@echo off
cd /d "D:\cursor-multi-ai-extension\glassquill"

rem Setup VS environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul

echo Building GlassQuill with Tabs and Drive Browser...
cd src
cl /EHsc /O2 /std:c++17 glassquill.cpp /Fe:../GlassQuill-TabsAndDrives.exe /link user32.lib gdi32.lib opengl32.lib dwmapi.lib shell32.lib ole32.lib ws2_32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ================================
    echo BUILD SUCCESSFUL!
    echo ================================
    echo.
    echo NEW FEATURES ADDED:
    echo • Multi-tab editing system
    echo • Drive browser (C:, D:, etc.)
    echo • Folder navigation and file loading
    echo • Enhanced UI with tab bar
    echo.
    echo CONTROLS:
    echo • Ctrl+T = New tab
    echo • Ctrl+W = Close tab  
    echo • Ctrl+Tab = Switch tabs
    echo • Ctrl+D = Browse drives
    echo • Ctrl+O = Browse folders
    echo • Ctrl+M = Marketplace
    echo • Ctrl+Space = AI completion
    echo • F4 = Toggle chameleon
    echo • Click tabs to switch, X to close
    echo.
) else (
    echo BUILD FAILED!
)

pause