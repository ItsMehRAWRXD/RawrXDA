#pragma once
/**
 * @file build_manager.h
 * @brief Build system integration for CMake, MSBuild, Ninja
 * Batch 3 - Item 35: Build system hooks
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <optional>
#include <future>

namespace RawrXD::BuildSystem {

enum class BuildTool {
    CMake,
    MSBuild,
    Ninja,
    Make,
    Custom
};

enum class BuildConfiguration {
    Debug,
    Release,
    RelWithDebInfo,
    MinSizeRel
};

enum class BuildStatus {
    Idle,
    Configuring,
    Building,
    Linking,
    Completed,
    Failed,
    Cancelled
};

struct BuildTarget {
    std::string name;
    std::string outputPath;
    std::vector<std::string> sourceFiles;
    std::vector<std::string> dependencies;
    bool isExecutable;
    bool isDefault;
};

struct BuildTask {
    std::string name;
    std::string command;
    std::vector<std::string> args;
    std::string workingDirectory;
    std::vector<std::string> dependencies;
    bool parallelizable;
};

struct BuildDiagnostic {
    enum Severity { Info, Warning, Error, Fatal };
    Severity severity;
    std::string filePath;
    uint32_t line;
    uint32_t column;
    std::string message;
    std::string code;
};

struct BuildProgress {
    BuildStatus status;
    std::string currentTask;
    size_t completedTasks;
    size_t totalTasks;
    size_t errors;
    size_t warnings;
    std::vector<BuildDiagnostic> diagnostics;
};

class BuildManager {
public:
    BuildManager();
    ~BuildManager();

    // Project detection
    bool detectBuildSystem(const std::string& projectRoot);
    BuildTool getDetectedTool() const;
    std::vector<BuildTarget> discoverTargets();

    // Configuration
    bool configure(const std::string& buildDirectory,
                   BuildConfiguration config = BuildConfiguration::Release,
                   const std::map<std::string, std::string>& options = {});
    bool configureCMake(const std::string& generator,
                        const std::string& toolchain = "");
    bool configureMSBuild(const std::string& platform = "x64");
    bool configureNinja(const std::string& toolchainFile = "");

    // Building
    bool build(const std::string& target = "");
    bool buildTarget(const std::string& targetName);
    bool clean();
    bool rebuild(const std::string& target = "");
    void cancelBuild();

    // Task management
    void addCustomTask(const BuildTask& task);
    bool runTask(const std::string& taskName);
    std::vector<std::string> getAvailableTasks() const;

    // Progress & diagnostics
    using ProgressCallback = std::function<void(const BuildProgress&)>;
    using DiagnosticCallback = std::function<void(const BuildDiagnostic&)>;
    void setProgressCallback(ProgressCallback callback);
    void setDiagnosticCallback(DiagnosticCallback callback);
    BuildProgress getCurrentProgress() const;
    std::vector<BuildDiagnostic> getDiagnostics() const;
    void clearDiagnostics();

    // Incremental builds
    bool needsRebuild(const std::string& target) const;
    std::vector<std::string> getModifiedFiles() const;
    void touchFile(const std::string& filePath);

    // Build cache
    void enableBuildCache(bool enabled);
    bool isBuildCacheEnabled() const;
    void clearBuildCache();

    // Environment
    void setEnvironmentVariable(const std::string& name, const std::string& value);
    void setParallelJobs(uint32_t jobs);
    void setVerboseOutput(bool verbose);

private:
    BuildTool m_tool;
    std::string m_projectRoot;
    std::string m_buildDirectory;
    BuildConfiguration m_config;
    std::atomic<BuildStatus> m_status{BuildStatus::Idle};
    std::atomic<bool> m_cancelled{false};
    std::atomic<uint32_t> m_parallelJobs{0};
    bool m_verbose{false};
    bool m_buildCache{false};

    ProgressCallback m_progressCallback;
    DiagnosticCallback m_diagnosticCallback;
    std::vector<BuildDiagnostic> m_diagnostics;
    std::map<std::string, BuildTask> m_customTasks;
    mutable std::mutex m_mutex;

    bool executeBuildCommand(const std::string& command,
                             const std::vector<std::string>& args,
                             const std::string& workingDir);
    void parseOutput(const std::string& output);
    BuildDiagnostic parseDiagnostic(const std::string& line);
};

// Global instance
BuildManager& getBuildManager();

// Utility functions
std::string buildToolToString(BuildTool tool);
std::string buildConfigToString(BuildConfiguration config);
BuildConfiguration stringToBuildConfig(const std::string& str);

} // namespace RawrXD::BuildSystem
