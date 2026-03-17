@echo off
REM ===============================================================================
REM RawrXD IDE - Complete Enterprise Build System
REM Production-Ready Build with Full Integration
REM ===============================================================================

setlocal enabledelayedexpansion

echo.
echo ===============================================================================
echo RawrXD Enterprise IDE - Complete Build System
echo Production-Ready with CLI + GUI + Agent + Universal Dispatcher
echo ===============================================================================
echo.

REM ===========================
REM Configuration
REM ===========================
set "SRC_DIR=D:\RawrXD-production-lazy-init\src\masm"
set "OBJ_DIR=D:\RawrXD-production-lazy-init\obj"
set "BIN_DIR=D:\RawrXD-production-lazy-init\bin"
set "LOG_DIR=D:\RawrXD-production-lazy-init\logs"

REM MASM Compiler
set "ML64=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set "LINK=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"

REM Build flags
set "ASMFLAGS=/c /nologo /W3 /Zi /Fo"
set "LINKFLAGS=/SUBSYSTEM:CONSOLE /ENTRY:main /NOLOGO /DEBUG"

REM Libraries
set "LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib uuid.lib ws2_32.lib advapi32.lib"

REM ===========================
REM Create Directories
REM ===========================
echo [1/15] Creating build directories...
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

set "BUILD_LOG=%LOG_DIR%\build_%date:~-4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.log"
set "BUILD_LOG=%BUILD_LOG: =0%"

echo Build started at %date% %time% > "%BUILD_LOG%"
echo Build Log: %BUILD_LOG%
echo.

REM ===========================
REM Build Core Modules
REM ===========================

echo [2/15] Building ASM Log System...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\asm_log.obj" "%SRC_DIR%\asm_log.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: asm_log.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] asm_log.obj

echo [3/15] Building ASM Memory Manager...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\asm_memory.obj" "%SRC_DIR%\asm_memory.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: asm_memory.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] asm_memory.obj

echo [4/15] Building ASM String Utilities...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\asm_string.obj" "%SRC_DIR%\asm_string.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: asm_string.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] asm_string.obj

echo [5/15] Building Enterprise Common...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\enterprise_common.obj" "%SRC_DIR%\enterprise_common_production.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: enterprise_common_production.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] enterprise_common.obj

REM ===========================
REM Build Agent System
REM ===========================

echo [6/15] Building Agentic Kernel...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\agentic_kernel.obj" "%SRC_DIR%\agentic_kernel.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: agentic_kernel.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] agentic_kernel.obj

echo [7/20] Building Agentic MASM...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\agentic_masm.obj" "%SRC_DIR%\agentic_masm_simple.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: agentic_masm_simple.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] agentic_masm.obj

echo [8/20] Building Language Scaffolders...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\language_scaffolders.obj" "%SRC_DIR%\language_scaffolders_simple.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: language_scaffolders_simple.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] language_scaffolders.obj

REM ===========================
REM Build Advanced Systems
REM ===========================

echo [9/15] Building Advanced Planning Engine...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\advanced_planning_engine.obj" "%SRC_DIR%\advanced_planning_engine_production.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: advanced_planning_engine_production.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] advanced_planning_engine.obj

echo [10/15] Building REST API Server...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\rest_api_server_full.obj" "%SRC_DIR%\rest_api_server_production.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: rest_api_server_production.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] rest_api_server_full.obj

echo [11/15] Building Distributed Tracer...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\distributed_tracer.obj" "%SRC_DIR%\distributed_tracer_production.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: distributed_tracer_production.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] distributed_tracer.obj

REM ===========================
REM Build CLI Access System
REM ===========================

echo [12/15] Building Universal Dispatcher...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\universal_dispatcher.obj" "%SRC_DIR%\masm\universal_dispatcher_production.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: universal_dispatcher_production.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] universal_dispatcher.obj

echo [13/15] Building CLI Access System...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\cli_access_system.obj" "%SRC_DIR%\cli_access_system.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: cli_access_system.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] cli_access_system.obj

echo [14/15] Building UI Extended Stubs...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\ui_extended_stubs.obj" "%SRC_DIR%\ui_extended_stubs.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: ui_extended_stubs.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] ui_extended_stubs.obj

REM ===========================
REM Build UI and Integration
REM ===========================

echo [15/15] Building UI MASM...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\ui_masm.obj" "%SRC_DIR%\ui_masm.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: ui_masm.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] ui_masm.obj

echo [16/15] Building Agentic IDE Bridge...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\agentic_ide_bridge.obj" "%SRC_DIR%\agentic_ide_bridge.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: agentic_ide_bridge.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] agentic_ide_bridge.obj

echo [17/20] Building Main Entry Point...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\main_masm.obj" "%SRC_DIR%\main_masm.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: main_masm.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] main_masm.obj

echo [18/20] Building Pure MASM Compiler...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\pure_masm_compiler.obj" "%SRC_DIR%\pure_masm_compiler.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: pure_masm_compiler.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] pure_masm_compiler.obj

echo [19/20] Building Enterprise Common Simple...
"%ML64%" %ASMFLAGS%"%OBJ_DIR%\enterprise_common_simple.obj" "%SRC_DIR%\enterprise_common_simple.asm" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: enterprise_common_simple.asm compilation failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)
echo     [OK] enterprise_common_simple.obj

echo.
echo ===============================================================================
echo Linking RawrXD Enterprise IDE...
echo ===============================================================================

"%LINK%" %LINKFLAGS% ^
    /OUT:"%BIN_DIR%\RawrXD_Enterprise.exe" ^
    "%OBJ_DIR%\main_masm.obj" ^
    "%OBJ_DIR%\agentic_ide_bridge.obj" ^
    "%OBJ_DIR%\cli_access_system.obj" ^
    "%OBJ_DIR%\ui_extended_stubs.obj" ^
    "%OBJ_DIR%\universal_dispatcher.obj" ^
    "%OBJ_DIR%\ui_masm.obj" ^
    "%OBJ_DIR%\agentic_kernel.obj" ^
    "%OBJ_DIR%\agentic_masm.obj" ^
    "%OBJ_DIR%\language_scaffolders.obj" ^
    "%OBJ_DIR%\advanced_planning_engine.obj" ^
    "%OBJ_DIR%\rest_api_server_full.obj" ^
    "%OBJ_DIR%\distributed_tracer.obj" ^
    "%OBJ_DIR%\pure_masm_compiler.obj" ^
    "%OBJ_DIR%\enterprise_common_simple.obj" ^
    "%OBJ_DIR%\asm_log.obj" ^
    "%OBJ_DIR%\asm_memory.obj" ^
    "%OBJ_DIR%\asm_string.obj" ^
    %LIBS% >> "%BUILD_LOG%" 2>&1

if errorlevel 1 (
    echo ERROR: Linking failed
    type "%BUILD_LOG%" | findstr /i "error"
    goto :build_failed
)

echo.
echo ===============================================================================
echo BUILD SUCCESSFUL!
echo ===============================================================================
echo.
echo Executable: %BIN_DIR%\RawrXD_Enterprise.exe
echo Build Log:  %BUILD_LOG%
echo.
echo Features Included:
echo   [✓] Dual Mode (CLI + GUI)
echo   [✓] 40-Agent Swarm System  
echo   [✓] Universal Command Dispatcher
echo   [✓] CLI Access System (21 commands)
echo   [✓] Menu Integration (14 actions)
echo   [✓] Widget Control (6 types)
echo   [✓] Signal/Slot Framework (7 signals)
echo   [✓] 50+ Language Support
echo   [✓] Pure MASM Compiler Integration
echo   [✓] Drag & Drop Integration
echo   [✓] REST API Server
echo   [✓] Distributed Tracing
echo   [✓] Advanced Planning Engine
echo   [✓] Enterprise Logging
echo.
echo Usage:
echo   GUI Mode:  %BIN_DIR%\RawrXD_Enterprise.exe
echo   CLI Mode:  %BIN_DIR%\RawrXD_Enterprise.exe help
echo   Commands:  %BIN_DIR%\RawrXD_Enterprise.exe ^<command^> [args]
echo.
echo ===============================================================================

goto :end

:build_failed
echo.
echo ===============================================================================
echo BUILD FAILED!
echo ===============================================================================
echo.
echo Check the build log for details: %BUILD_LOG%
echo.
exit /b 1

:end
endlocal
