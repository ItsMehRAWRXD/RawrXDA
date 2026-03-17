/*==========================================================================
 * RawrXD Build Task Provider — Implementation
 *
 * Manages build task lifecycle:
 *  submit → queue → run (via ToolchainBridge) → publish diagnostics → done
 *
 * Also generates VS Code tasks.json for external tool integration.
 *=========================================================================*/

#include "build_task_provider.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <sstream>
#include <filesystem>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace RawrXD {
namespace IDE {

/* =========================================================================
 * BuildTask
 * ========================================================================= */

BuildTask::BuildTask(const BuildTaskConfig& config, uint64_t id)
    : m_config(config), m_id(id) {
    if (m_config.label.empty()) {
        m_config.label = "Build #" + std::to_string(id);
    }
}

BuildTask::~BuildTask() = default;

void BuildTask::cancel() {
    m_cancelled.store(true);
    if (m_state.load() == BuildTaskState::Pending) {
        m_state.store(BuildTaskState::Cancelled);
        m_result.state = BuildTaskState::Cancelled;
        if (m_completionCb) m_completionCb(m_result);
    }
}

void BuildTask::run(RawrXD::Toolchain::ToolchainBridge* bridge,
                     DiagnosticsProvider* diags) {
    if (m_cancelled.load()) {
        m_state.store(BuildTaskState::Cancelled);
        m_result.state = BuildTaskState::Cancelled;
        return;
    }

    m_state.store(BuildTaskState::Running);
    auto t0 = std::chrono::high_resolution_clock::now();

    if (m_progressCb) m_progressCb("Starting build...", 0);

    bool success = true;

    switch (m_config.kind) {
    case BuildTaskKind::Assemble:
    case BuildTaskKind::CompileCpp:
    case BuildTaskKind::FullBuild:
    {
        for (size_t i = 0; i < m_config.sourceFiles.size(); i++) {
            if (m_cancelled.load()) {
                m_state.store(BuildTaskState::Cancelled);
                m_result.state = BuildTaskState::Cancelled;
                return;
            }

            const auto& src = m_config.sourceFiles[i];
            int pct = static_cast<int>((i * 100) / m_config.sourceFiles.size());
            if (m_progressCb)
                m_progressCb("Building " + src, pct);

            RawrXD::Toolchain::BuildConfig cfg;
            cfg.sourceFile = src;
            cfg.outputPath = m_config.outputPath.empty() ?
                             "" : m_config.outputPath;
            cfg.entryPoint = m_config.entryPoint;
            cfg.subsystem  = m_config.subsystem;
            cfg.imageBase  = m_config.imageBase;
            cfg.debugInfo  = m_config.debugInfo;
            cfg.verbose    = m_config.verbose;

            auto result = bridge->build(cfg);

            m_result.diagnostics.insert(m_result.diagnostics.end(),
                                        result.diagnostics.begin(),
                                        result.diagnostics.end());
            m_result.errorCount   += result.errorCount;
            m_result.warningCount += result.warningCount;

            if (!result.success) success = false;
            if (result.success && !result.outputPath.empty()) {
                m_result.outputPath = result.outputPath;
                m_result.outputSize = result.outputSize;
            }

            /* Push diagnostics to the diagnostics provider for LSP */
            if (diags && !result.diagnostics.empty()) {
                /* Force re-analysis to pick up build errors */
                auto lspDiags = diags->getDiagnostics(src);
                /* Diagnostics are auto-published by the bridge → diags flow */
            }
        }
        break;
    }

    case BuildTaskKind::Link:
    {
        if (m_progressCb) m_progressCb("Linking...", 50);

        /* Multi-file link: build each, then link */
        std::vector<RawrXD::Toolchain::BuildConfig> cfgs;
        for (const auto& src : m_config.sourceFiles) {
            RawrXD::Toolchain::BuildConfig c;
            c.sourceFile = src;
            c.outputPath = m_config.outputPath;
            c.entryPoint = m_config.entryPoint;
            c.subsystem  = m_config.subsystem;
            c.imageBase  = m_config.imageBase;
            cfgs.push_back(c);
        }

        auto result = bridge->buildProject(cfgs);
        m_result.diagnostics.insert(m_result.diagnostics.end(),
                                    result.diagnostics.begin(),
                                    result.diagnostics.end());
        m_result.errorCount   += result.errorCount;
        m_result.warningCount += result.warningCount;
        if (!result.success) success = false;
        m_result.outputPath = result.outputPath;
        m_result.outputSize = result.outputSize;
        break;
    }

    case BuildTaskKind::Clean:
    {
        if (m_progressCb) m_progressCb("Cleaning...", 10);

        /* Remove .obj and .exe files in output directory */
        for (const auto& src : m_config.sourceFiles) {
            std::string objPath = src;
            size_t dot = objPath.rfind('.');
            if (dot != std::string::npos)
                objPath = objPath.substr(0, dot) + ".obj";
            DeleteFileA(objPath.c_str());
        }
        if (!m_config.outputPath.empty()) {
            DeleteFileA(m_config.outputPath.c_str());
        }
        if (m_progressCb) m_progressCb("Clean complete", 100);
        break;
    }

    case BuildTaskKind::Rebuild:
    {
        /* Clean then build */
        if (m_progressCb) m_progressCb("Cleaning...", 5);
        for (const auto& src : m_config.sourceFiles) {
            std::string objPath = src;
            size_t dot = objPath.rfind('.');
            if (dot != std::string::npos)
                objPath = objPath.substr(0, dot) + ".obj";
            DeleteFileA(objPath.c_str());
        }
        if (!m_config.outputPath.empty()) {
            DeleteFileA(m_config.outputPath.c_str());
        }

        if (m_progressCb) m_progressCb("Rebuilding...", 10);

        for (size_t i = 0; i < m_config.sourceFiles.size(); i++) {
            if (m_cancelled.load()) {
                m_state.store(BuildTaskState::Cancelled);
                m_result.state = BuildTaskState::Cancelled;
                return;
            }

            RawrXD::Toolchain::BuildConfig cfg;
            cfg.sourceFile = m_config.sourceFiles[i];
            cfg.outputPath = m_config.outputPath;
            cfg.entryPoint = m_config.entryPoint;
            cfg.subsystem  = m_config.subsystem;
            cfg.imageBase  = m_config.imageBase;
            cfg.debugInfo  = m_config.debugInfo;
            cfg.verbose    = m_config.verbose;

            auto result = bridge->build(cfg);
            m_result.diagnostics.insert(m_result.diagnostics.end(),
                                        result.diagnostics.begin(),
                                        result.diagnostics.end());
            m_result.errorCount += result.errorCount;
            m_result.warningCount += result.warningCount;
            if (!result.success) success = false;
            m_result.outputPath = result.outputPath;
            m_result.outputSize = result.outputSize;
        }
        break;
    }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    m_result.elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (success) {
        m_state.store(BuildTaskState::Succeeded);
        m_result.state = BuildTaskState::Succeeded;
        if (m_progressCb) m_progressCb("Build succeeded", 100);
    } else {
        m_state.store(BuildTaskState::Failed);
        m_result.state = BuildTaskState::Failed;
        if (m_progressCb)
            m_progressCb("Build failed (" + std::to_string(m_result.errorCount) + " errors)", 100);
    }

    if (m_completionCb) m_completionCb(m_result);
}


/* =========================================================================
 * BuildTaskProvider
 * ========================================================================= */

BuildTaskProvider::BuildTaskProvider()  = default;
BuildTaskProvider::~BuildTaskProvider() { shutdown(); }

bool BuildTaskProvider::initialize(RawrXD::Toolchain::ToolchainBridge* bridge,
                                    DiagnosticsProvider* diagProvider) {
    if (!bridge) return false;
    m_bridge   = bridge;
    m_diagProv = diagProvider;
    m_shutdown.store(false);

    /* Start worker thread */
    m_worker = std::thread([this]() { workerLoop(); });

    m_initialized.store(true);
    spdlog::info("[BuildTaskProvider] Initialized");
    return true;
}

void BuildTaskProvider::shutdown() {
    if (!m_initialized.exchange(false)) return;

    /* Signal worker to exit */
    m_shutdown.store(true);
    m_queueCv.notify_all();

    if (m_worker.joinable()) m_worker.join();

    /* Cancel pending tasks */
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& [id, task] : m_tasks) {
        if (task->getState() == BuildTaskState::Pending) {
            task->cancel();
        }
    }
    m_tasks.clear();

    spdlog::info("[BuildTaskProvider] Shutdown");
}

/* ---- Task Submission ---- */

uint64_t BuildTaskProvider::submitTask(const BuildTaskConfig& config) {
    uint64_t id = m_nextId.fetch_add(1);
    auto task = std::make_unique<BuildTask>(config, id);

    spdlog::info("[BuildTaskProvider] Task #{} submitted: {} ({} files)",
                 id, config.label, config.sourceFiles.size());

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_tasks[id] = std::move(task);
    }

    {
        std::lock_guard<std::mutex> qlk(m_queueMutex);
        m_queue.push_back(id);
    }
    m_queueCv.notify_one();

    return id;
}

bool BuildTaskProvider::cancelTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return false;
    it->second->cancel();
    return true;
}

BuildTaskResult BuildTaskProvider::waitForTask(uint64_t taskId, uint32_t timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : UINT32_MAX);

    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_tasks.find(taskId);
            if (it == m_tasks.end()) return {};
            auto state = it->second->getState();
            if (state == BuildTaskState::Succeeded ||
                state == BuildTaskState::Failed ||
                state == BuildTaskState::Cancelled) {
                return it->second->getResult();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /* Timeout */
    return {};
}

/* ---- Convenience ---- */

uint64_t BuildTaskProvider::buildFile(const std::string& sourceFile,
                                       const std::string& outputPath) {
    BuildTaskConfig cfg;
    cfg.kind = BuildTaskKind::FullBuild;
    cfg.label = "Build " + fs::path(sourceFile).filename().string();
    cfg.sourceFiles = {sourceFile};
    cfg.outputPath  = outputPath.empty() ? inferOutputPath(sourceFile) : outputPath;
    return submitTask(cfg);
}

uint64_t BuildTaskProvider::buildProject(const std::vector<std::string>& sourceFiles,
                                           const std::string& outputPath) {
    BuildTaskConfig cfg;
    cfg.kind = BuildTaskKind::FullBuild;
    cfg.label = "Build Project (" + std::to_string(sourceFiles.size()) + " files)";
    cfg.sourceFiles = sourceFiles;
    cfg.outputPath  = outputPath;
    return submitTask(cfg);
}

uint64_t BuildTaskProvider::cleanBuild(const std::string& outputDir) {
    BuildTaskConfig cfg;
    cfg.kind = BuildTaskKind::Clean;
    cfg.label = "Clean " + outputDir;
    cfg.outputPath = outputDir;
    return submitTask(cfg);
}

/* ---- Workspace Task Auto-Detection ---- */

std::vector<BuildTaskConfig> BuildTaskProvider::detectTasks(const std::string& root) {
    std::vector<BuildTaskConfig> detected;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();

            /* Transform to lowercase for comparison */
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".asm") {
                BuildTaskConfig cfg;
                cfg.kind = BuildTaskKind::FullBuild;
                cfg.label = "Build " + entry.path().filename().string();
                cfg.sourceFiles = {entry.path().string()};
                cfg.outputPath  = inferOutputPath(entry.path().string());
                detected.push_back(std::move(cfg));
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("[BuildTaskProvider] detectTasks error: {}", e.what());
    }

    if (m_taskListCb && !detected.empty()) {
        m_taskListCb(detected);
    }

    spdlog::info("[BuildTaskProvider] Detected {} build tasks in {}", detected.size(), root);
    return detected;
}

/* ---- VS Code tasks.json Generation ---- */

nlohmann::json BuildTaskProvider::generateTasksJson(const std::string& root) {
    auto detected = detectTasks(root);

    nlohmann::json tasks_json;
    tasks_json["version"] = "2.0.0";
    tasks_json["tasks"]   = nlohmann::json::array();

    /* Default build task */
    {
        nlohmann::json t;
        t["label"]   = "RawrXD: Build Active File (MASM64)";
        t["type"]    = "shell";
        t["command"] = "ml64";
        t["args"]    = nlohmann::json::array({
            "/c", "/Fo${fileDirname}\\${fileBasenameNoExtension}.obj",
            "${file}"
        });
        t["group"]   = { {"kind", "build"}, {"isDefault", true} };
        t["presentation"] = { {"reveal", "always"}, {"panel", "shared"} };
        t["problemMatcher"] = nlohmann::json::array({
            {
                {"owner", "masm64"},
                {"fileLocation", nlohmann::json::array({"relative", "${workspaceFolder}"})},
                {"pattern", {
                    {"regexp", R"(^(.+)\((\d+)\)\s*:\s*(error|warning)\s+(A\d+):\s*(.+)$)"},
                    {"file", 1},
                    {"line", 2},
                    {"severity", 3},
                    {"code", 4},
                    {"message", 5}
                }}
            }
        });
        tasks_json["tasks"].push_back(t);
    }

    /* Link task */
    {
        nlohmann::json t;
        t["label"]   = "RawrXD: Link (MSVC link.exe)";
        t["type"]    = "shell";
        t["command"] = "link";
        t["args"]    = nlohmann::json::array({
            "/SUBSYSTEM:CONSOLE",
            "/ENTRY:_start",
            "/OUT:${fileDirname}\\${fileBasenameNoExtension}.exe",
            "${fileDirname}\\${fileBasenameNoExtension}.obj",
            "kernel32.lib"
        });
        t["group"] = "build";
        t["presentation"] = { {"reveal", "always"}, {"panel", "shared"} };
        t["problemMatcher"] = nlohmann::json::array({
            {
                {"owner", "msvc-linker"},
                {"pattern", {
                    {"regexp", R"(^(.+?)\s*:\s*(error|warning)\s+(LNK\d+):\s*(.+)$)"},
                    {"file", 1},
                    {"severity", 2},
                    {"code", 3},
                    {"message", 4}
                }}
            }
        });
        tasks_json["tasks"].push_back(t);
    }

    /* Full build (assemble + link) */
    {
        nlohmann::json t;
        t["label"]   = "RawrXD: Full Build (Assemble + Link)";
        t["type"]    = "shell";
        t["command"] = "powershell";
        t["args"]    = nlohmann::json::array({
            "-ExecutionPolicy", "Bypass", "-File",
            "${workspaceFolder}\\RawrXD-Build.ps1",
            "-Source", "${file}"
        });
        t["group"]   = { {"kind", "build"}, {"isDefault", false} };
        t["presentation"] = { {"reveal", "always"}, {"panel", "shared"} };
        t["dependsOrder"] = "sequence";
        tasks_json["tasks"].push_back(t);
    }

    /* Clean task */
    {
        nlohmann::json t;
        t["label"]   = "RawrXD: Clean";
        t["type"]    = "shell";
        t["command"] = "powershell";
        t["args"]    = nlohmann::json::array({
            "-Command",
            "Remove-Item '${workspaceFolder}\\*.obj','${workspaceFolder}\\*.exe' -ErrorAction SilentlyContinue -Force"
        });
        t["group"] = "build";
        tasks_json["tasks"].push_back(t);
    }

    /* Add detected per-file tasks */
    for (const auto& cfg : detected) {
        nlohmann::json t;
        t["label"]   = cfg.label;
        t["type"]    = "shell";
        t["command"] = "ml64";
        t["args"]    = nlohmann::json::array();
        t["args"].push_back("/c");
        for (const auto& src : cfg.sourceFiles) {
            t["args"].push_back(src);
        }
        t["group"] = "build";
        tasks_json["tasks"].push_back(t);
    }

    return tasks_json;
}

/* ---- Status / Metrics ---- */

BuildTaskState BuildTaskProvider::getTaskState(uint64_t taskId) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return BuildTaskState::Pending;
    return it->second->getState();
}

nlohmann::json BuildTaskProvider::getStatus() const {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(m_mutex);
    j["total_tasks"]  = m_tasks.size();

    int pending = 0, running = 0, succeeded = 0, failed = 0;
    for (const auto& [id, t] : m_tasks) {
        switch (t->getState()) {
        case BuildTaskState::Pending:   pending++;   break;
        case BuildTaskState::Running:   running++;   break;
        case BuildTaskState::Succeeded: succeeded++; break;
        case BuildTaskState::Failed:    failed++;    break;
        default: break;
        }
    }
    j["pending"]   = pending;
    j["running"]   = running;
    j["succeeded"] = succeeded;
    j["failed"]    = failed;

    return j;
}

nlohmann::json BuildTaskProvider::getMetrics() const {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(m_statsMutex);
    j["total_builds"]     = m_stats.totalBuilds;
    j["success_builds"]   = m_stats.successBuilds;
    j["failed_builds"]    = m_stats.failedBuilds;
    j["avg_build_time_ms"]= m_stats.avgBuildTimeMs;
    return j;
}

/* ---- Worker Thread ---- */

void BuildTaskProvider::workerLoop() {
    spdlog::info("[BuildTaskProvider] Worker thread started");

    while (!m_shutdown.load()) {
        uint64_t taskId = 0;

        {
            std::unique_lock<std::mutex> lk(m_queueMutex);
            m_queueCv.wait(lk, [this]() {
                return !m_queue.empty() || m_shutdown.load();
            });
            if (m_shutdown.load()) break;
            if (m_queue.empty()) continue;
            taskId = m_queue.front();
            m_queue.erase(m_queue.begin());
        }

        BuildTask* task = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_tasks.find(taskId);
            if (it != m_tasks.end()) {
                task = it->second.get();
            }
        }

        if (task) {
            runTask(task);
        }
    }

    spdlog::info("[BuildTaskProvider] Worker thread exiting");
}

void BuildTaskProvider::runTask(BuildTask* task) {
    spdlog::info("[BuildTaskProvider] Running task #{}: {}", task->getId(), task->getLabel());

    task->run(m_bridge, m_diagProv);

    /* Update stats */
    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.totalBuilds++;
        if (task->getState() == BuildTaskState::Succeeded)
            m_stats.successBuilds++;
        else if (task->getState() == BuildTaskState::Failed)
            m_stats.failedBuilds++;

        double total = m_stats.avgBuildTimeMs * (m_stats.totalBuilds - 1) +
                       task->getResult().elapsedMs;
        m_stats.avgBuildTimeMs = total / m_stats.totalBuilds;
    }

    spdlog::info("[BuildTaskProvider] Task #{} finished: {} ({:.1f}ms, {} errors, {} warnings)",
                 task->getId(),
                 task->getState() == BuildTaskState::Succeeded ? "SUCCESS" : "FAILED",
                 task->getResult().elapsedMs,
                 task->getResult().errorCount,
                 task->getResult().warningCount);
}

std::string BuildTaskProvider::inferOutputPath(const std::string& sourceFile) {
    fs::path p(sourceFile);
    return (p.parent_path() / p.stem()).string() + ".exe";
}

} // namespace IDE
} // namespace RawrXD
