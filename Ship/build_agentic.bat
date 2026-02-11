@echo off
setlocal

:: Set paths (adjust if needed or rely on existing environment)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" (
    set "ML64=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
    set "LINK=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
) else (
    set "ML64=ml64.exe"
    set "LINK=link.exe"
)

echo ============================================================================
echo Building RawrXD Agentic Backend v5.0...
echo ============================================================================
"%ML64%" /c /Fo rawrxd_agentic.obj rawrxd_agentic.asm
if %errorlevel% neq 0 goto error

"%LINK%" rawrxd_agentic.obj /subsystem:windows /entry:WinMain ^
    /defaultlib:kernel32.lib user32.lib shell32.lib ws2_32.lib ^
    wininet.lib shlwapi.lib gdi32.lib gdiplus.lib ole32.lib ^
    /out:rawrxd_agentic.exe
if %errorlevel% neq 0 goto error

echo.
echo ============================================================================
echo Building RawrXD MASM Backend Bridge v4.0...
echo ============================================================================
"%ML64%" /c /Fo rawrxd_bridge.obj rawrxd_bridge.asm
if %errorlevel% neq 0 goto error

"%LINK%" rawrxd_bridge.obj /subsystem:windows /entry:WinMain ^
    /defaultlib:kernel32.lib user32.lib shell32.lib ws2_32.lib ^
    wininet.lib shlwapi.lib ^
    /out:rawrxd_bridge.exe
if %errorlevel% neq 0 goto error

echo.
echo ============================================================================
echo BUILD SUCCESSFUL
echo ============================================================================
goto :eof

:error
echo.
echo ============================================================================
echo BUILD FAILED
echo ============================================================================
exit /b 1
