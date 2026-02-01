cmake_minimum_required(VERSION 3.20)
project(RawrXD VERSION 1.0.0 LANGUAGES CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# No Qt
# No AUTOMOC/AUTORCC/AUTOUIC

add_definitions(
    -DUNICODE
    -D_UNICODE
    # -DWIN32_LEAN_AND_MEAN
    -D_WIN32_WINNT=0x0601
    -DNOMINMAX
    -DRAWRXD_NO_QT
    -DNDEBUG
)

# Include directories
include_directories(
    include
    src
    src/qtapp
    src/gui
    src/orchestration
    src/agentic
    src/compiler
    src/inference
    src/streaming
    src/masm
    third_party/directx/include
)

# Source files (Qt-free)
set(RAWRXD_SOURCES
    # Main
    src/main.cpp
    
    # Application
    src/RawrXD_Application.cpp
    src/RawrXD_Window.cpp
    # src/qtapp/RawrXD_DocumentManager.cpp # Not created yet
    
    # GUI
    src/gui/RawrXD_EditorWindow.cpp
    src/gui/RawrXD_Sidebar.cpp
    src/gui/RawrXD_Panel.cpp
    src/gui/ThermalDashboardWidget.cpp
    src/gui/ModelConversionDialog.cpp
    src/gui/sovereign_dashboard_widget.cpp
    src/ai_integration_hub.cpp
    src/ai_model_caller.cpp
    src/agentic_configuration.cpp
    # src/gui/RawrXD_MenuBar.cpp
    # src/gui/RawrXD_ToolBar.cpp
    # src/gui/RawrXD_StatusBar.cpp
    
    # Orchestration
    # src/orchestration/RawrXD_Orchestrator.cpp
    
    # Agentic
    # src/agentic/RawrXD_AgentEngine.cpp
    
    # Compiler
    # src/compiler/RawrXD_Compiler.cpp
    
    # Inference
    # src/inference/RawrXD_InferenceEngine.cpp
    
    # Streaming
    # src/streaming/RawrXD_StreamingEngine.cpp

    # Core Logic
    src/agentic_engine.cpp
    src/universal_model_router.cpp
    src/streaming_gguf_loader.cpp
    src/cpu_inference_engine.cpp
    src/cloud_api_client.cpp
    src/RawrXD_PipeClient.cpp
    src/intelligent_codebase_engine.cpp
    src/model_trainer.cpp
    
    # Utils
    src/utils/Diagnostics.cpp
)

# MASM sources
set(RAWRXD_ASM_SOURCES_REAL
    # Core Engine (Real Logic)
    src/RawrXD_Titan.asm
    src/RawrXD_System_Primitives.asm
    src/RawrXD_GPU_Memory.asm
    src/RawrXD_Inference_Engine.asm
    src/RawrXD_Model_StateMachine.asm
    
    # Legacy/Streaming
    # src/masm/RawrXD_Streaming_Orchestrator.asm
    # src/masm/RawrXD_Math_AVX512.asm
    # src/masm/RawrXD_Quantize.asm
    # src/masm/RawrXD_Compress.asm
    # src/masm/RawrXD_GPU_Vulkan.asm
)

# Executable
add_executable(RawrXD WIN32
    ${RAWRXD_SOURCES}
    # No direct ASM link for Client (uses DLL or Pipe)
)

# Link libraries (Windows only, no Qt)
target_link_libraries(RawrXD PRIVATE
    kernel32
    user32
    gdi32
    comctl32
    comdlg32
    shell32
    ole32
    oleaut32
    uuid
    d2d1
    dwrite
    windowscodecs
    winhttp
    ws2_32
    dbghelp
    shlwapi
    version
    winmm
    imm32
    # vulkan-1
)

# --- Agentic IDE (Native Win32) Target ---
set(AGENTIC_SOURCES
    src/agentic_ide_main.cpp
    src/agentic_ide.cpp
    src/agentic_engine.cpp
    src/autonomous_model_manager.cpp
    src/universal_model_router.cpp
    src/cpu_inference_engine.cpp
    src/cloud_api_client.cpp
    src/tool_registry.cpp
    src/plan_orchestrator.cpp
    src/zero_day_agentic_engine.cpp
    src/multi_tab_editor.cpp
    src/terminal_pool.cpp
    src/chat_interface.cpp
    src/lsp_client.cpp
    src/RawrXD_Application.cpp
    src/RawrXD_Editor.cpp
    src/RawrXD_Window.cpp
    src/RawrXD_Renderer_D2D.cpp
    src/RawrXD_Lexer_MASM.cpp
    src/RawrXD_StyleManager.cpp
    src/RawrXD_TextBuffer.cpp
    src/RawrXD_SignalSlot.cpp
    src/RawrXD_UndoStack.cpp
    src/RawrXD_Lexer.cpp
    src/autonomous_intelligence_orchestrator.cpp
    src/streaming_gguf_loader.cpp
    src/intelligent_codebase_engine.cpp
    src/autonomous_feature_engine.cpp
    src/hybrid_cloud_manager.cpp
    src/tool_registry_init.cpp
    src/RawrXD_MainIntegration.cpp
    src/RawrXD_PipeClient.cpp
    src/ai_model_caller.cpp
    src/agentic_configuration.cpp
    src/utils/Diagnostics.cpp
)

add_executable(RawrXD-AgenticIDE WIN32 ${AGENTIC_SOURCES} ${RAWRXD_ASM_SOURCES_REAL})
target_compile_definitions(RawrXD-AgenticIDE PRIVATE RAWRXD_STATIC_ASM)

target_link_libraries(RawrXD-AgenticIDE PRIVATE
    kernel32 user32 gdi32 comctl32 comdlg32 shell32 
    ole32 oleaut32 uuid d2d1 dwrite windowscodecs 
    winhttp wininet ws2_32 dbghelp shlwapi version winmm imm32
)
# --- CLI Compiler Target ---
add_executable(rawrxd-cli
    src/cli/rawrxd_cli_compiler.cpp
    src/ai_model_caller.cpp
    src/cpu_inference_engine.cpp
    src/universal_model_router.cpp
    src/utils/Diagnostics.cpp
    src/streaming_gguf_loader.cpp
    src/cloud_api_client.cpp
    src/RawrXD_PipeClient.cpp
    src/agentic_configuration.cpp
)
target_link_libraries(rawrxd-cli PRIVATE
    kernel32 user32 gdi32 shlwapi winhttp ws2_32
)
