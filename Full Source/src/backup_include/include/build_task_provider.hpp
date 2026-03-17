/*==========================================================================
 * RawrXD Build Task Provider
 *
 * Provides IDE build tasks for MASM64/C++ projects:
 *  - Single-file assembly (ml64)
 *  - Single-file C++ (cl.exe)
 *  - Link (link.exe / custom PE writer)
 *  - Full build pipeline (assemble + link → .exe)
 *  - Clean / Rebuild
 *
 * Integrates with:
 *  - RawrXD::Compiler::ToolchainBridge for actual compilation
 *  - DiagnosticsProvider for error reporting
 *  - IDEOrchestrator for lifecycle management
 *=========================================================================*/

#pragma once

#include "../include/toolchain/toolchain_bridge.h"
#include "diagnostics_provider.hpp"
#include "CommonTypes.h"

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace IDE {

/* ---- Build Task Types ---- */

enum class BuildTaskKind {
    Assemble,       /* .asm → .obj */
    CompileCpp,     /* .cpp → .obj */
    Link,           /* .obj → .exe */
    FullBuild,      /* .asm/.cpp → .exe */
    Clean,          /* remove outputs */
    Rebuild         /* clean + full-build */
};

enum class BuildTaskState {
    Pending,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

struct BuildTaskConfig {
    BuildTaskKind           kind       = BuildTaskKind::FullBuild;
    std::string             label;                     /* Display name */
    std::vector<std::string>sourceFiles;               /* Input file(s) */
    std::string             outputPath;                /* Output binary */
    std::string             entryPoint = "_start";
    uint32_t                subsystem  = 3;            /* Console */
    uint64_t                imageBase  = 0x140000000ULL;
    bool                    debugInfo  = false;
    bool                    verbose    = false;
    std::vector<std::string>includeLibs;               /* Extra .lib */
    std::vector<std::string>defines;                   /* /D macros */
};

struct BuildTaskResult {
    BuildTaskState          state      = BuildTaskState::Pending;
    std::string             outputPath;
    uint64_t                outputSize = 0;
    uint32_t                errorCount = 0;
    uint32_t                warningCount = 0;
    double                  elapsedMs  = 0.0;
    std::vector<RawrXD::Toolchain::ToolchainDiagnostic> diagnostics;
    std::string             log;                       /* Raw process output */
};

/* ---- BuildTask (runnable handle) ---- */

class BuildTask {
public:
    using ProgressCallback = std::function<void(const std::string& message, int pct)>;
    using CompletionCallback = std::function<void(const BuildTaskResult& result)>;

    BuildTask(const BuildTaskConfig& config, uint64_t id);
    ~BuildTask();

    uint64_t            getId()     const { return m_id; }
    const std::string&  getLabel()  const { return m_config.label; }
    BuildTaskState      getState()  const { return m_state.load(); }
    const BuildTaskResult& getResult() const { return m_result; }

    void setProgressCallback(ProgressCallback cb)   { m_progressCb = std::move(cb); }
    void setCompletionCallback(CompletionCallback cb){ m_completionCb = std::move(cb); }

    void cancel();

private:
    friend class BuildTaskProvider;
    void run(RawrXD::Toolchain::ToolchainBridge* bridge,
             DiagnosticsProvider* diags);

    BuildTaskConfig     m_config;
    uint64_t            m_id;
    std::atomic<BuildTaskState> m_state{BuildTaskState::Pending};
    BuildTaskResult     m_result;
    std::atomic<bool>   m_cancelled{false};
    ProgressCallback    m_progressCb;
    CompletionCallback  m_completionCb;
};


/* ---- BuildTaskProvider ---- */

class BuildTaskProvider {
public:
    using TaskListCallback = std::function<void(const std::vector<BuildTaskConfig>&)>;

    BuildTaskProvider();
    ~BuildTaskProvider();

    /* --- Lifecycle --- */
    bool initialize(RawrXD::Toolchain::ToolchainBridge* bridge,
                    DiagnosticsProvider* diagProvider);
    void shutdown();

    /* --- Task Management --- */
    uint64_t submitTask(const BuildTaskConfig& config);
    bool     cancelTask(uint64_t taskId);
    BuildTaskResult waitForTask(uint64_t taskId, uint32_t timeoutMs = 0);

    /* --- Convenience Builders --- */
    uint64_t buildFile(const std::string& sourceFile,
                       const std::string& outputPath = "");
    uint64_t buildProject(const std::vector<std::string>& sourceFiles,
                          const std::string& outputPath);
    uint64_t cleanBuild(const std::string& outputDir);

    /* --- Auto-detect tasks from workspace --- */
    std::vector<BuildTaskConfig> detectTasks(const std::string& workspaceRoot);

    /* --- VS Code tasks.json generation --- */
    nlohmann::json generateTasksJson(const std::string& workspaceRoot);

    /* --- Status / Metrics --- */
    BuildTaskState getTaskState(uint64_t taskId) const;
    nlohmann::json getStatus() const;
    nlohmann::json getMetrics() const;

    /* --- Callbacks --- */
    void setTaskListCallback(TaskListCallback cb) { m_taskListCb = std::move(cb); }

private:
    RawrXD::Toolchain::ToolchainBridge*         m_bridge   = nullptr;
    DiagnosticsProvider*                         m_diagProv = nullptr;
    std::atomic<bool>                            m_initialized{false};

    /* Task registry */
    mutable std::mutex                           m_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<BuildTask>> m_tasks;
    std::atomic<uint64_t>                        m_nextId{1};

    /* Worker thread pool (single for now) */
    std::thread                                  m_worker;
    std::vector<uint64_t>                        m_queue;
    std::mutex                                   m_queueMutex;
    std::condition_variable                      m_queueCv;
    std::atomic<bool>                            m_shutdown{false};

    TaskListCallback                             m_taskListCb;

    /* Stats */
    struct Stats {
        uint64_t totalBuilds    = 0;
        uint64_t successBuilds  = 0;
        uint64_t failedBuilds   = 0;
        double   avgBuildTimeMs = 0.0;
    };
    mutable std::mutex                           m_statsMutex;
    Stats                                        m_stats;

    void workerLoop();
    void runTask(BuildTask* task);
    std::string inferOutputPath(const std::string& sourceFile);
};

} // namespace IDE
} // namespace RawrXD
