@echo off
setlocal EnableDelayedExpansion
echo ===========================================
echo RawrXD v3.0 C++ Builder (De-simulated)
echo ===========================================

set SRCS=^
 src\main.cpp^
 src\agentic_ide.cpp^
 src\gui\native_editor.cpp^
 src\cli\enhanced_cli.cpp^
 src\backend\vulkan_compute.cpp^
 src\ai\gguf_parser.cpp^
 src\ai\universal_model_router.cpp^
 src\ai\token_generator.cpp^
 src\agentic\swarm_orchestrator.cpp^
 src\agentic\chain_of_thought.cpp^
 src\RawrXD_Editor.cpp^
 src\RawrXD_Window.cpp^
 src\RawrXD_TextBuffer.cpp^
 src\RawrXD_Renderer_D2D.cpp^
 src\RawrXD_UndoStack.cpp^
 src\RawrXD_StyleManager.cpp^
 src\RawrXD_Lexer.cpp

set LIBS=user32.lib gdi32.lib shell32.lib advapi32.lib ole32.lib shlwapi.lib

if not exist release mkdir release

echo [Compiling...]
cl /nologo /EHsc /std:c++20 /O2 /I src /I include %SRCS% /Fe:release\RawrXD_v3.exe %LIBS%

if %ERRORLEVEL% NEQ 0 (
    echo [!] Build Failed
    exit /b %ERRORLEVEL%
)

echo [Success] Output: release\RawrXD_v3.exe
echo Done.
