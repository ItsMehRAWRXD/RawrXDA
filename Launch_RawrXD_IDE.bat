@echo off
REM Run RawrXD-Win32IDE with working directory = repo root (config, crash_dumps, plugins resolve here)
setlocal
cd /d "%~dp0"
if not exist "build_ide\bin\RawrXD-Win32IDE.exe" (
    echo RawrXD-Win32IDE.exe not found. Build with: cmake --build build_ide --target RawrXD-Win32IDE
    pause
    exit /b 1
)
start "" "build_ide\bin\RawrXD-Win32IDE.exe"
endlocal
