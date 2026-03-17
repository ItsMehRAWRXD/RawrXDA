#pragma once

#include <QObject>
#include <QSettings>
#include <QHash>
#include <QString>
#include <QVariant>

/**
 * @brief MASM Feature Manager - Runtime toggle system for 212+ MASM components
 * 
 * Manages feature flags for all Pure MASM x64 components (212 files across 50+ categories).
 * Each feature can be enabled/disabled at runtime without recompilation.
 * Settings persist across application restarts via QSettings.
 * 
 * Architecture:
 * - Category-based organization (agent, agentic, hotpatch, UI, ML, etc.)
 * - Per-file granular control (each .asm file is a toggleable feature)
 * - Performance profiling integration (tracks CPU/memory impact per feature)
 * - Hot-reload support (apply changes without restart where possible)
 * 
 * Usage:
 *   MasmFeatureManager* mgr = MasmFeatureManager::instance();
 *   mgr->setFeatureEnabled("hotpatch.model_memory_hotpatch", true);
 *   bool enabled = mgr->isFeatureEnabled("agent.orchestrator");
 */
class MasmFeatureManager : public QObject {
    Q_OBJECT

public:
    // Singleton accessor
    static MasmFeatureManager* instance();

    // Feature categories (50+ categories from 212 MASM files)
    enum Category {
        // Core Runtime (10 files: asm_memory, asm_sync, asm_string, asm_events, asm_log, etc.)
        CategoryRuntime,
        
        // Hotpatch System (15 files: model_memory_hotpatch, byte_level_hotpatcher, proxy_hotpatcher, etc.)
        CategoryHotpatch,
        
        // Agent Systems (21 files: agent_orchestrator, agent_planner, agent_executor, etc.)
        CategoryAgent,
        
        // Agentic AI (9 files: agentic_failure_detector, agentic_puppeteer, agentic_copilot_bridge, etc.)
        CategoryAgentic,
        
        // UI Systems (4 files: ui_phase1, ui_masm, ui_helpers, ui_system)
        CategoryUI,
        
        // Chat Systems (3 files: chat_persistence, agent_chat_modes, agent_chat_enhanced)
        CategoryChat,
        
        // ML/Training (42 files: masm_ml_training_studio, masm_tensor_debugger, masm_notebook, etc.)
        CategoryML,
        
        // GGUF/Model Loading (3 files: gguf_loader, gguf_metadata_parser, gguf_server_hotpatch)
        CategoryGGUF,
        
        // GPU Backend (2 files: masm_gpu_backend, masm_gpu_backend_clean)
        CategoryGPU,
        
        // Orchestration (4 files: ai_orchestration_coordinator, autonomous_task_executor, etc.)
        CategoryOrchestration,
        
        // Output/Logging (5 files: output_pane_logger, output_pane_filter, output_pane_search, etc.)
        CategoryOutput,
        
        // Pane/Layout (6 files: pane_manager, pane_integration, dynamic_pane_manager, etc.)
        CategoryPane,
        
        // Plugin System (1 file: plugin_loader)
        CategoryPlugin,
        
        // Menu System (2 files: menu_system, menu_hooks)
        CategoryMenu,
        
        // File Management (3 files: file_manager, file_tree_driver, file_tree_context_menu)
        CategoryFile,
        
        // Terminal (1 file: terminal_system)
        CategoryTerminal,
        
        // Git Integration (1 file: git_integration)
        CategoryGit,
        
        // Telemetry (1 file: telemetry_system)
        CategoryTelemetry,
        
        // Security (1 file: masm_security_manager)
        CategorySecurity,
        
        // Threading (1 file: threading_system)
        CategoryThreading,
        
        // Signal/Slot (1 file: signal_slot_system)
        CategorySignalSlot,
        
        // Keyboard Shortcuts (1 file: keyboard_shortcuts)
        CategoryKeyboard,
        
        // Webview (1 file: webview_integration)
        CategoryWebview,
        
        // Session Management (1 file: session_manager)
        CategorySession,
        
        // Compression (1 file: quantization)
        CategoryCompression,
        
        // HTTP Client (1 file: http_client)
        CategoryHTTP,
        
        // JSON Parser (1 file: json_parser)
        CategoryJSON,
        
        // Ollama Integration (2 files: ollama_bridge, ollama_pull)
        CategoryOllama,
        
        // Advanced Features (1 file: advanced_stub_implementations)
        CategoryAdvanced,
        
        // Stub Systems (4 files: stub_completion, rawrxd_stubs, missing_stubs, etc.)
        CategoryStubs,
        
        // Test Harnesses (5 files: test_core_functions, test_gguf_loader, etc.)
        CategoryTest,
        
        // Experimental (RawrXD dual engine, rawr1024, etc.)
        CategoryExperimental
    };

    // Feature metadata
    struct FeatureInfo {
        QString name;                  // e.g., "model_memory_hotpatch"
        QString displayName;           // e.g., "Model Memory Hotpatch"
        QString description;           // Detailed explanation
        Category category;             // Which category it belongs to
        bool enabledByDefault;         // Default state
        bool requiresRestart;          // True if hot-reload not supported
        int estimatedMemoryKB;         // Memory footprint when enabled
        int estimatedCPUPercent;       // CPU overhead (0-100)
        QStringList dependencies;      // List of features this depends on
        QString asmFilePath;           // Path to .asm source file
    };

    // Get all available features (212 total)
    QList<FeatureInfo> getAllFeatures() const;
    
    // Get features by category
    QList<FeatureInfo> getFeaturesByCategory(Category category) const;
    
    // Feature state management
    bool isFeatureEnabled(const QString& featureName) const;
    void setFeatureEnabled(const QString& featureName, bool enabled);
    void resetToDefaults();
    
    // Category-level control
    void setCategoryEnabled(Category category, bool enabled);
    bool isCategoryEnabled(Category category) const;
    
    // Bulk operations
    void enableAll();
    void disableAll();
    void enableCategory(Category category);
    void disableCategory(Category category);
    
    // Performance profiling
    struct PerformanceMetrics {
        qint64 totalCpuTimeMs;         // Total CPU time consumed
        qint64 peakMemoryBytes;        // Peak memory usage
        int callCount;                 // Number of times invoked
        double avgLatencyMs;           // Average latency per call
    };
    
    PerformanceMetrics getPerformanceMetrics(const QString& featureName) const;
    void resetPerformanceMetrics();
    
    // Hot-reload support (for features that support it)
    bool canHotReload(const QString& featureName) const;
    bool hotReload(const QString& featureName);
    
    // Settings persistence
    void saveSettings();
    void loadSettings();
    
    // Export/Import configurations
    bool exportConfig(const QString& filePath) const;
    bool importConfig(const QString& filePath);
    
    // Presets
    enum Preset {
        PresetMinimal,         // Only critical features (Runtime + Hotpatch)
        PresetStandard,        // Balanced (Core + UI + Agent)
        PresetPerformance,     // Optimized for speed (no telemetry, minimal logging)
        PresetDevelopment,     // All dev tools (full logging, profiling, test harnesses)
        PresetMaximum,         // Everything enabled (212 features)
        PresetCustom           // User-defined
    };
    
    void applyPreset(Preset preset);
    Preset getCurrentPreset() const;
    
    // Helper methods (public for UI components)
    QString categoryToString(Category category) const;
    Category stringToCategory(const QString& categoryStr) const;
    
signals:
    void featureEnabledChanged(const QString& featureName, bool enabled);
    void categoryEnabledChanged(Category category, bool enabled);
    void presetChanged(Preset preset);
    void performanceMetricsUpdated(const QString& featureName);
    void hotReloadCompleted(const QString& featureName, bool success);

private:
    explicit MasmFeatureManager(QObject* parent = nullptr);
    ~MasmFeatureManager() override;
    MasmFeatureManager(const MasmFeatureManager&) = delete;
    MasmFeatureManager& operator=(const MasmFeatureManager&) = delete;

    // Internal state
    QHash<QString, FeatureInfo> m_features;
    QHash<QString, bool> m_enabledStates;
    QHash<QString, PerformanceMetrics> m_performanceData;
    QSettings* m_settings;
    Preset m_currentPreset;
    
    // Initialization
    void initializeFeatures();
    void registerFeature(const FeatureInfo& info);
};
