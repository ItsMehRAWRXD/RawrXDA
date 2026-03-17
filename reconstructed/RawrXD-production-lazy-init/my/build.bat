@echo off
if not defined VSCMD_VER (
    call "C:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
)
set "PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64;%PATH%"

echo ========================================
echo Building Production Pure MASM IDE
echo ========================================

set MASM_DIR=src\masm\final-ide
set BUILD_DIR=build_production_masm
set OBJ_DIR=%BUILD_DIR%\obj
set BIN_DIR=%BUILD_DIR%\bin

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %OBJ_DIR% mkdir %OBJ_DIR%
if not exist %BIN_DIR% mkdir %BIN_DIR%

echo [0/5] Compiling MASM Stub Implementations...
echo --------------------------------------------
REM ml64 /c /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um" /Fo%OBJ_DIR%\masm_stubs.obj src\masm_stubs.asm
REM if errorlevel 1 goto :error

echo [1/4] Compiling Core System Components...
echo ----------------------------------------
ml64 /c /Fo%OBJ_DIR%\asm_sync.obj %MASM_DIR%\asm_sync.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\asm_memory.obj %MASM_DIR%\asm_memory.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\caching_layer.obj %MASM_DIR%\caching_layer.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\asm_string.obj %MASM_DIR%\asm_string.asm
if errorlevel 1 goto :error

echo [2/4] Compiling UI and Editor Components...
echo ------------------------------------------
ml64 /c /Fo%OBJ_DIR%\main_masm.obj %MASM_DIR%\main_masm.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\ui_masm.obj %MASM_DIR%\ui_masm.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\missing_ui_functions.obj %MASM_DIR%\missing_ui_functions.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\main_window_masm.obj %MASM_DIR%\main_window_masm.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\text_editor.obj %MASM_DIR%\text_editor.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\ide_components.obj %MASM_DIR%\ide_components.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\masm_command_palette.obj %MASM_DIR%\masm_command_palette.asm
if errorlevel 1 goto :error

echo [3/4] Compiling AI and Agentic Components...
echo --------------------------------------------
ml64 /c /Fo%OBJ_DIR%\agentic_engine.obj %MASM_DIR%\agentic_engine.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\agentic_puppeteer.obj %MASM_DIR%\agentic_puppeteer.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\autonomous_task_executor_clean.obj %MASM_DIR%\autonomous_task_executor_clean.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\ai_orchestration_glue_clean.obj %MASM_DIR%\ai_orchestration_glue_clean.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\masm_inference_engine.obj %MASM_DIR%\masm_inference_engine.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\ml_masm.obj %MASM_DIR%\ml_masm.asm
if errorlevel 1 goto :error

echo [4/4] Compiling Hotpatch and Utility Components...
echo -------------------------------------------------
ml64 /c /Fo%OBJ_DIR%\unified_hotpatch_manager.obj %MASM_DIR%\unified_hotpatch_manager.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\model_memory_hotpatch.obj %MASM_DIR%\model_memory_hotpatch.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\byte_level_hotpatcher.obj %MASM_DIR%\byte_level_hotpatcher.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\masm_gguf_parser.obj %MASM_DIR%\masm_gguf_parser.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\logging.obj %MASM_DIR%\logging.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\console_log.obj %MASM_DIR%\console_log.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\missing_implementations.obj %MASM_DIR%\missing_implementations.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\final_missing_symbols.obj %MASM_DIR%\final_missing_symbols.asm
if errorlevel 1 goto :error

echo [5/4] Compiling Complete Implementation Files...
echo ------------------------------------------------
ml64 /c /Fo%OBJ_DIR%\complete_implementations.obj %MASM_DIR%\complete_implementations.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\complete_implementations_part2.obj %MASM_DIR%\complete_implementations_part2.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\complete_implementations_part3.obj %MASM_DIR%\complete_implementations_part3.asm
if errorlevel 1 goto :error
ml64 /c /Fo%OBJ_DIR%\complete_implementations_part4.obj %MASM_DIR%\complete_implementations_part4.asm
if errorlevel 1 goto :error

echo [LINK] Linking Production Executable...
echo ---------------------------------------
link /SUBSYSTEM:WINDOWS /ENTRY:main_entry /FORCE:MULTIPLE ^
     /OUT:%BIN_DIR%\RawrXD-MASM-IDE.exe ^
     %OBJ_DIR%\*.obj ^
     kernel32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib ^
     shell32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib ^
     dwmapi.lib msimg32.lib uxtheme.lib ws2_32.lib dwrite.lib d2d1.lib
if errorlevel 1 goto :error

echo BUILD SUCCESSFUL!
exit /b 0

:error
echo BUILD FAILED!
exit /b 1
