@echo off
REM ============================================================================
REM BUILD.bat - Complete RawrXD Pure MASM IDE Build (Production)
REM ============================================================================

setlocal enabledelayedexpansion
set BUILD_CONFIG=%1
if "%BUILD_CONFIG%"=="" set BUILD_CONFIG=Release

echo ============================================================================
echo RawrXD Pure MASM IDE Build System
echo ============================================================================
echo Configuration: %BUILD_CONFIG%
echo Time: %date% %time%
echo.

REM Detect MASM
set MASM_CMD=ml64.exe
set LINK_CMD=link.exe
set RC_CMD=rc.exe

where /q %MASM_CMD%
if errorlevel 1 (
    set "MASM_CMD=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
    set "LINK_CMD=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
    set "RC_CMD=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\rc.exe"
    
    if not exist "!MASM_CMD!" (
        echo ERROR: ml64.exe not found. Install Visual Studio 2022 with MSVC.
        exit /b 1
    )
)

echo [1/5] Assembling MASM runtime...
%MASM_CMD% /c /Zi /W3 /I. asm_memory.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. asm_sync.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. asm_string.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. asm_events.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. asm_log.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. console_log.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. process_manager.asm
if errorlevel 1 goto build_fail

echo [2/5] Assembling hotpatch layers...
%MASM_CMD% /c /Zi /W3 /I. model_memory_hotpatch.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. byte_level_hotpatcher.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. gguf_server_hotpatch.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. proxy_hotpatcher.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. unified_masm_hotpatch.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. unified_hotpatch_manager.asm
if errorlevel 1 goto build_fail

echo [3/5] Assembling agentic systems...
%MASM_CMD% /c /Zi /W3 /I. agentic_failure_detector.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. agentic_puppeteer.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. gui_designer_agent.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. extension_agent.asm
if errorlevel 1 goto build_fail

echo [4/5] Assembling model loader, agentic engine, and UI...
mkdir build\bin\%BUILD_CONFIG% 2>nul
%MASM_CMD% /c /Zi /W3 /I. ml_masm.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. plugin_loader.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. agentic_masm.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. rawr1024_dual_engine.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. ui_masm.asm
if errorlevel 1 goto build_fail
%MASM_CMD% /c /Zi /W3 /I. main_masm.asm
if errorlevel 1 goto build_fail

echo [5/5] Linking main executable...
mkdir build\bin\%BUILD_CONFIG% 2>nul

set SDK_LIB="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
set UCRT_LIB="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
set MSVC_LIB="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64"

%LINK_CMD% /SUBSYSTEM:CONSOLE /ENTRY:RawrMain /LARGEADDRESSAWARE:NO ^
       /LIBPATH:%SDK_LIB% /LIBPATH:%UCRT_LIB% /LIBPATH:%MSVC_LIB% ^
       /OUT:build\bin\%BUILD_CONFIG%\RawrXD.exe ^
       kernel32.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib ^
       comdlg32.lib wininet.lib dwmapi.lib uxtheme.lib d2d1.lib dwrite.lib ^
       advapi32.lib ntdll.lib wintrust.lib ^
       asm_memory.obj asm_sync.obj asm_string.obj asm_events.obj asm_log.obj console_log.obj process_manager.obj ^
       model_memory_hotpatch.obj byte_level_hotpatcher.obj ^
       gguf_server_hotpatch.obj proxy_hotpatcher.obj ^
       unified_masm_hotpatch.obj unified_hotpatch_manager.obj ^
       agentic_failure_detector.obj agentic_puppeteer.obj gui_designer_agent.obj extension_agent.obj ^
       ml_masm.obj plugin_loader.obj agentic_masm.obj rawr1024_dual_engine.obj ui_masm.obj main_masm.obj

if errorlevel 1 goto build_fail

echo.
echo ============================================================================
echo BUILD SUCCESS
echo ============================================================================
echo Executable: build\bin\%BUILD_CONFIG%\RawrXD.exe
for %%A in (build\bin\%BUILD_CONFIG%\RawrXD.exe) do (
    echo File size: %%~zA bytes
)
echo.
echo Next steps:
echo   1. Create Plugins\ folder
echo   2. Drop plugin DLLs into Plugins\
echo   3. Run: RawrXD.exe
echo.
goto end

:build_fail
echo.
echo ============================================================================
echo BUILD FAILED
echo ============================================================================
exit /b 1

:end
endlocal
