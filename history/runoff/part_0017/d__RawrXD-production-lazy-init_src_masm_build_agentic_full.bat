@echo off
REM =====================================================================
REM Build script for RawrXD Agentic Kernel and all components
REM =====================================================================

setlocal enabledelayedexpansion

echo ========================================
echo RawrXD Agentic Kernel Build System
echo ========================================
echo.

REM Check for ml64
where ml64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: ml64 not found. Please run from VS Developer Command Prompt.
    exit /b 1
)

REM Check for link
where link >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: link not found. Please run from VS Developer Command Prompt.
    exit /b 1
)

echo [1/8] Compiling agentic_kernel.asm...
ml64 /c /Fo"agentic_kernel.obj" /nologo /W3 /Zi agentic_kernel.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile agentic_kernel.asm
    exit /b 1
)
echo SUCCESS

echo [2/8] Compiling language_scaffolders.asm...
ml64 /c /Fo"language_scaffolders.obj" /nologo /W3 /Zi language_scaffolders.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile language_scaffolders.asm
    exit /b 1
)
echo SUCCESS

echo [3/8] Compiling agentic_ide_bridge.asm...
ml64 /c /Fo"agentic_ide_bridge.obj" /nologo /W3 /Zi agentic_ide_bridge.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile agentic_ide_bridge.asm
    exit /b 1
)
echo SUCCESS

echo [4/8] Compiling react_vite_scaffolder.asm...
ml64 /c /Fo"react_vite_scaffolder.obj" /nologo /W3 /Zi react_vite_scaffolder.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile react_vite_scaffolder.asm
    exit /b 1
)
echo SUCCESS

echo [5/8] Compiling existing agentic_tools.asm...
ml64 /c /Fo"agentic_tools.obj" /nologo /W3 /Zi agentic_tools.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile agentic_tools.asm
    exit /b 1
)
echo SUCCESS

echo [6/8] Compiling existing ui_masm.asm...
ml64 /c /Fo"ui_masm.obj" /nologo /W3 /Zi ui_masm.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile ui_masm.asm
    exit /b 1
)
echo SUCCESS

echo [7/8] Compiling universal_dispatcher.asm...
ml64 /c /Fo"universal_dispatcher.obj" /nologo /W3 /Zi universal_dispatcher.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile universal_dispatcher.asm
    exit /b 1
)
echo SUCCESS

echo [8/9] Compiling referenced modules...
ml64 /c /Fo"agentic_masm.obj" /nologo /W3 /Zi agentic_masm.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile agentic_masm.asm
    exit /b 1
)
ml64 /c /Fo"advanced_planning_engine.obj" /nologo /W3 /Zi advanced_planning_engine.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile advanced_planning_engine.asm
    exit /b 1
)
ml64 /c /Fo"rest_api_server_full.obj" /nologo /W3 /Zi rest_api_server_full.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile rest_api_server_full.asm
    exit /b 1
)
ml64 /c /Fo"distributed_tracer.obj" /nologo /W3 /Zi distributed_tracer.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile distributed_tracer.asm
    exit /b 1
)
ml64 /c /Fo"enterprise_common.obj" /nologo /W3 /Zi enterprise_common.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile enterprise_common.asm
    exit /b 1
)
ml64 /c /Fo"asm_log.obj" /nologo /W3 /Zi asm_log.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile asm_log.asm
    exit /b 1
)
echo SUCCESS

echo [9/9] Compiling build test...
ml64 /c /Fo"build_test.obj" /nologo /W3 /Zi build_test.asm
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile build_test.asm
    exit /b 1
)
echo SUCCESS

echo [10/10] Linking all components...
link /OUT:RawrXD_AgenticIDE.exe ^
    /SUBSYSTEM:WINDOWS ^
    /MACHINE:X64 ^
    /DEBUG ^
    /NOLOGO ^
    agentic_kernel.obj ^
    language_scaffolders.obj ^
    agentic_ide_bridge.obj ^
    react_vite_scaffolder.obj ^
    agentic_tools.obj ^
    ui_masm.obj ^
    universal_dispatcher.obj ^
    agentic_masm.obj ^
    advanced_planning_engine.obj ^
    rest_api_server_full.obj ^
    distributed_tracer.obj ^
    enterprise_common.obj ^
    asm_log.obj ^
    build_test.obj ^
    kernel32.lib ^
    user32.lib ^
    shell32.lib ^
    advapi32.lib ^
    gdi32.lib ^
    comctl32.lib ^
    comdlg32.lib

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to link executable
    exit /b 1
)
echo SUCCESS

echo [10/10] Build complete!
echo.
echo Output: RawrXD_AgenticIDE.exe
echo.
echo Features enabled:
echo   - Universal Dispatcher with intent classification
echo   - Drag and drop file support
echo   - Instrumentation and performance monitoring
echo   - 40-agent swarm
echo   - 800-B embedded model
echo   - 50+ language support
echo   - React/Vite scaffolding
echo   - QT IDE integration
echo   - CLI IDE integration
echo   - Deep research mode
echo   - Autonomous execution
echo.
echo To run: RawrXD_AgenticIDE.exe
echo.

endlocal
exit /b 0
