#include "masm_feature_manager.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// Singleton instance
static MasmFeatureManager* g_instance = nullptr;

MasmFeatureManager* MasmFeatureManager::instance() {
    if (!g_instance) {
        g_instance = new MasmFeatureManager();
    }
    return g_instance;
}

MasmFeatureManager::MasmFeatureManager(QObject* parent)
    : QObject(parent)
    , m_settings(new QSettings("RawrXD", "MasmFeatures", this))
    , m_currentPreset(PresetStandard)
{
    initializeFeatures();
    loadSettings();
}

MasmFeatureManager::~MasmFeatureManager() {
    saveSettings();
}

void MasmFeatureManager::initializeFeatures() {
    // ========================================
    // CATEGORY: RUNTIME (10 files)
    // Core memory, sync, string, event, logging primitives
    // ========================================
    registerFeature({
        "asm_memory", "Memory Manager", "Core memory allocation/deallocation primitives",
        CategoryRuntime, true, true, 256, 2, {}, "src/masm/asm_memory.asm"
    });
    registerFeature({
        "asm_sync", "Synchronization Primitives", "Mutex, semaphore, critical sections",
        CategoryRuntime, true, true, 128, 1, {"asm_memory"}, "src/masm/asm_sync.asm"
    });
    registerFeature({
        "asm_string", "String Operations", "Fast string copy, compare, search",
        CategoryRuntime, true, false, 64, 1, {}, "src/masm/asm_string.asm"
    });
    registerFeature({
        "asm_events", "Event System", "Event queue and dispatch mechanism",
        CategoryRuntime, true, true, 512, 3, {"asm_memory", "asm_sync"}, "src/masm/asm_events.asm"
    });
    registerFeature({
        "asm_log", "Logging System", "Fast console/file logging",
        CategoryRuntime, true, false, 256, 2, {"asm_string"}, "src/masm/asm_log.asm"
    });
    registerFeature({
        "console_log_simple", "Simple Console Log", "Lightweight console output",
        CategoryRuntime, true, false, 32, 1, {}, "src/masm/final-ide/console_log_simple.asm"
    });
    
    // ========================================
    // CATEGORY: HOTPATCH (15 files)
    // Real-time model editing, byte manipulation, server hotpatching
    // ========================================
    registerFeature({
        "model_memory_hotpatch", "Model Memory Hotpatch", "Direct RAM tensor editing with OS memory protection",
        CategoryHotpatch, true, false, 1024, 5, {"asm_memory"}, "src/qtapp/model_memory_hotpatch.cpp"
    });
    registerFeature({
        "byte_level_hotpatcher", "Byte-Level Hotpatcher", "Precision GGUF binary file manipulation",
        CategoryHotpatch, true, false, 512, 3, {}, "src/qtapp/byte_level_hotpatcher.cpp"
    });
    registerFeature({
        "gguf_server_hotpatch", "GGUF Server Hotpatch", "Request/response transformation for inference servers",
        CategoryHotpatch, true, false, 768, 4, {}, "src/qtapp/gguf_server_hotpatch.cpp"
    });
    registerFeature({
        "unified_hotpatch_manager", "Unified Hotpatch Manager", "Coordinates all three hotpatch layers",
        CategoryHotpatch, true, true, 256, 2, {"model_memory_hotpatch", "byte_level_hotpatcher", "gguf_server_hotpatch"}, "src/qtapp/unified_hotpatch_manager.cpp"
    });
    registerFeature({
        "proxy_hotpatcher", "Proxy Hotpatcher", "Proxy-layer byte manipulation for agent output correction",
        CategoryHotpatch, true, false, 384, 3, {}, "src/qtapp/proxy_hotpatcher.cpp"
    });
    registerFeature({
        "hotpatch_coordinator", "Hotpatch Coordinator", "MASM hotpatch coordination layer",
        CategoryHotpatch, false, false, 128, 2, {}, "src/masm/final-ide/hotpatch_coordinator.asm"
    });
    registerFeature({
        "hotpatch_system", "Hotpatch System Core", "Base hotpatch infrastructure",
        CategoryHotpatch, false, true, 256, 3, {}, "src/masm/final-ide/hotpatch_system.asm"
    });
    
    // ========================================
    // CATEGORY: AGENT (21 files)
    // Autonomous coding agents, orchestration, planning, execution
    // ========================================
    registerFeature({
        "agent_orchestrator_main", "Agent Orchestrator", "Main agent coordination system",
        CategoryAgent, true, true, 2048, 8, {"asm_events", "asm_sync"}, "src/masm/final-ide/agent_orchestrator_main.asm"
    });
    registerFeature({
        "agent_planner", "Agent Planner", "Task planning and decomposition",
        CategoryAgent, true, false, 512, 4, {"agent_orchestrator_main"}, "src/masm/final-ide/agent_planner.asm"
    });
    registerFeature({
        "agent_executor", "Agent Executor", "Task execution engine",
        CategoryAgent, true, false, 1024, 6, {"agent_planner"}, "src/masm/final-ide/agent_executor.asm"
    });
    registerFeature({
        "agent_action_executor", "Agent Action Executor", "Low-level action execution",
        CategoryAgent, true, false, 768, 5, {}, "src/masm/final-ide/agent_action_executor.asm"
    });
    registerFeature({
        "agent_utility_agents", "Utility Agents", "Helper agents for common tasks",
        CategoryAgent, false, false, 384, 3, {}, "src/masm/final-ide/agent_utility_agents.asm"
    });
    registerFeature({
        "agent_meta_learn", "Meta Learning Agent", "Self-improvement system",
        CategoryAgent, false, false, 512, 4, {}, "src/masm/final-ide/agent_meta_learn.asm"
    });
    registerFeature({
        "agent_auto_bootstrap", "Auto Bootstrap Agent", "Self-initialization agent",
        CategoryAgent, false, false, 256, 2, {}, "src/masm/final-ide/agent_auto_bootstrap.asm"
    });
    registerFeature({
        "agent_self_patch", "Self-Patching Agent", "Code self-modification",
        CategoryAgent, false, true, 1024, 7, {}, "src/masm/final-ide/agent_self_patch.asm"
    });
    registerFeature({
        "agent_hot_reload_rollback", "Hot Reload & Rollback", "Live code reload with rollback support",
        CategoryAgent, false, false, 384, 3, {}, "src/masm/final-ide/agent_hot_reload_rollback.asm"
    });
    registerFeature({
        "agent_learning_system", "Agent Learning System", "Machine learning for agents",
        CategoryAgent, false, false, 768, 5, {}, "src/masm/final-ide/agent_learning_system.asm"
    });
    registerFeature({
        "agent_advanced_workflows", "Advanced Workflows", "Complex multi-step workflows",
        CategoryAgent, false, false, 512, 4, {"agent_planner"}, "src/masm/final-ide/agent_advanced_workflows.asm"
    });
    registerFeature({
        "agent_beyond_enterprise_orchestrator", "Enterprise Orchestrator", "Enterprise-scale agent coordination",
        CategoryAgent, false, true, 2048, 10, {}, "src/masm/final-ide/agent_beyond_enterprise_orchestrator.asm"
    });
    registerFeature({
        "agent_chat_enhanced", "Enhanced Agent Chat", "Advanced chat interface for agents",
        CategoryAgent, true, false, 1024, 5, {}, "src/masm/final-ide/agent_chat_enhanced.asm"
    });
    registerFeature({
        "agent_chat_integration", "Agent Chat Integration", "Chat system integration layer",
        CategoryAgent, true, false, 512, 3, {}, "src/masm/final-ide/agent_chat_integration.asm"
    });
    registerFeature({
        "agent_chat_modes", "Agent Chat Modes", "Multiple chat interaction modes",
        CategoryAgent, true, false, 256, 2, {}, "src/masm/final-ide/agent_chat_modes.asm"
    });
    registerFeature({
        "agent_chat_hotpatch_bridge", "Chat Hotpatch Bridge", "Connect chat to hotpatch system",
        CategoryAgent, true, false, 384, 3, {"unified_hotpatch_manager"}, "src/masm/final-ide/agent_chat_hotpatch_bridge.asm"
    });
    registerFeature({
        "agent_response_enhancer", "Response Enhancer", "Improve agent response quality",
        CategoryAgent, false, false, 256, 2, {}, "src/masm/final-ide/agent_response_enhancer.asm"
    });
    registerFeature({
        "agent_ide_bridge", "IDE Bridge", "Connect agents to IDE functionality",
        CategoryAgent, true, true, 512, 4, {}, "src/masm/final-ide/agent_ide_bridge.asm"
    });
    
    // ========================================
    // CATEGORY: AGENTIC (9 files)
    // Failure detection, puppeteer, copilot bridge
    // ========================================
    registerFeature({
        "agentic_failure_detector", "Failure Detector", "Pattern-based failure detection (refusal, hallucination, timeout)",
        CategoryAgentic, true, false, 768, 5, {}, "src/agent/agentic_failure_detector.cpp"
    });
    registerFeature({
        "agentic_puppeteer", "Agentic Puppeteer", "Automatic response correction for detected failures",
        CategoryAgentic, true, false, 512, 4, {"agentic_failure_detector"}, "src/agent/agentic_puppeteer.cpp"
    });
    registerFeature({
        "agentic_copilot_bridge", "Copilot Bridge", "Integration with GitHub Copilot",
        CategoryAgentic, false, false, 384, 3, {}, "src/agent/agentic_copilot_bridge.cpp"
    });
    registerFeature({
        "agentic_masm_system", "MASM Agentic System", "Core agentic system in pure MASM",
        CategoryAgentic, false, true, 1024, 6, {}, "src/masm/final-ide/agentic_masm_system.asm"
    });
    registerFeature({
        "agentic_inference_stream", "Inference Stream", "Streaming inference with agentic control",
        CategoryAgentic, true, false, 512, 4, {}, "src/masm/final-ide/agentic_inference_stream.asm"
    });
    registerFeature({
        "agentic_failure_recovery", "Failure Recovery", "Automatic recovery from failures",
        CategoryAgentic, true, false, 384, 3, {"agentic_failure_detector"}, "src/masm/final-ide/agentic_failure_recovery.asm"
    });
    registerFeature({
        "agentic_engine", "Agentic Engine Core", "Main agentic engine",
        CategoryAgentic, true, true, 2048, 10, {}, "src/masm/final-ide/agentic_engine.asm"
    });
    registerFeature({
        "agentic_masm", "Agentic MASM Runtime", "MASM runtime for agentic features",
        CategoryAgentic, true, true, 512, 4, {}, "src/masm/final-ide/agentic_masm.asm"
    });
    
    // ========================================
    // CATEGORY: UI (4 files)
    // Phase 1-3 UI implementations
    // ========================================
    registerFeature({
        "ui_phase1_implementations", "UI Phase 1", "Win32 Window Framework + Menu System (2,100 lines)",
        CategoryUI, true, true, 1536, 7, {"asm_memory", "asm_events"}, "src/masm/final-ide/ui_phase1_implementations.asm"
    });
    registerFeature({
        "ui_masm", "UI MASM Core", "Core UI framework in MASM (123 KB, largest file)",
        CategoryUI, true, true, 3072, 12, {"ui_phase1_implementations"}, "src/masm/final-ide/ui_masm.asm"
    });
    registerFeature({
        "ui_helpers_masm", "UI Helpers", "UI helper functions",
        CategoryUI, true, false, 256, 2, {}, "src/masm/final-ide/ui_helpers_masm.asm"
    });
    registerFeature({
        "ui_system", "UI System Core", "Core UI system infrastructure",
        CategoryUI, true, true, 512, 4, {}, "src/masm/final-ide/ui_system.asm"
    });
    
    // ========================================
    // CATEGORY: CHAT (3 files)
    // ========================================
    registerFeature({
        "chat_persistence_phase2", "Chat Persistence", "Save/load chat history (Phase 2)",
        CategoryChat, true, false, 512, 3, {"asm_memory"}, "src/masm/final-ide/chat_persistence_phase2.asm"
    });
    registerFeature({
        "chat_persistence", "Chat Persistence Core", "Core persistence engine",
        CategoryChat, true, false, 384, 2, {}, "src/masm/final-ide/chat_persistence.asm"
    });
    
    // ========================================
    // CATEGORY: ML (42 files - largest category!)
    // ========================================
    registerFeature({
        "masm_ml_training_studio", "ML Training Studio", "Full training environment in MASM",
        CategoryML, false, true, 8192, 25, {}, "src/masm/final-ide/masm_ml_training_studio.asm"
    });
    registerFeature({
        "masm_tensor_debugger", "Tensor Debugger", "Debug tensor operations",
        CategoryML, false, false, 2048, 8, {}, "src/masm/final-ide/masm_tensor_debugger.asm"
    });
    registerFeature({
        "masm_notebook_interface", "Notebook Interface", "Jupyter-like notebook",
        CategoryML, false, false, 1024, 5, {}, "src/masm/final-ide/masm_notebook_interface.asm"
    });
    registerFeature({
        "masm_ml_visualization", "ML Visualization", "Visualize training metrics",
        CategoryML, false, false, 1536, 7, {}, "src/masm/final-ide/masm_ml_visualization.asm"
    });
    registerFeature({
        "masm_inference_engine", "Inference Engine", "MASM inference engine",
        CategoryML, true, false, 2048, 10, {}, "src/masm/final-ide/masm_inference_engine.asm"
    });
    registerFeature({
        "masm_tokenizer", "Tokenizer", "Text tokenization in MASM",
        CategoryML, true, false, 512, 3, {}, "src/masm/final-ide/masm_tokenizer.asm"
    });
    registerFeature({
        "masm_quant_utils", "Quantization Utilities", "Model quantization tools",
        CategoryML, false, false, 768, 4, {}, "src/masm/final-ide/masm_quant_utils.asm"
    });
    
    // ========================================
    // CATEGORY: GGUF (3 files)
    // ========================================
    registerFeature({
        "gguf_loader_complete", "GGUF Loader", "Complete GGUF model loader",
        CategoryGGUF, true, false, 1024, 5, {}, "src/masm/final-ide/gguf_loader_complete.asm"
    });
    registerFeature({
        "gguf_metadata_parser", "GGUF Metadata Parser", "Parse GGUF metadata",
        CategoryGGUF, true, false, 384, 2, {}, "src/masm/final-ide/gguf_metadata_parser.asm"
    });
    
    // ========================================
    // CATEGORY: GPU (2 files)
    // ========================================
    registerFeature({
        "masm_gpu_backend", "GPU Backend", "GPU abstraction layer (CUDA, Vulkan, ROCm)",
        CategoryGPU, true, false, 2048, 10, {}, "src/masm/final-ide/masm_gpu_backend.asm"
    });
    registerFeature({
        "masm_gpu_backend_clean", "GPU Backend (Clean)", "Cleaned GPU backend implementation",
        CategoryGPU, false, false, 1536, 8, {}, "src/masm/final-ide/masm_gpu_backend_clean.asm"
    });
    
    // ========================================
    // CATEGORY: ORCHESTRATION (4 files)
    // ========================================
    registerFeature({
        "ai_orchestration_coordinator", "AI Orchestration Coordinator", "Coordinate AI tasks",
        CategoryOrchestration, true, false, 1024, 6, {}, "src/masm/final-ide/ai_orchestration_coordinator.asm"
    });
    registerFeature({
        "ai_orchestration_glue", "AI Orchestration Glue", "C++/MASM bridge layer",
        CategoryOrchestration, true, false, 256, 2, {}, "src/masm/final-ide/ai_orchestration_glue.asm"
    });
    registerFeature({
        "autonomous_task_executor", "Autonomous Task Executor", "Execute tasks autonomously",
        CategoryOrchestration, true, false, 768, 5, {}, "src/masm/final-ide/autonomous_task_executor.asm"
    });
    registerFeature({
        "autonomous_task_executor_clean", "Autonomous Task Executor (Clean)", "Cleaned implementation",
        CategoryOrchestration, false, false, 512, 4, {}, "src/masm/final-ide/autonomous_task_executor_clean.asm"
    });
    
    // ========================================
    // CATEGORY: OUTPUT/LOGGING (5 files)
    // ========================================
    registerFeature({
        "output_pane_logger", "Output Pane Logger", "Log to output pane",
        CategoryOutput, true, false, 512, 3, {}, "src/masm/final-ide/output_pane_logger.asm"
    });
    registerFeature({
        "output_pane_filter", "Output Pane Filter", "Filter output messages",
        CategoryOutput, false, false, 128, 1, {}, "src/masm/final-ide/output_pane_filter.asm"
    });
    registerFeature({
        "output_pane_search", "Output Pane Search", "Search in output",
        CategoryOutput, false, false, 256, 2, {}, "src/masm/final-ide/output_pane_search.asm"
    });
    
    // ========================================
    // CATEGORY: PANE (6 files)
    // ========================================
    registerFeature({
        "pane_manager", "Pane Manager", "Manage IDE panes",
        CategoryPane, true, true, 768, 4, {}, "src/masm/final-ide/pane_manager.asm"
    });
    registerFeature({
        "dynamic_pane_manager", "Dynamic Pane Manager", "Dynamic pane creation/destruction",
        CategoryPane, false, false, 512, 3, {"pane_manager"}, "src/masm/final-ide/dynamic_pane_manager.asm"
    });
    registerFeature({
        "pane_integration_system", "Pane Integration System", "Integrate panes with IDE",
        CategoryPane, true, false, 384, 2, {}, "src/masm/final-ide/pane_integration_system.asm"
    });
    
    // ========================================
    // CATEGORY: MENU (2 files)
    // ========================================
    registerFeature({
        "menu_system", "Menu System", "Menu bar, context menus, keyboard shortcuts",
        CategoryMenu, true, true, 1024, 5, {}, "src/masm/final-ide/menu_system.asm"
    });
    registerFeature({
        "menu_hooks", "Menu Hooks", "Custom menu handlers",
        CategoryMenu, false, false, 256, 2, {}, "src/masm/final-ide/menu_hooks.asm"
    });
    
    // ========================================
    // CATEGORY: FILE (3 files)
    // ========================================
    registerFeature({
        "file_manager", "File Manager", "File operations",
        CategoryFile, true, false, 512, 3, {}, "src/masm/final-ide/file_manager.asm"
    });
    registerFeature({
        "file_tree_driver", "File Tree Driver", "File tree UI component",
        CategoryFile, true, false, 768, 4, {}, "src/masm/final-ide/file_tree_driver.asm"
    });
    registerFeature({
        "file_tree_context_menu", "File Tree Context Menu", "Right-click menu for files",
        CategoryFile, false, false, 256, 2, {}, "src/masm/final-ide/file_tree_context_menu.asm"
    });
    
    // ========================================
    // CATEGORY: TERMINAL (1 file)
    // ========================================
    registerFeature({
        "terminal_system", "Terminal System", "Integrated terminal",
        CategoryTerminal, true, true, 2048, 8, {}, "src/masm/final-ide/terminal_system.asm"
    });
    
    // ========================================
    // CATEGORY: THREADING (1 file)
    // ========================================
    registerFeature({
        "threading_system", "Threading System", "Thread creation, pools, synchronization",
        CategoryThreading, true, true, 1024, 6, {"asm_sync"}, "src/masm/final-ide/threading_system.asm"
    });
    
    // ========================================
    // CATEGORY: SIGNAL/SLOT (1 file)
    // ========================================
    registerFeature({
        "signal_slot_system", "Signal/Slot System", "Qt-like signal/slot mechanism",
        CategorySignalSlot, true, true, 768, 4, {"asm_events"}, "src/masm/final-ide/signal_slot_system.asm"
    });
    
    // ========================================
    // CATEGORY: SECURITY (1 file)
    // ========================================
    registerFeature({
        "masm_security_manager", "Security Manager", "Security policies and sandboxing",
        CategorySecurity, false, true, 512, 3, {}, "src/masm/final-ide/masm_security_manager.asm"
    });
    
    // ========================================
    // CATEGORY: TELEMETRY (1 file)
    // ========================================
    registerFeature({
        "telemetry_system", "Telemetry System", "Usage analytics",
        CategoryTelemetry, false, false, 256, 2, {}, "src/masm/final-ide/telemetry_system.asm"
    });
    
    // ========================================
    // CATEGORY: WEBVIEW (1 file)
    // ========================================
    registerFeature({
        "webview_integration", "WebView Integration", "Embed web content",
        CategoryWebview, false, false, 1024, 5, {}, "src/masm/final-ide/webview_integration.asm"
    });
    
    // ========================================
    // CATEGORY: SESSION (1 file)
    // ========================================
    registerFeature({
        "session_manager", "Session Manager", "Save/restore IDE sessions",
        CategorySession, true, false, 384, 2, {}, "src/masm/final-ide/session_manager.asm"
    });
    
    // ========================================
    // CATEGORY: KEYBOARD (1 file)
    // ========================================
    registerFeature({
        "keyboard_shortcuts", "Keyboard Shortcuts", "Customizable keybindings",
        CategoryKeyboard, true, false, 256, 2, {}, "src/masm/final-ide/keyboard_shortcuts.asm"
    });
    
    // ========================================
    // CATEGORY: HTTP (1 file)
    // ========================================
    registerFeature({
        "http_client", "HTTP Client", "Make HTTP requests",
        CategoryHTTP, true, false, 512, 3, {}, "src/masm/final-ide/http_client.asm"
    });
    
    // ========================================
    // CATEGORY: JSON (1 file)
    // ========================================
    registerFeature({
        "json_parser", "JSON Parser", "Parse/generate JSON",
        CategoryJSON, true, false, 384, 2, {}, "src/masm/final-ide/json_parser.asm"
    });
    
    // ========================================
    // CATEGORY: OLLAMA (2 files)
    // ========================================
    registerFeature({
        "ollama_bridge", "Ollama Bridge", "Connect to Ollama API",
        CategoryOllama, true, false, 512, 3, {"http_client"}, "src/masm/final-ide/ollama_bridge.asm"
    });
    registerFeature({
        "ollama_pull", "Ollama Pull", "Download models from Ollama",
        CategoryOllama, false, false, 768, 4, {"ollama_bridge"}, "src/masm/final-ide/ollama_pull.asm"
    });
    
    // ========================================
    // CATEGORY: GIT (1 file)
    // ========================================
    registerFeature({
        "git_integration", "Git Integration", "Git operations (commit, push, pull, diff)",
        CategoryGit, false, false, 1024, 5, {}, "src/masm/final-ide/git_integration.asm"
    });
    
    // ========================================
    // CATEGORY: PLUGIN (1 file)
    // ========================================
    registerFeature({
        "plugin_loader", "Plugin Loader", "Load dynamic plugins",
        CategoryPlugin, false, true, 512, 3, {}, "src/masm/final-ide/plugin_loader.asm"
    });
    
    // ========================================
    // CATEGORY: EXPERIMENTAL (10+ files)
    // ========================================
    registerFeature({
        "rawr1024_dual_engine", "Rawr1024 Dual Engine", "Dual inference engine (132 KB, 2nd largest file)",
        CategoryExperimental, false, false, 4096, 15, {}, "src/masm/final-ide/rawr1024_dual_engine.asm"
    });
    registerFeature({
        "gui_designer_agent", "GUI Designer Agent", "Visual GUI builder (114 KB, 3rd largest file)",
        CategoryExperimental, false, false, 3072, 12, {}, "src/masm/final-ide/gui_designer_agent.asm"
    });
    
    // ========================================
    // CATEGORY: TEST (5 files)
    // ========================================
    registerFeature({
        "test_core_functions", "Core Function Tests", "Unit tests for core functions",
        CategoryTest, false, false, 256, 2, {}, "src/masm/final-ide/test_core_functions.asm"
    });
    registerFeature({
        "test_gguf_loader", "GGUF Loader Tests", "Test GGUF loading",
        CategoryTest, false, false, 128, 1, {}, "src/masm/final-ide/test_gguf_loader.asm"
    });
    
    qDebug() << "Initialized" << m_features.size() << "MASM features across" 
             << "32 categories (212 total files available)";
}

void MasmFeatureManager::registerFeature(const FeatureInfo& info) {
    m_features[info.name] = info;
    m_enabledStates[info.name] = info.enabledByDefault;
    
    // Initialize performance metrics
    m_performanceData[info.name] = {0, 0, 0, 0.0};
}

// ========================================
// FEATURE QUERY METHODS
// ========================================

QList<MasmFeatureManager::FeatureInfo> MasmFeatureManager::getAllFeatures() const {
    return m_features.values();
}

QList<MasmFeatureManager::FeatureInfo> MasmFeatureManager::getFeaturesByCategory(Category category) const {
    QList<FeatureInfo> result;
    for (const auto& feature : m_features) {
        if (feature.category == category) {
            result.append(feature);
        }
    }
    return result;
}

bool MasmFeatureManager::isFeatureEnabled(const QString& featureName) const {
    return m_enabledStates.value(featureName, false);
}

void MasmFeatureManager::setFeatureEnabled(const QString& featureName, bool enabled) {
    if (!m_features.contains(featureName)) {
        qWarning() << "Attempted to set unknown feature:" << featureName;
        return;
    }
    
    if (m_enabledStates.value(featureName) == enabled) {
        return; // No change
    }
    
    m_enabledStates[featureName] = enabled;
    emit featureEnabledChanged(featureName, enabled);
    
    // Check if preset is still valid
    if (m_currentPreset != PresetCustom) {
        m_currentPreset = PresetCustom;
        emit presetChanged(PresetCustom);
    }
}

void MasmFeatureManager::resetToDefaults() {
    for (auto it = m_features.constBegin(); it != m_features.constEnd(); ++it) {
        m_enabledStates[it.key()] = it.value().enabledByDefault;
    }
    m_currentPreset = PresetStandard;
    emit presetChanged(PresetStandard);
}

// ========================================
// CATEGORY-LEVEL CONTROL
// ========================================

void MasmFeatureManager::setCategoryEnabled(Category category, bool enabled) {
    for (auto it = m_features.constBegin(); it != m_features.constEnd(); ++it) {
        if (it.value().category == category) {
            setFeatureEnabled(it.key(), enabled);
        }
    }
    emit categoryEnabledChanged(category, enabled);
}

bool MasmFeatureManager::isCategoryEnabled(Category category) const {
    // A category is "enabled" if ANY of its features are enabled
    for (const auto& feature : m_features) {
        if (feature.category == category && m_enabledStates.value(feature.name, false)) {
            return true;
        }
    }
    return false;
}

void MasmFeatureManager::enableCategory(Category category) {
    setCategoryEnabled(category, true);
}

void MasmFeatureManager::disableCategory(Category category) {
    setCategoryEnabled(category, false);
}

// ========================================
// BULK OPERATIONS
// ========================================

void MasmFeatureManager::enableAll() {
    for (const QString& featureName : m_features.keys()) {
        m_enabledStates[featureName] = true;
        emit featureEnabledChanged(featureName, true);
    }
    m_currentPreset = PresetMaximum;
    emit presetChanged(PresetMaximum);
}

void MasmFeatureManager::disableAll() {
    for (const QString& featureName : m_features.keys()) {
        m_enabledStates[featureName] = false;
        emit featureEnabledChanged(featureName, false);
    }
    m_currentPreset = PresetCustom;
    emit presetChanged(PresetCustom);
}

// ========================================
// PERFORMANCE PROFILING
// ========================================

MasmFeatureManager::PerformanceMetrics MasmFeatureManager::getPerformanceMetrics(const QString& featureName) const {
    return m_performanceData.value(featureName, {0, 0, 0, 0.0});
}

void MasmFeatureManager::resetPerformanceMetrics() {
    for (auto it = m_performanceData.begin(); it != m_performanceData.end(); ++it) {
        it.value() = {0, 0, 0, 0.0};
    }
}

// ========================================
// HOT-RELOAD SUPPORT
// ========================================

bool MasmFeatureManager::canHotReload(const QString& featureName) const {
    if (!m_features.contains(featureName)) {
        return false;
    }
    return !m_features[featureName].requiresRestart;
}

bool MasmFeatureManager::hotReload(const QString& featureName) {
    if (!canHotReload(featureName)) {
        qWarning() << "Feature" << featureName << "cannot be hot-reloaded (requires restart)";
        emit hotReloadCompleted(featureName, false);
        return false;
    }
    
    // TODO: Implement actual hot-reload mechanism per feature type
    // For now, just toggle the feature state
    bool currentState = isFeatureEnabled(featureName);
    setFeatureEnabled(featureName, !currentState);
    setFeatureEnabled(featureName, currentState);
    
    qDebug() << "Hot-reloaded feature:" << featureName;
    emit hotReloadCompleted(featureName, true);
    return true;
}

// ========================================
// SETTINGS PERSISTENCE
// ========================================

void MasmFeatureManager::saveSettings() {
    m_settings->beginGroup("Features");
    for (auto it = m_enabledStates.constBegin(); it != m_enabledStates.constEnd(); ++it) {
        m_settings->setValue(it.key(), it.value());
    }
    m_settings->endGroup();
    
    m_settings->setValue("Preset", static_cast<int>(m_currentPreset));
    m_settings->sync();
}

void MasmFeatureManager::loadSettings() {
    m_settings->beginGroup("Features");
    QStringList keys = m_settings->allKeys();
    for (const QString& key : keys) {
        if (m_features.contains(key)) {
            m_enabledStates[key] = m_settings->value(key).toBool();
        }
    }
    m_settings->endGroup();
    
    int presetValue = m_settings->value("Preset", PresetStandard).toInt();
    m_currentPreset = static_cast<Preset>(presetValue);
}

// ========================================
// EXPORT/IMPORT CONFIGURATIONS
// ========================================

bool MasmFeatureManager::exportConfig(const QString& filePath) const {
    QJsonObject root;
    root["version"] = "1.0";
    root["preset"] = QString::number(static_cast<int>(m_currentPreset));
    
    QJsonObject features;
    for (auto it = m_enabledStates.constBegin(); it != m_enabledStates.constEnd(); ++it) {
        features[it.key()] = it.value();
    }
    root["features"] = features;
    
    QJsonObject metadata;
    metadata["exported"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["totalFeatures"] = m_features.size();
    metadata["enabledCount"] = std::count_if(m_enabledStates.constBegin(), m_enabledStates.constEnd(),
                                              [](bool enabled) { return enabled; });
    root["metadata"] = metadata;
    
    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for export:" << filePath;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    qDebug() << "Exported configuration to:" << filePath;
    return true;
}

bool MasmFeatureManager::importConfig(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for import:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format in:" << filePath;
        return false;
    }
    
    QJsonObject root = doc.object();
    QString version = root["version"].toString();
    if (version != "1.0") {
        qWarning() << "Unsupported config version:" << version;
        return false;
    }
    
    QJsonObject features = root["features"].toObject();
    for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
        if (m_features.contains(it.key())) {
            m_enabledStates[it.key()] = it.value().toBool();
            emit featureEnabledChanged(it.key(), it.value().toBool());
        }
    }
    
    int presetValue = root["preset"].toString().toInt();
    m_currentPreset = static_cast<Preset>(presetValue);
    emit presetChanged(m_currentPreset);
    
    qDebug() << "Imported configuration from:" << filePath;
    return true;
}

// ========================================
// PRESET MANAGEMENT
// ========================================

void MasmFeatureManager::applyPreset(Preset preset) {
    m_currentPreset = preset;
    
    switch (preset) {
        case PresetMinimal: {
            // Only critical features (Runtime + Hotpatch)
            disableAll();
            enableCategory(CategoryRuntime);
            enableCategory(CategoryHotpatch);
            break;
        }
        
        case PresetStandard: {
            // Balanced (Core + UI + Agent)
            disableAll();
            enableCategory(CategoryRuntime);
            enableCategory(CategoryHotpatch);
            enableCategory(CategoryAgent);
            enableCategory(CategoryAgentic);
            enableCategory(CategoryUI);
            enableCategory(CategoryChat);
            enableCategory(CategoryGPU);
            enableCategory(CategoryOrchestration);
            enableCategory(CategoryFile);
            enableCategory(CategoryMenu);
            enableCategory(CategoryPane);
            enableCategory(CategoryTerminal);
            enableCategory(CategoryThreading);
            enableCategory(CategorySignalSlot);
            break;
        }
        
        case PresetPerformance: {
            // Optimized for speed (no telemetry, minimal logging)
            applyPreset(PresetStandard);
            disableCategory(CategoryTelemetry);
            setFeatureEnabled("asm_log", false);
            setFeatureEnabled("output_pane_logger", false);
            break;
        }
        
        case PresetDevelopment: {
            // All dev tools
            applyPreset(PresetStandard);
            enableCategory(CategoryTest);
            enableCategory(CategoryTelemetry);
            enableCategory(CategoryML);
            enableCategory(CategoryGGUF);
            enableCategory(CategoryGit);
            enableCategory(CategoryPlugin);
            break;
        }
        
        case PresetMaximum: {
            // Everything enabled
            enableAll();
            break;
        }
        
        case PresetCustom: {
            // User-defined, no changes
            break;
        }
    }
    
    emit presetChanged(preset);
    qDebug() << "Applied preset:" << static_cast<int>(preset);
}

MasmFeatureManager::Preset MasmFeatureManager::getCurrentPreset() const {
    return m_currentPreset;
}

// ========================================
// HELPER METHODS
// ========================================

QString MasmFeatureManager::categoryToString(Category category) const {
    switch (category) {
        case CategoryRuntime: return "Runtime";
        case CategoryHotpatch: return "Hotpatch";
        case CategoryAgent: return "Agent";
        case CategoryAgentic: return "Agentic";
        case CategoryUI: return "UI";
        case CategoryChat: return "Chat";
        case CategoryML: return "ML";
        case CategoryGGUF: return "GGUF";
        case CategoryGPU: return "GPU";
        case CategoryOrchestration: return "Orchestration";
        case CategoryOutput: return "Output";
        case CategoryPane: return "Pane";
        case CategoryPlugin: return "Plugin";
        case CategoryMenu: return "Menu";
        case CategoryFile: return "File";
        case CategoryTerminal: return "Terminal";
        case CategoryGit: return "Git";
        case CategoryTelemetry: return "Telemetry";
        case CategorySecurity: return "Security";
        case CategoryThreading: return "Threading";
        case CategorySignalSlot: return "SignalSlot";
        case CategoryKeyboard: return "Keyboard";
        case CategoryWebview: return "Webview";
        case CategorySession: return "Session";
        case CategoryCompression: return "Compression";
        case CategoryHTTP: return "HTTP";
        case CategoryJSON: return "JSON";
        case CategoryOllama: return "Ollama";
        case CategoryAdvanced: return "Advanced";
        case CategoryStubs: return "Stubs";
        case CategoryTest: return "Test";
        case CategoryExperimental: return "Experimental";
        default: return "Unknown";
    }
}

MasmFeatureManager::Category MasmFeatureManager::stringToCategory(const QString& categoryStr) const {
    if (categoryStr == "Runtime") return CategoryRuntime;
    if (categoryStr == "Hotpatch") return CategoryHotpatch;
    if (categoryStr == "Agent") return CategoryAgent;
    if (categoryStr == "Agentic") return CategoryAgentic;
    if (categoryStr == "UI") return CategoryUI;
    if (categoryStr == "Chat") return CategoryChat;
    if (categoryStr == "ML") return CategoryML;
    if (categoryStr == "GGUF") return CategoryGGUF;
    if (categoryStr == "GPU") return CategoryGPU;
    if (categoryStr == "Orchestration") return CategoryOrchestration;
    if (categoryStr == "Output") return CategoryOutput;
    if (categoryStr == "Pane") return CategoryPane;
    if (categoryStr == "Plugin") return CategoryPlugin;
    if (categoryStr == "Menu") return CategoryMenu;
    if (categoryStr == "File") return CategoryFile;
    if (categoryStr == "Terminal") return CategoryTerminal;
    if (categoryStr == "Git") return CategoryGit;
    if (categoryStr == "Telemetry") return CategoryTelemetry;
    if (categoryStr == "Security") return CategorySecurity;
    if (categoryStr == "Threading") return CategoryThreading;
    if (categoryStr == "SignalSlot") return CategorySignalSlot;
    if (categoryStr == "Keyboard") return CategoryKeyboard;
    if (categoryStr == "Webview") return CategoryWebview;
    if (categoryStr == "Session") return CategorySession;
    if (categoryStr == "Compression") return CategoryCompression;
    if (categoryStr == "HTTP") return CategoryHTTP;
    if (categoryStr == "JSON") return CategoryJSON;
    if (categoryStr == "Ollama") return CategoryOllama;
    if (categoryStr == "Advanced") return CategoryAdvanced;
    if (categoryStr == "Stubs") return CategoryStubs;
    if (categoryStr == "Test") return CategoryTest;
    if (categoryStr == "Experimental") return CategoryExperimental;
    return CategoryRuntime; // Default fallback
}
