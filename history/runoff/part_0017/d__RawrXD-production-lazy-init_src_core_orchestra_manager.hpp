#ifndef ORCHESTRA_MANAGER_HPP
#define ORCHESTRA_MANAGER_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QFuture>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <memory>

/**
 * @brief OrchestraManager - Core IDE orchestration engine
 * 
 * Provides all IDE functionality through a unified API that can be
 * consumed by both GUI and CLI frontends. This ensures feature parity
 * between the visual IDE and command-line interface.
 * 
 * Features:
 * - Project management (open, close, create)
 * - Build system integration (CMake, QMake, Meson, etc.)
 * - Version control (Git, SVN, Mercurial)
 * - Debugging and profiling
 * - AI/ML model inference
 * - File operations and search
 * - Terminal/shell execution
 * - Testing and code coverage
 * - Hotpatching and runtime modification
 */

namespace RawrXD {

// Forward declarations
class BuildManager;
class VCSManager;
class DebugManager;
class ProfilerManager;
class AIInferenceManager;
class TerminalManager;
class TestManager;
class HotpatchManager;
class ProjectManager;
class FileManager;
class ModelDiscoveryService; // NEW: Model discovery service

/**
 * @brief Task execution result
 */
struct TaskResult {
    bool success = false;
    QString message;
    QJsonObject data;
    int exitCode = 0;
    qint64 durationMs = 0;
};

/**
 * @brief Task progress callback
 */
using ProgressCallback = std::function<void(int percent, const QString& status)>;

/**
 * @brief Output callback for streaming results
 */
using OutputCallback = std::function<void(const QString& line, bool isError)>;

/**
 * @brief Completion callback for async operations
 */
using CompletionCallback = std::function<void(const TaskResult& result)>;

/**
 * @brief Model information structure
 */
struct ModelInfo {
    QString id;
    QString name;
    QString path;
    qint64 sizeMB = 0;
    QString format;  // GGUF, SafeTensors, PyTorch, etc.
    QString framework;  // Llama, Mistral, Phi, etc.
    uint32_t parameterCount = 0;  // in millions
    QString quantization;  // Q4_0, Q5_1, FP16, etc.
    bool cached = false;
    QString checksum;
    QJsonObject metadata;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["path"] = path;
        obj["size_mb"] = static_cast<int>(sizeMB);
        obj["format"] = format;
        obj["framework"] = framework;
        obj["parameters_m"] = static_cast<int>(parameterCount);
        obj["quantization"] = quantization;
        obj["cached"] = cached;
        obj["checksum"] = checksum;
        obj["metadata"] = metadata;
        return obj;
    }
    
    static ModelInfo fromJson(const QJsonObject& obj) {
        ModelInfo m;
        m.id = obj["id"].toString();
        m.name = obj["name"].toString();
        m.path = obj["path"].toString();
        m.sizeMB = obj["size_mb"].toInt();
        m.format = obj["format"].toString();
        m.framework = obj["framework"].toString();
        m.parameterCount = obj["parameters_m"].toInt();
        m.quantization = obj["quantization"].toString();
        m.cached = obj["cached"].toBool();
        m.checksum = obj["checksum"].toString();
        m.metadata = obj["metadata"].toObject();
        return m;
    }
};

/**
 * @brief Model discovery and loading metrics
 */
struct ModelMetrics {
    ModelMetrics() = default;
    ModelMetrics(const ModelMetrics& other)
        : totalDiscoveryCalls(other.totalDiscoveryCalls)
        , discoveryLatencies(other.discoveryLatencies)
        , totalLoadCalls(other.totalLoadCalls)
        , successfulLoads(other.successfulLoads)
        , failedLoads(other.failedLoads)
        , loadLatencies(other.loadLatencies)
        , cacheHits(other.cacheHits)
        , cacheMisses(other.cacheMisses)
        , totalCacheSize(other.totalCacheSize)
        , selectionLatencies(other.selectionLatencies) {
    }

    ModelMetrics& operator=(const ModelMetrics& other) {
        if (this != &other) {
            totalDiscoveryCalls = other.totalDiscoveryCalls;
            discoveryLatencies = other.discoveryLatencies;
            totalLoadCalls = other.totalLoadCalls;
            successfulLoads = other.successfulLoads;
            failedLoads = other.failedLoads;
            loadLatencies = other.loadLatencies;
            cacheHits = other.cacheHits;
            cacheMisses = other.cacheMisses;
            totalCacheSize = other.totalCacheSize;
            selectionLatencies = other.selectionLatencies;
        }
        return *this;
    }

    // Discovery metrics
    uint64_t totalDiscoveryCalls = 0;
    std::vector<uint64_t> discoveryLatencies;  // microseconds
    
    // Load metrics
    uint64_t totalLoadCalls = 0;
    uint64_t successfulLoads = 0;
    uint64_t failedLoads = 0;
    std::vector<uint64_t> loadLatencies;  // microseconds
    
    // Cache metrics
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t totalCacheSize = 0;
    
    // Selection metrics
    std::vector<uint64_t> selectionLatencies;  // microseconds
    
    mutable std::mutex lock;
    
    QString generatePrometheusMetrics() const {
        std::stringstream ss;
        
        // Counter metrics
        ss << "# HELP model_discovery_total Total number of model discovery operations\n";
        ss << "# TYPE model_discovery_total counter\n";
        ss << "model_discovery_total " << totalDiscoveryCalls << "\n\n";
        
        ss << "# HELP model_load_total Total number of model load attempts\n";
        ss << "# TYPE model_load_total counter\n";
        ss << "model_load_total " << totalLoadCalls << "\n\n";
        
        ss << "# HELP model_load_success Total number of successful model loads\n";
        ss << "# TYPE model_load_success counter\n";
        ss << "model_load_success " << successfulLoads << "\n\n";
        
        ss << "# HELP model_load_failures Total number of failed model loads\n";
        ss << "# TYPE model_load_failures counter\n";
        ss << "model_load_failures " << failedLoads << "\n\n";
        
        ss << "# HELP model_cache_hits Total cache hits\n";
        ss << "# TYPE model_cache_hits counter\n";
        ss << "model_cache_hits " << cacheHits << "\n\n";
        
        ss << "# HELP model_cache_misses Total cache misses\n";
        ss << "# TYPE model_cache_misses counter\n";
        ss << "model_cache_misses " << cacheMisses << "\n\n";
        
        return QString::fromStdString(ss.str());
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["total_discovery_calls"] = static_cast<qint64>(totalDiscoveryCalls);
        obj["total_load_calls"] = static_cast<qint64>(totalLoadCalls);
        obj["successful_loads"] = static_cast<qint64>(successfulLoads);
        obj["failed_loads"] = static_cast<qint64>(failedLoads);
        obj["cache_hits"] = static_cast<qint64>(cacheHits);
        obj["cache_misses"] = static_cast<qint64>(cacheMisses);
        obj["total_cache_size"] = static_cast<qint64>(totalCacheSize);
        return obj;
    }
};

/**
 * @brief Headless mode configuration for CI/CD environments
 */
struct HeadlessConfig {
    bool enabled = false;           // Whether headless mode is active
    bool suppressUI = true;         // Suppress all UI dialogs
    bool autoExit = true;           // Exit after task completion
    int timeoutSeconds = 300;       // Default timeout (5 minutes)
    QString outputFormat = "json";  // Output format: json, plain, silent
    QString logLevel = "INFO";      // Log level for headless mode
    bool failFast = true;           // Exit on first error
    
    QJsonObject toJson() const {
        return QJsonObject{
            {"enabled", enabled},
            {"suppress_ui", suppressUI},
            {"auto_exit", autoExit},
            {"timeout_seconds", timeoutSeconds},
            {"output_format", outputFormat},
            {"log_level", logLevel},
            {"fail_fast", failFast}
        };
    }
    
    static HeadlessConfig fromJson(const QJsonObject& obj) {
        HeadlessConfig cfg;
        cfg.enabled = obj["enabled"].toBool(false);
        cfg.suppressUI = obj["suppress_ui"].toBool(true);
        cfg.autoExit = obj["auto_exit"].toBool(true);
        cfg.timeoutSeconds = obj["timeout_seconds"].toInt(300);
        cfg.outputFormat = obj["output_format"].toString("json");
        cfg.logLevel = obj["log_level"].toString("INFO");
        cfg.failFast = obj["fail_fast"].toBool(true);
        return cfg;
    }
};

class OrchestraManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     */
    static OrchestraManager& instance();

    /**
     * @brief Initialize the orchestra manager
     * @param configPath Optional path to configuration file
     * @return true if initialization successful
     */
    bool initialize(const QString& configPath = QString());

    /**
     * @brief Shutdown and cleanup all subsystems
     */
    void shutdown();

    /**
     * @brief Check if manager is initialized
     */
    bool isInitialized() const { return m_initialized; }

    // ================================================================
    // Headless Mode / CI-CD Support
    // ================================================================

    /**
     * @brief Enable headless mode for CI/CD environments
     * @param config Headless configuration
     */
    void setHeadlessMode(const HeadlessConfig& config);

    /**
     * @brief Check if running in headless mode
     */
    bool isHeadless() const { return m_headlessConfig.enabled; }

    /**
     * @brief Get current headless configuration
     */
    HeadlessConfig headlessConfig() const { return m_headlessConfig; }

    /**
     * @brief Run a batch of commands in headless mode
     * @param commands List of commands to execute
     * @param callback Completion callback with results
     * @return Exit code (0 = success)
     */
    int runHeadlessBatch(const QStringList& commands, CompletionCallback callback = nullptr);

    /**
     * @brief Run a single command in headless mode
     * @param command Command to execute
     * @param args Arguments
     * @return Task result
     */
    TaskResult runHeadlessCommand(const QString& command, const QStringList& args = QStringList());

    // ================================================================
    // Project Management
    // ================================================================

    /**
     * @brief Open a project directory
     * @param path Project root path
     * @param callback Completion callback
     */
    void openProject(const QString& path, CompletionCallback callback = nullptr);

    /**
     * @brief Close current project
     */
    void closeProject();

    /**
     * @brief Create a new project
     * @param path Project path
     * @param templateName Project template (e.g., "cpp", "python", "rust")
     * @param options Additional creation options
     * @param callback Completion callback
     */
    void createProject(const QString& path, const QString& templateName,
                       const QVariantMap& options = QVariantMap(),
                       CompletionCallback callback = nullptr);

    /**
     * @brief Get current project path
     */
    QString currentProjectPath() const { return m_projectPath; }

    /**
     * @brief Check if a project is open
     */
    bool hasOpenProject() const { return !m_projectPath.isEmpty(); }

    // ================================================================
    // Build System
    // ================================================================

    /**
     * @brief Build the current project
     * @param target Build target (empty for default)
     * @param config Configuration (Debug/Release)
     * @param output Output callback for build output
     * @param callback Completion callback
     */
    void build(const QString& target = QString(),
               const QString& config = "Release",
               OutputCallback output = nullptr,
               CompletionCallback callback = nullptr);

    /**
     * @brief Clean build artifacts
     * @param callback Completion callback
     */
    void clean(CompletionCallback callback = nullptr);

    /**
     * @brief Rebuild (clean + build)
     * @param target Build target
     * @param config Configuration
     * @param output Output callback
     * @param callback Completion callback
     */
    void rebuild(const QString& target = QString(),
                 const QString& config = "Release",
                 OutputCallback output = nullptr,
                 CompletionCallback callback = nullptr);

    /**
     * @brief Configure build system (CMake configure, etc.)
     * @param options Configuration options
     * @param output Output callback
     * @param callback Completion callback
     */
    void configure(const QVariantMap& options = QVariantMap(),
                   OutputCallback output = nullptr,
                   CompletionCallback callback = nullptr);

    /**
     * @brief List available build targets
     */
    QStringList listBuildTargets() const;

    // ================================================================
    // Version Control
    // ================================================================

    /**
     * @brief Get VCS status for current project
     * @param callback Completion callback with status info
     */
    void vcsStatus(CompletionCallback callback = nullptr);

    /**
     * @brief Stage files for commit
     * @param files Files to stage (empty for all)
     * @param callback Completion callback
     */
    void vcsStage(const QStringList& files = QStringList(),
                  CompletionCallback callback = nullptr);

    /**
     * @brief Commit staged changes
     * @param message Commit message
     * @param callback Completion callback
     */
    void vcsCommit(const QString& message, CompletionCallback callback = nullptr);

    /**
     * @brief Push to remote
     * @param remote Remote name (default: origin)
     * @param branch Branch name (default: current)
     * @param callback Completion callback
     */
    void vcsPush(const QString& remote = "origin",
                 const QString& branch = QString(),
                 CompletionCallback callback = nullptr);

    /**
     * @brief Pull from remote
     * @param remote Remote name
     * @param branch Branch name
     * @param callback Completion callback
     */
    void vcsPull(const QString& remote = "origin",
                 const QString& branch = QString(),
                 CompletionCallback callback = nullptr);

    /**
     * @brief Create a new branch
     * @param name Branch name
     * @param checkout Whether to checkout after creation
     * @param callback Completion callback
     */
    void vcsCreateBranch(const QString& name, bool checkout = true,
                         CompletionCallback callback = nullptr);

    /**
     * @brief Switch to a branch
     * @param name Branch name
     * @param callback Completion callback
     */
    void vcsCheckout(const QString& name, CompletionCallback callback = nullptr);

    /**
     * @brief List branches
     * @param includeRemote Include remote branches
     */
    QStringList vcsListBranches(bool includeRemote = false) const;

    /**
     * @brief Get commit history
     * @param count Number of commits to retrieve
     * @param callback Completion callback with history
     */
    void vcsLog(int count = 50, CompletionCallback callback = nullptr);

    // ================================================================
    // Debugging
    // ================================================================

    /**
     * @brief Start debugging session
     * @param executable Path to executable
     * @param args Command-line arguments
     * @param workingDir Working directory
     * @param callback Completion callback
     */
    void debugStart(const QString& executable,
                    const QStringList& args = QStringList(),
                    const QString& workingDir = QString(),
                    CompletionCallback callback = nullptr);

    /**
     * @brief Stop debugging session
     */
    void debugStop();

    /**
     * @brief Continue execution
     */
    void debugContinue();

    /**
     * @brief Step over (next line)
     */
    void debugStepOver();

    /**
     * @brief Step into (enter function)
     */
    void debugStepInto();

    /**
     * @brief Step out (exit function)
     */
    void debugStepOut();

    /**
     * @brief Set breakpoint
     * @param file Source file
     * @param line Line number
     * @param condition Optional condition
     * @return Breakpoint ID
     */
    int debugSetBreakpoint(const QString& file, int line,
                           const QString& condition = QString());

    /**
     * @brief Remove breakpoint
     * @param id Breakpoint ID
     */
    void debugRemoveBreakpoint(int id);

    /**
     * @brief List all breakpoints
     */
    QJsonArray debugListBreakpoints() const;

    /**
     * @brief Evaluate expression in debug context
     * @param expression Expression to evaluate
     * @param callback Completion callback with result
     */
    void debugEvaluate(const QString& expression,
                       CompletionCallback callback = nullptr);

    /**
     * @brief Get call stack
     */
    QJsonArray debugGetCallStack() const;

    /**
     * @brief Get local variables
     */
    QJsonObject debugGetLocals() const;

    /**
     * @brief Check if debugger is running
     */
    bool isDebugging() const { return m_debugging; }

    // ================================================================
    // AI/ML Inference
    // ================================================================

    /**
     * @brief Load a GGUF model
     * @param modelPath Path to model file
     * @param progress Progress callback
     * @param callback Completion callback
     */
    void aiLoadModel(const QString& modelPath,
                     ProgressCallback progress = nullptr,
                     CompletionCallback callback = nullptr);

    /**
     * @brief Unload current model
     */
    void aiUnloadModel();

    /**
     * @brief Run inference
     * @param prompt Input prompt
     * @param options Inference options (temperature, max_tokens, etc.)
     * @param output Streaming output callback
     * @param callback Completion callback
     */
    void aiInfer(const QString& prompt,
                 const QVariantMap& options = QVariantMap(),
                 OutputCallback output = nullptr,
                 CompletionCallback callback = nullptr);

    /**
     * @brief Cancel current inference
     */
    void aiCancelInference();

    /**
     * @brief Get code completion suggestions
     * @param context Code context
     * @param cursorPosition Cursor position
     * @param callback Completion callback with suggestions
     */
    void aiCodeComplete(const QString& context,
                        int cursorPosition,
                        CompletionCallback callback = nullptr);

    /**
     * @brief Get code explanation
     * @param code Code to explain
     * @param output Streaming output
     * @param callback Completion callback
     */
    void aiExplainCode(const QString& code,
                       OutputCallback output = nullptr,
                       CompletionCallback callback = nullptr);

    /**
     * @brief Get refactoring suggestions
     * @param code Code to refactor
     * @param instruction Refactoring instruction
     * @param output Streaming output
     * @param callback Completion callback
     */
    void aiRefactorCode(const QString& code,
                        const QString& instruction,
                        OutputCallback output = nullptr,
                        CompletionCallback callback = nullptr);

    /**
     * @brief Check if model is loaded
     */
    bool aiIsModelLoaded() const { return m_modelLoaded; }

    /**
     * @brief Get loaded model name
     */
    QString aiModelName() const { return m_modelName; }

    // ================================================================
    // Terminal/Shell Execution
    // ================================================================

    /**
     * @brief Execute a command
     * @param command Command to execute
     * @param args Command arguments
     * @param workingDir Working directory
     * @param output Output callback
     * @param callback Completion callback
     */
    void exec(const QString& command,
              const QStringList& args = QStringList(),
              const QString& workingDir = QString(),
              OutputCallback output = nullptr,
              CompletionCallback callback = nullptr);

    /**
     * @brief Execute a shell command (via cmd.exe or bash)
     * @param command Shell command string
     * @param output Output callback
     * @param callback Completion callback
     */
    void shell(const QString& command,
               OutputCallback output = nullptr,
               CompletionCallback callback = nullptr);

    /**
     * @brief Create a new terminal session
     * @param shell Shell to use (pwsh, cmd, bash)
     * @return Terminal session ID
     */
    int createTerminal(const QString& shell = "pwsh");

    /**
     * @brief Send input to terminal
     * @param sessionId Terminal session ID
     * @param input Input text
     */
    void terminalSend(int sessionId, const QString& input);

    /**
     * @brief Close terminal session
     * @param sessionId Terminal session ID
     */
    void terminalClose(int sessionId);

    /**
     * @brief List active terminal sessions
     */
    QJsonArray listTerminals() const;

    // ================================================================
    // File Operations
    // ================================================================

    /**
     * @brief Read file contents
     * @param path File path
     * @return File contents or empty string on error
     */
    QString readFile(const QString& path) const;

    /**
     * @brief Write file contents
     * @param path File path
     * @param content Content to write
     * @return true if successful
     */
    bool writeFile(const QString& path, const QString& content);

    /**
     * @brief Search files by pattern
     * @param pattern Glob pattern
     * @param searchPath Search root (default: project root)
     * @return List of matching file paths
     */
    QStringList findFiles(const QString& pattern,
                          const QString& searchPath = QString()) const;

    /**
     * @brief Search file contents
     * @param query Search query (regex supported)
     * @param filePattern File pattern to search in
     * @param searchPath Search root
     * @param callback Completion callback with results
     */
    void searchInFiles(const QString& query,
                       const QString& filePattern = "*",
                       const QString& searchPath = QString(),
                       CompletionCallback callback = nullptr);

    /**
     * @brief Replace in files
     * @param search Search pattern
     * @param replace Replacement text
     * @param filePattern File pattern
     * @param searchPath Search root
     * @param dryRun If true, only report changes
     * @param callback Completion callback
     */
    void replaceInFiles(const QString& search,
                        const QString& replace,
                        const QString& filePattern = "*",
                        const QString& searchPath = QString(),
                        bool dryRun = false,
                        CompletionCallback callback = nullptr);

    // ================================================================
    // Testing
    // ================================================================

    /**
     * @brief Discover tests in project
     * @param callback Completion callback with test list
     */
    void testDiscover(CompletionCallback callback = nullptr);

    /**
     * @brief Run all tests
     * @param output Output callback
     * @param callback Completion callback with results
     */
    void testRunAll(OutputCallback output = nullptr,
                    CompletionCallback callback = nullptr);

    /**
     * @brief Run specific tests
     * @param testIds Test IDs to run
     * @param output Output callback
     * @param callback Completion callback
     */
    void testRun(const QStringList& testIds,
                 OutputCallback output = nullptr,
                 CompletionCallback callback = nullptr);

    /**
     * @brief Get code coverage report
     * @param callback Completion callback with coverage data
     */
    void testCoverage(CompletionCallback callback = nullptr);

    // ================================================================
    // Hotpatching
    // ================================================================

    /**
     * @brief Apply a hotpatch
     * @param patchData Patch data
     * @param callback Completion callback
     */
    void hotpatchApply(const QByteArray& patchData,
                       CompletionCallback callback = nullptr);

    /**
     * @brief Revert last hotpatch
     * @param callback Completion callback
     */
    void hotpatchRevert(CompletionCallback callback = nullptr);

    /**
     * @brief List applied hotpatches
     */
    QJsonArray hotpatchList() const;

    // ================================================================
    // Diagnostics & Health Checks
    // ================================================================

    /**
     * @brief Run diagnostics for toolchain and environment
     * @param callback Completion callback with diagnostics result
     */
    void runDiagnostics(CompletionCallback callback = nullptr);

    // ================================================================
    // Model Discovery Service
    // ================================================================

    /**
     * @brief Initialize model discovery service
     * @param cachePath Model cache directory path
     * @param maxCacheSizeMB Maximum cache size in MB
     * @return true if initialization successful
     */
    bool initializeModelDiscovery(const QString& cachePath = QString(), 
                                  qint64 maxCacheSizeMB = 50000);

    /**
     * @brief Discover models in specified paths
     * @param searchPaths Paths to search for models
     * @param recursive Whether to search recursively
     * @return List of discovered models
     */
    QList<ModelInfo> discoverModels(const QStringList& searchPaths = QStringList(), 
                                   bool recursive = true);

    /**
     * @brief Get all discovered models
     */
    QList<ModelInfo> getDiscoveredModels() const;

    /**
     * @brief Get metadata for a specific model
     */
    ModelInfo getModelInfo(const QString& modelId) const;

    /**
     * @brief Load a model by ID
     * @param modelId Model identifier
     * @param progress Progress callback (0-100)
     * @return true if load successful
     */
    bool loadModel(const QString& modelId, 
                   std::function<void(int)> progress = nullptr);

    /**
     * @brief Get model discovery metrics
     */
    ModelMetrics getModelMetrics() const;

    /**
     * @brief Check if model discovery service is initialized
     */
    bool isModelDiscoveryInitialized() const { return m_modelDiscoveryService != nullptr; }

    /**
     * @brief Get system information
     */
    QJsonObject getSystemInfo() const;

    /**
     * @brief Get subsystem status
     */
    QJsonObject getSubsystemStatus() const;

signals:
    // Project signals
    void projectOpened(const QString& path);
    void projectClosed();

    // Build signals
    void buildStarted(const QString& target);
    void buildProgress(int percent, const QString& status);
    void buildOutput(const QString& line, bool isError);
    void buildFinished(bool success, const QString& message);

    // VCS signals
    void vcsStatusChanged();
    void vcsOperationStarted(const QString& operation);
    void vcsOperationFinished(const QString& operation, bool success);

    // Debug signals
    void debugStarted();
    void debugStopped();
    void debugPaused(const QString& file, int line);
    void debugResumed();
    void debugOutput(const QString& output);
    void breakpointHit(int id, const QString& file, int line);

    // Model discovery signals
    void modelDiscoveryStarted();
    void modelDiscoveryProgress(int percent, const QString& status);
    void modelDiscoveryFinished(const QList<ModelInfo>& models);
    void modelDiscoveryError(const QString& error);
    void modelLoadingStarted(const QString& modelId);
    void modelLoadingProgress(int percent);
    void modelLoaded(const QString& modelId);
    void modelLoadFailed(const QString& modelId, const QString& error);

    // AI inference signals
    void aiModelLoaded(const QString& modelName);
    void aiModelUnloaded();
    void aiInferenceStarted();
    void aiInferenceToken(const QString& token);
    void aiInferenceFinished();

    // Terminal signals
    void terminalCreated(int sessionId);
    void terminalOutput(int sessionId, const QString& output);
    void terminalClosed(int sessionId);

    // Test signals
    void testStarted(const QString& testId);
    void testPassed(const QString& testId);
    void testFailed(const QString& testId, const QString& message);
    void testFinished(int passed, int failed, int skipped);

    // General signals
    void error(const QString& component, const QString& message);
    void warning(const QString& component, const QString& message);
    void info(const QString& component, const QString& message);

private:
    OrchestraManager(QObject* parent = nullptr);
    ~OrchestraManager();
    Q_DISABLE_COPY(OrchestraManager)

    bool m_initialized = false;
    QString m_projectPath;
    bool m_debugging = false;
    bool m_modelLoaded = false;
    QString m_modelName;
    HeadlessConfig m_headlessConfig;  // Headless mode configuration

    // Subsystem managers (implemented in orchestra_manager.cpp)
    std::unique_ptr<BuildManager> m_buildManager;
    std::unique_ptr<VCSManager> m_vcsManager;
    std::unique_ptr<DebugManager> m_debugManager;
    std::unique_ptr<ProfilerManager> m_profilerManager;
    std::unique_ptr<AIInferenceManager> m_aiManager;
    std::unique_ptr<TerminalManager> m_terminalManager;
    std::unique_ptr<TestManager> m_testManager;
    std::unique_ptr<HotpatchManager> m_hotpatchManager;
    std::unique_ptr<ProjectManager> m_projectManager;
    std::unique_ptr<FileManager> m_fileManager;
    std::unique_ptr<ModelDiscoveryService> m_modelDiscoveryService; // NEW: Model discovery service
};

} // namespace RawrXD

#endif // ORCHESTRA_MANAGER_HPP
