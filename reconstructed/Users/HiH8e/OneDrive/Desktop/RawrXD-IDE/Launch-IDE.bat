@echo off
chcp 65001 >nul
echo ================================================
echo   RawrXD IDE Launcher
echo ================================================
echo.
echo Available IDEs:
echo   1. RawrXD-AgenticIDE (Recommended - Full Features)
echo   2. RawrXD-QtShell (Lightweight Qt Interface)
echo   3. RawrXD-Win32IDE (Native Windows Interface)
echo.
set /p choice="Select IDE (1-3) or press Enter for default (1): "
if "%choice%"=="" set choice=1
if "%choice%"=="1" (
    echo Launching RawrXD-AgenticIDE...
    start "" "RawrXD-AgenticIDE.exe"
) else if "%choice%"=="2" (
    echo Launching RawrXD-QtShell...
    start "" "RawrXD-QtShell.exe"
) else if "%choice%"=="3" (
    echo Launching RawrXD-Win32IDE...
    start "" "RawrXD-Win32IDE.exe"
) else (
    echo Invalid choice. Launching default...
    start "" "RawrXD-AgenticIDE.exe"
)
echo.
echo IDE launched successfully!
pause