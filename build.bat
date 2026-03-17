@echo off
REM Build script for RawrXD TextEditorGUI
REM Requires ml64.exe and Windows SDK

echo ============================================
echo Building RawrXD Text Editor GUI
echo ============================================

REM Check if ml64.exe is available
where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: ml64.exe not found in PATH
    echo Make sure MASM64 is installed
    goto error
)

echo.
echo [1/3] Assembling main editor module...
ml64 /c /Fo:RawrXD_TextEditorGUI.obj RawrXD_TextEditorGUI.asm /W3
if errorlevel 1 goto error

echo [2/3] Assembling WinMain integration example...
ml64 /c /Fo:WinMain_Integration_Example.obj WinMain_Integration_Example.asm /W3
if errorlevel 1 goto error

echo [3/3] Assembling AI completion integration module...
ml64 /c /Fo:AICompletionIntegration.obj AICompletionIntegration.asm /W3
if errorlevel 1 goto error

echo.
echo [4/4] Linking executable...
link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup ^
    /MACHINE:X64 ^
    RawrXD_TextEditorGUI.obj WinMain_Integration_Example.obj AICompletionIntegration.obj ^
    kernel32.lib user32.lib gdi32.lib ^
    /OUT:RawrXD_TextEditorGUI.exe
if errorlevel 1 goto error

echo.
echo ============================================
echo Build completed successfully!
echo ============================================
echo.
echo Executable: RawrXD_TextEditorGUI.exe
echo.
echo Run with: RawrXD_TextEditorGUI.exe
echo.
goto end

:error
echo.
echo ============================================
echo ERROR: Build failed!
echo ============================================
exit /b 1

:end