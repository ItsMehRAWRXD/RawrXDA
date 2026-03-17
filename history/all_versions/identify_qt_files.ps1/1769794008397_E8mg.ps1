# Script to identify Qt GUI files vs core logic files
# Pure Qt GUI files (100% UI framework, safe to delete):

$GUI_FILES = @(
    # MainWindow variants
    "MainWindow.cpp", "MainWindow.h", "MainWindow.h.bak", "MainWindow_OLD.h",
    "MainWindowMinimal.cpp", "MainWindowMinimal.h",
    "MainWindowSimple.cpp", "MainWindowSimple.h", "MainWindowSimple_Utils.cpp",
    "MainWindow_AI_Integration.cpp", "MainWindow_ViewToggleConnections.h",
    "MainWindow_Widget_Integration.h", "MainWindow_v5.cpp", "MainWindow_v5.h",
    "RawrXDMainWindow.cpp", "RawrXDMainWindow.h",
    "MinimalWindow.cpp", "MinimalWindow.h",
    
    # Activity Bar
    "ActivityBar.cpp", "ActivityBar.h",
    "ActivityBarButton.cpp", "ActivityBarButton.h",
    
    # Debugger/Panels
    "DebuggerPanel.cpp", "DebuggerPanel.h", "DebuggerPanel.hpp",
    
    # Terminal
    "TerminalManager.cpp", "TerminalManager.h",
    "TerminalWidget.cpp", "TerminalWidget.h",
    "terminal_pool.h",
    
    # Test Explorer
    "TestExplorerPanel.cpp", "TestExplorerPanel.h", "TestExplorerPanel.hpp",
    
    # Theme Manager
    "ThemeManager.cpp", "ThemeManager.h", "ThemeManager.hpp",
    
    # Chat UI
    "chat_interface.h", "chat_workspace.h",
    "ai_chat_panel.cpp", "ai_chat_panel.hpp",
    "ai_chat_panel_manager.cpp", "ai_chat_panel_manager.hpp",
    "agent_chat_breadcrumb.hpp",
    
    # Settings UI
    "settings_dialog.cpp", "settings_dialog.h",
    "settings_dialog_visual.cpp",
    "ci_cd_settings.cpp", "ci_cd_settings.h",
    "ci_cd_settings_broken.cpp",
    
    # Panels and Widgets (UI-only)
    "discovery_dashboard.cpp", "discovery_dashboard.h", "discovery_dashboard.hpp",
    "enterprise_tools_panel.cpp", "enterprise_tools_panel.h", "enterprise_tools_panel.hpp",
    "interpretability_panel.cpp", "interpretability_panel.h",
    "interpretability_panel_enhanced.cpp", "interpretability_panel_enhanced.hpp",
    "interpretability_panel_production.cpp", "interpretability_panel_production.hpp",
    "problems_panel.cpp", "problems_panel.hpp",
    "ai_digestion_panel.cpp", "ai_digestion_panel.hpp",
    "ai_digestion_panel_impl.cpp",
    "ai_digestion_widgets.cpp",
    "layer_quant_widget.cpp", "layer_quant_widget.hpp",
    "blob_converter_panel.cpp", "blob_converter_panel.hpp",
    
    # Menu/Command UI
    "command_palette.cpp", "command_palette.hpp",
    "gui_command_menu.cpp", "gui_command_menu.hpp",
    
    # Dashboard/UI
    "metrics_dashboard.cpp", "metrics_dashboard.hpp",
    "observability_dashboard.h",
    "todo_dock.h",
    "training_progress_dock.h",
    
    # Editor UI
    "code_minimap.cpp", "code_minimap.h", "code_minimap.hpp",
    "editor_with_minimap.cpp", "editor_with_minimap.h",
    "syntax_highlighter.cpp", "syntax_highlighter.hpp",
    "multi_tab_editor.h",
    "multi_file_search.h",
    
    # Misc UI
    "hardware_backend_selector.h",
    "tokenizer_language_selector.cpp", "tokenizer_language_selector.h",
    "tokenizer_selector.cpp", "tokenizer_selector.h",
    "training_dialog.h",
    "lsp_client.h",
    "file_browser.h",
    "todo_dock.h",
    "real_time_refactoring.h",
    "real_time_refactoring.cpp",
    "real_time_refactoring.hpp",
    
    # Telemetry UI
    "EnterpriseTelemetry.h", "TelemetryWindow.h",
    
    # Qt main applications
    "main_qt.cpp", "main_qt_migrated.cpp",
    "minimal_qt_test.cpp",
    "test_qt.cpp",
    "mainwindow_integration_tests.cpp",
    "production_integration_test.cpp",
    "production_integration_example.cpp",
    "test_chat_streaming.cpp",
    "test_chat_console.cpp",
    
    # AI Panel UI
    "ai_code_assistant_panel.cpp", "ai_code_assistant_panel.h",
    "ai_code_assistant_panel_real.cpp",
    "ai_completion_provider.cpp", "ai_completion_provider.h",
    "agentic_text_edit.h",
    "ai_switcher.cpp", "ai_switcher.hpp"
)

Write-Host "=== PURE Qt GUI FILES TO DELETE ===" -ForegroundColor Red
Write-Host "Total files to delete: $($GUI_FILES.Count)"
Write-Host ""

foreach ($file in $GUI_FILES) {
    Write-Host $file
}

Write-Host ""
Write-Host "=== CORE LOGIC FILES TO CONVERT ===" -ForegroundColor Green
$CORE_FILES = @(
    "inference_engine.cpp", "inference_engine.hpp",
    "gguf_loader.cpp", "gguf_loader.hpp", "gguf_loader.h",
    "bpe_tokenizer.cpp", "bpe_tokenizer.hpp",
    "sentencepiece_tokenizer.cpp", "sentencepiece_tokenizer.hpp",
    "transformer_inference.cpp", "transformer_inference.hpp",
    "vulkan_compute.h",
    "streaming_inference.cpp", "streaming_inference.hpp",
    "streaming_inference_api.cpp", "streaming_inference_api.hpp",
    "gpu_backend.cpp", "gpu_backend.hpp",
    "health_check_server.cpp", "health_check_server.hpp",
    "gguf_server.cpp", "gguf_server.hpp",
    "model_queue.cpp", "model_queue.hpp",
    "model_monitor.cpp", "model_monitor.hpp",
    "project_manager.cpp", "project_manager.hpp",
    "backup_manager.cpp", "backup_manager.hpp"
)

Write-Host "Total core files to convert: $($CORE_FILES.Count)"
Write-Host ""

foreach ($file in $CORE_FILES) {
    Write-Host $file
}
