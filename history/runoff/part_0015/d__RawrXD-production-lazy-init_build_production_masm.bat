@echo off
REM Production MASM Build Script - Complete Pure MASM IDE
REM Builds all completed MASM components into production-ready executable

echo ========================================
echo Building Production Pure MASM IDE
echo ========================================

REM Set paths
set MASM_DIR=src\masm\final-ide
set BUILD_DIR=build_production_masm
set OBJ_DIR=%BUILD_DIR%\obj
set BIN_DIR=%BUILD_DIR%\bin

REM Create build directories
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %OBJ_DIR% mkdir %OBJ_DIR%
if not exist %BIN_DIR% mkdir %BIN_DIR%

echo.
echo [1/4] Compiling Core System Components...
echo ----------------------------------------

REM Core system primitives
ml64 /c /Fo%OBJ_DIR%\asm_sync.obj %MASM_DIR%\asm_sync.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\asm_memory.obj %MASM_DIR%\asm_memory.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\caching_layer.obj %MASM_DIR%\caching_layer.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\asm_string.obj %MASM_DIR%\asm_string.asm
if errorlevel 1 goto :error

echo.
echo [2/4] Compiling UI and Editor Components...
echo ------------------------------------------

REM Main UI system
ml64 /c /Fo%OBJ_DIR%\main_masm.obj %MASM_DIR%\main_masm.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\ui_masm.obj %MASM_DIR%\ui_masm.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\missing_ui_functions.obj %MASM_DIR%\missing_ui_functions.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\main_window_masm.obj %MASM_DIR%\main_window_masm.asm
if errorlevel 1 goto :error

REM Editor system
ml64 /c /Fo%OBJ_DIR%\text_editor.obj %MASM_DIR%\text_editor.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\ide_components.obj %MASM_DIR%\ide_components.asm
if errorlevel 1 goto :error

REM Command palette
ml64 /c /Fo%OBJ_DIR%\masm_command_palette.obj %MASM_DIR%\masm_command_palette.asm
if errorlevel 1 goto :error

echo.
echo [3/4] Compiling AI and Agentic Components...
echo --------------------------------------------

REM Agentic system
ml64 /c /Fo%OBJ_DIR%\agentic_engine.obj %MASM_DIR%\agentic_engine.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\agentic_puppeteer.obj %MASM_DIR%\agentic_puppeteer.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\autonomous_task_executor_clean.obj %MASM_DIR%\autonomous_task_executor_clean.asm
if errorlevel 1 goto :error

REM AI orchestration
ml64 /c /Fo%OBJ_DIR%\ai_orchestration_glue_clean.obj %MASM_DIR%\ai_orchestration_glue_clean.asm
if errorlevel 1 goto :error

REM Inference engine
ml64 /c /Fo%OBJ_DIR%\masm_inference_engine.obj %MASM_DIR%\masm_inference_engine.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\ml_masm.obj %MASM_DIR%\ml_masm.asm
if errorlevel 1 goto :error

echo.
echo [4/4] Compiling Hotpatch and Utility Components...
echo -------------------------------------------------

REM Hotpatch system
ml64 /c /Fo%OBJ_DIR%\unified_hotpatch_manager.obj %MASM_DIR%\unified_hotpatch_manager.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\model_memory_hotpatch.obj %MASM_DIR%\model_memory_hotpatch.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\byte_level_hotpatcher.obj %MASM_DIR%\byte_level_hotpatcher.asm
if errorlevel 1 goto :error

REM GGUF and model loading
ml64 /c /Fo%OBJ_DIR%\masm_gguf_parser.obj %MASM_DIR%\masm_gguf_parser.asm
if errorlevel 1 goto :error

REM Logging and utilities
ml64 /c /Fo%OBJ_DIR%\logging.obj %MASM_DIR%\logging.asm
if errorlevel 1 goto :error

ml64 /c /Fo%OBJ_DIR%\console_log.obj %MASM_DIR%\console_log.asm
if errorlevel 1 goto :error

echo.
echo [LINK] Linking Production Executable...
echo ---------------------------------------

REM Link all objects into final executable
link /SUBSYSTEM:WINDOWS /ENTRY:main_entry ^
     /OUT:%BIN_DIR%\RawrXD-MASM-IDE.exe ^
     %OBJ_DIR%\*.obj ^
     kernel32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib ^
     shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib

if errorlevel 1 goto :error

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Production MASM IDE built successfully:
echo   Executable: %BIN_DIR%\RawrXD-MASM-IDE.exe
echo   Objects:    %OBJ_DIR%\
echo.
echo Key Features Implemented:
echo   ✓ Pure MASM x64 codebase (zero C++ dependencies)
echo   ✓ Complete Win32 UI system with VS Code layout
echo   ✓ Text editor with undo/redo functionality
echo   ✓ Command palette with fuzzy search
echo   ✓ Agentic AI system with task scheduling
echo   ✓ Autonomous task executor with retry logic
echo   ✓ Unified hotpatch manager with statistics
echo   ✓ GGUF model loading and inference engine
echo   ✓ Thread-safe synchronization primitives
echo   ✓ Comprehensive logging system
echo.
echo Ready for production deployment!
echo.
goto :end

:error
echo.
echo ========================================
echo BUILD FAILED!
echo ========================================
echo.
echo Error occurred during compilation.
echo Check the error messages above.
echo.
exit /b 1

:end