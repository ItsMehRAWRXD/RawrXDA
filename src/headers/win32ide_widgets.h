#ifndef WIN32IDE_WIDGETS_H_
#define WIN32IDE_WIDGETS_H_

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

// Forward declarations
struct HWND__;
typedef HWND__* HWND;

// ============================================================================
// Model Version Structure
// ============================================================================
struct ModelVersion {
    std::string name;
    std::string path;
    uint64_t sizeBytes = 0;
    bool isActive = false;
};

// ============================================================================
// ProjectContext (Core context)
// ============================================================================
namespace RawrXD {

struct ProjectContext {
    std::string workspacePath;
    std::vector<std::string> openFiles;
    ProjectContext() = default;
};

} // namespace RawrXD

// ============================================================================
// CATEGORY 1 - Win32IDE UI Widgets (~40 symbols)
// ============================================================================

/**
 * @class BenchmarkMenu
 * @brief Menu widget for benchmark operations in Win32IDE
 */
class BenchmarkMenu {
public:
    /**
     * Constructor
     * @param hwnd Parent window handle (optional)
     */
    explicit BenchmarkMenu(HWND* hwnd = nullptr);

    /**
     * Destructor
     */
    ~BenchmarkMenu();

    /**
     * Initialize the benchmark menu
     */
    void initialize();

    /**
     * Open the benchmark dialog
     */
    void openBenchmarkDialog();

private:
    void* m_hwnd;
};

/**
 * @class CheckpointManager
 * @brief Manages checkpoint creation and restoration
 */
class CheckpointManager {
public:
    /**
     * Constructor
     * @param ide IDE context pointer (optional)
     */
    explicit CheckpointManager(void* ide = nullptr);

    /**
     * Destructor
     */
    ~CheckpointManager();

    /**
     * Initialize checkpoint manager with path and max checkpoint count
     * @param path Directory path for checkpoint storage
     * @param maxCheckpoints Maximum number of checkpoints to maintain
     * @return true if initialization successful
     */
    bool initialize(const std::string& path, int maxCheckpoints);

private:
    std::string m_path;
    int m_maxCheckpoints;
};

/**
 * @class IocpFileWatcher
 * @brief File system watcher using I/O Completion Ports
 */
class IocpFileWatcher {
public:
    /**
     * Constructor
     */
    IocpFileWatcher();

    /**
     * Destructor
     */
    ~IocpFileWatcher();

    /**
     * Start watching directory
     * @param path Directory path to watch
     * @return true if watch started successfully
     */
    bool Start(const std::wstring& path);

    /**
     * Stop watching directory
     */
    void Stop();

    /**
     * Set callback for file change notifications
     * @param cb Callback function to invoke on file changes
     */
    void SetCallback(std::function<void(const std::string&)> cb);

private:
    std::function<void(const std::string&)> m_callback;
    bool m_running;
};

/**
 * @class ModelRegistry
 * @brief Registry for available AI models
 */
class ModelRegistry {
public:
    /**
     * Constructor
     * @param ide IDE context pointer (optional)
     */
    explicit ModelRegistry(void* ide = nullptr);

    /**
     * Destructor
     */
    ~ModelRegistry();

    /**
     * Initialize the model registry
     */
    void initialize();

    /**
     * Get all registered models
     * @return Vector of ModelVersion structures
     */
    std::vector<ModelVersion> getAllModels() const;

    /**
     * Set active model by index
     * @param index Model index
     * @return true if model activated successfully
     */
    bool setActiveModel(int index);

private:
    std::vector<ModelVersion> m_models;
    int m_activeIndex;
};

/**
 * @class MultiFileSearchWidget
 * @brief Widget for searching across multiple files
 */
class MultiFileSearchWidget {
public:
    /**
     * Destructor
     */
    ~MultiFileSearchWidget();

private:
};

/**
 * @class UniversalModelRouter
 * @brief Routes AI requests to appropriate model backends
 */
class UniversalModelRouter {
public:
    /**
     * Constructor
     */
    UniversalModelRouter();

    /**
     * Destructor
     */
    ~UniversalModelRouter();

    /**
     * Get available backend engines
     * @return Vector of available backend names
     */
    std::vector<std::string> getAvailableBackends() const;

    /**
     * Initialize local AI engine with model path
     * @param modelPath Path to local model file
     */
    void initializeLocalEngine(const std::string& modelPath);

    /**
     * Route inference request to appropriate backend
     * @param model Model identifier
     * @param prompt User prompt/input
     * @param ctx Project context
     * @param callback Response callback function
     */
    void routeRequest(const std::string& model,
                     const std::string& prompt,
                     const RawrXD::ProjectContext& ctx,
                     std::function<void(const std::string&, bool)> callback);

private:
    std::vector<std::string> m_backends;
};

/**
 * @class FeatureRegistryPanel
 * @brief Panel for managing feature registry
 */
class FeatureRegistryPanel {
public:
    /**
     * Destructor
     */
    ~FeatureRegistryPanel();

private:
};

/**
 * @class InterpretabilityPanel
 * @brief Panel for model interpretability and visualization
 */
class InterpretabilityPanel {
public:
    /**
     * Initialize the interpretability panel
     */
    void initialize();

    /**
     * Set parent window
     * @param hwnd Parent window handle
     */
    void setParent(void* hwnd);

    /**
     * Show the panel
     */
    void show();

private:
    void* m_parent;
};

#endif // WIN32IDE_WIDGETS_H_
