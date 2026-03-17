@echo off
REM ============================================================================
REM build_plugins.bat - Build example plugins as DLLs
REM ============================================================================

setlocal enabledelayedexpansion

if not exist Plugins mkdir Plugins

echo Building FileHashPlugin.dll...
ml64.exe /c /coff FileHashPlugin.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble FileHashPlugin.asm
    exit /b 1
)

link.exe /DLL /ENTRY:PluginMain /OUT:Plugins\FileHashPlugin.dll ^
         kernel32.lib advapi32.lib FileHashPlugin.obj
if errorlevel 1 (
    echo ERROR: Failed to link FileHashPlugin.dll
    exit /b 1
)

echo SUCCESS: All plugins built
echo Output: Plugins\ folder
pause
