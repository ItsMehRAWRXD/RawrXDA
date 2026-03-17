#ifndef SAFE_MODE_CONFIG_HPP
#define SAFE_MODE_CONFIG_HPP

#include <QString>
#include <QStringList>
#include <QFlags>
#include <cstdint>

/**
 * @brief Safe Mode Configuration for RawrXD-QtShell
 * 
 * Provides comprehensive safe mode infrastructure with feature gates,
 * subsystem control, and diagnostic capabilities.
 */

namespace SafeMode {

/**
 * @brief Application execution modes
 */
enum class AppMode : uint32_t {
    NormalGUI = 0,         ///< Full IDE with all features
    SafeModeGUI = 1,       ///< GUI with core features only
    SafeModeCLI = 2,       ///< Command-line diagnostic mode
    CLIMode = 3            ///< Full CLI orchestration mode
};

/**
 * @brief Feature flags for safe mode
 */
enum class FeatureFlag : uint32_t {
    // Core features
    CoreEditor = 1 << 0,           ///< Basic text editor
    ProjectExplorer = 1 << 1,      ///< File/project browser
    ProblemsPanel = 1 << 2,        ///< Diagnostics and errors
    
    // AI/ML features
    AIChat = 1 << 3,               ///< AI chat panel
    AICodeCompletion = 1 << 4,     ///< AI code completion
    AIModels = 1 << 5,             ///< GGUF model loading
    Inference = 1 << 6,            ///< Model inference engine
    
    // Development features
    Debugger = 1 << 7,             ///< Debugging support
    TestRunner = 1 << 8,           ///< Unit test execution
    BuildSystem = 1 << 9,          ///< Build integration
    VersionControl = 1 << 10,      ///< Git/VCS integration
    
    // Performance/Monitoring
    Profiler = 1 << 11,            ///< Performance profiling
    LatencyMonitor = 1 << 12,      ///< Latency tracking
    Metrics = 1 << 13,             ///< Metrics collection
    
    // Advanced features
    Hotpatch = 1 << 14,            ///< Runtime hotpatching
    MASMIntegration = 1 << 15,     ///< MASM assembly support
    Collaboration = 1 << 16,       ///< Real-time collaboration
    CloudSync = 1 << 17,           ///< Cloud synchronization
    
    // Format support
    MarkdownSupport = 1 << 18,     ///< Markdown editing
    SheetSupport = 1 << 19,        ///< Spreadsheet editing
    MultiLangSupport = 1 << 20,    ///< 60+ language support
    
    // Terminal/Shell
    TerminalCluster = 1 << 21,     ///< Multi-terminal support
    ExecutionEngine = 1 << 22,     ///< Task execution
    
    // Agentic systems
    AgentSystem = 1 << 23,         ///< Autonomous agent framework
    Planning = 1 << 24,            ///< Agent planning
    ErrorRecovery = 1 << 25        ///< Agent error recovery
};

Q_DECLARE_FLAGS(FeatureFlags, FeatureFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(FeatureFlags)

/**
 * @brief Safe mode configuration
 */
class Config {
public:
    /**
     * @brief Get the global safe mode configuration instance
     */
    static Config& instance();
    
    /**
     * @brief Initialize safe mode configuration from command-line arguments
     */
    static void initialize(const QStringList& args);
    
    // Mode queries
    AppMode appMode() const { return m_appMode; }
    bool isNormalGUI() const { return m_appMode == AppMode::NormalGUI; }
    bool isSafeModeGUI() const { return m_appMode == AppMode::SafeModeGUI; }
    bool isSafeModeCLI() const { return m_appMode == AppMode::SafeModeCLI; }
    bool isCLIMode() const { return m_appMode == AppMode::CLIMode; }
    bool isSafeMode() const { return m_appMode != AppMode::NormalGUI && m_appMode != AppMode::CLIMode; }
    
    // Feature queries
    bool isFeatureEnabled(FeatureFlag flag) const { return m_enabledFeatures.testFlag(flag); }
    bool isFeatureEnabled(FeatureFlags flags) const { return (m_enabledFeatures & flags) == flags; }
    FeatureFlags enabledFeatures() const { return m_enabledFeatures; }
    
    // Configuration setters
    void setAppMode(AppMode mode) { m_appMode = mode; }
    void setEnabledFeatures(FeatureFlags flags) { m_enabledFeatures = flags; }
    void enableFeature(FeatureFlag flag) { m_enabledFeatures |= flag; }
    void disableFeature(FeatureFlag flag) { m_enabledFeatures &= ~FeatureFlags(flag); }
    
    // Diagnostic settings
    bool verboseDiagnostics() const { return m_verboseDiagnostics; }
    void setVerboseDiagnostics(bool verbose) { m_verboseDiagnostics = verbose; }
    
    bool skipStartupChecks() const { return m_skipStartupChecks; }
    void setSkipStartupChecks(bool skip) { m_skipStartupChecks = skip; }
    
    bool enableRecoveryMode() const { return m_recoveryMode; }
    void setRecoveryMode(bool enable) { m_recoveryMode = enable; }
    
    QString diagnosticsOutputPath() const { return m_diagnosticsPath; }
    void setDiagnosticsOutputPath(const QString& path) { m_diagnosticsPath = path; }
    
    // Feature gate helpers
    static FeatureFlags getFeatureFlagsForMode(AppMode mode);
    static QString getModeDescription(AppMode mode);
    
    // String conversions
    QString appModeString() const;
    QStringList featureFlagsString() const;
    
private:
    Config();
    
    AppMode m_appMode = AppMode::NormalGUI;
    FeatureFlags m_enabledFeatures = FeatureFlags();
    bool m_verboseDiagnostics = false;
    bool m_skipStartupChecks = false;
    bool m_recoveryMode = false;
    QString m_diagnosticsPath;
};

/**
 * @brief Predefined feature sets for different modes
 */
namespace FeatureSets {
    // Normal GUI: all features enabled
    inline FeatureFlags getNormalGUIFeatures() {
        return FeatureFlags(
            SafeMode::FeatureFlag::CoreEditor |
            SafeMode::FeatureFlag::ProjectExplorer |
            SafeMode::FeatureFlag::ProblemsPanel |
            SafeMode::FeatureFlag::AIChat |
            SafeMode::FeatureFlag::AICodeCompletion |
            SafeMode::FeatureFlag::AIModels |
            SafeMode::FeatureFlag::Inference |
            SafeMode::FeatureFlag::Debugger |
            SafeMode::FeatureFlag::TestRunner |
            SafeMode::FeatureFlag::BuildSystem |
            SafeMode::FeatureFlag::VersionControl |
            SafeMode::FeatureFlag::Profiler |
            SafeMode::FeatureFlag::LatencyMonitor |
            SafeMode::FeatureFlag::Metrics |
            SafeMode::FeatureFlag::Hotpatch |
            SafeMode::FeatureFlag::MASMIntegration |
            SafeMode::FeatureFlag::Collaboration |
            SafeMode::FeatureFlag::CloudSync |
            SafeMode::FeatureFlag::MarkdownSupport |
            SafeMode::FeatureFlag::SheetSupport |
            SafeMode::FeatureFlag::MultiLangSupport |
            SafeMode::FeatureFlag::TerminalCluster |
            SafeMode::FeatureFlag::ExecutionEngine |
            SafeMode::FeatureFlag::AgentSystem |
            SafeMode::FeatureFlag::Planning |
            SafeMode::FeatureFlag::ErrorRecovery
        );
    }
    
    // Safe Mode GUI: core features only
    inline FeatureFlags getSafeModeGUIFeatures() {
        return FeatureFlags(
            SafeMode::FeatureFlag::CoreEditor |
            SafeMode::FeatureFlag::ProjectExplorer |
            SafeMode::FeatureFlag::ProblemsPanel
        );
    }
    
    // Safe Mode CLI: diagnostic features
    inline FeatureFlags getSafeModeCLIFeatures() {
        return FeatureFlags(
            SafeMode::FeatureFlag::ProblemsPanel |
            SafeMode::FeatureFlag::Metrics
        );
    }
    
    // CLI Mode: core execution features
    inline FeatureFlags getCLIModeFeatures() {
        return FeatureFlags(
            SafeMode::FeatureFlag::TerminalCluster |
            SafeMode::FeatureFlag::ExecutionEngine |
            SafeMode::FeatureFlag::AgentSystem |
            SafeMode::FeatureFlag::Planning
        );
    }
}

} // namespace SafeMode

#endif // SAFE_MODE_CONFIG_HPP
