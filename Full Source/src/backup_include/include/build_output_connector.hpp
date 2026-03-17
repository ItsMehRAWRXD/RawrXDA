/**
 * @file build_output_connector.hpp
 * @brief Connects MASM build process output to Problems Panel in real-time
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

typedef std::vector<std::string> stringList;

class ProblemsPanel;

/**
 * @struct BuildError
 * @brief Represents a single build error/warning
 */
struct BuildError {
    std::string file;
    int line = 0;
    int column = 0;
    std::string severity;   // "error", "warning", "info"
    std::string code;       // e.g., "ML2005", "LNK1104"
    std::string message;
    std::string fullText;   // Original line from output
};

/**
 * @struct BuildConfiguration
 * @brief Build system configuration
 */
struct BuildConfiguration {
    std::string buildTool = "ml64.exe";  // ml64.exe, cl.exe, link.exe, etc.
    std::string sourceFile;
    std::string outputFile;
    std::stringList includePaths;
    std::stringList libraryPaths;
    std::stringList defines;
    std::stringList additionalFlags;
    std::string workingDirectory;
    int timeoutMs = 120000;  // 2 minutes default
};

/**
 * @class BuildOutputConnector
 * @brief Connects build process stdout/stderr to Problems Panel in real-time
 *
 * Features:
 * - Real-time streaming of build output
 * - Incremental error parsing as output arrives
 * - Support for MASM (ml64), C/C++ (cl.exe), linker (link.exe)
 * - Progress reporting
 * - Build cancellation
 * - Build history tracking
 */
class BuildOutputConnector  {

public:
    enum BuildState {
        Idle,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    explicit BuildOutputConnector(ProblemsPanel* problemsPanel);
    ~BuildOutputConnector();

    // Build control
    bool startBuild(const BuildConfiguration& config);
    void cancelBuild();
    
    // State queries
    BuildState getBuildState() const { return m_state; }
    bool isBuilding() const { return m_state == Running; }
    int getErrorCount() const { return m_errorCount; }
    int getWarningCount() const { return m_warningCount; }
    std::string getBuildOutput() const { return m_fullOutput; }
    
    // Configuration
    void setProblemsPanel(ProblemsPanel* panel);
    void setAutoScrollOutput(bool enabled) { m_autoScrollOutput = enabled; }
    void setRealTimeUpdates(bool enabled) { m_realTimeUpdates = enabled; }
    
    // Build history
    struct BuildHistoryEntry {
        // DateTime timestamp;
        BuildConfiguration config;
        BuildState result;
        int errorCount;
        int warningCount;
        int64_t durationMs;
        std::string output;
    };
    
    std::vector<BuildHistoryEntry> getBuildHistory(int maxEntries = 50) const;
    void clearBuildHistory();

\npublic:\n    void buildStarted(const std::string& sourceFile);
    void buildProgress(int percentage, const std::string& status);
    void buildOutputReceived(const std::string& output);
    void buildErrorDetected(const BuildError& error);
    void buildCompleted(bool success, int errorCount, int warningCount, int64_t durationMs);
    void buildCancelled();
    void buildFailed(const std::string& error);
    
    // Real-time diagnostics
    void diagnosticAdded(const std::string& file, int line, const std::string& severity, const std::string& message);

\nprivate:\n    void onProcessStarted();
    void onProcessFinished(int exitCode, void*::ExitStatus exitStatus);
    void onProcessError(void*::ProcessError error);
    void onStdoutReadyRead();
    void onStderrReadyRead();
    void updateProgress();

private:
    void processOutputLine(const std::string& line, bool isStderr);
    void parseErrorLine(const std::string& line);
    BuildError parseMASMError(const std::string& line);
    BuildError parseCppError(const std::string& line);
    BuildError parseLinkerError(const std::string& line);
    BuildError parseGenericError(const std::string& line);
    
    void sendToProblemsPanel(const BuildError& error);
    void updateBuildProgress();
    void recordBuildInHistory(BuildState result, int64_t durationMs);
    
    ProblemsPanel* m_problemsPanel;
    void** m_buildProcess;
    BuildConfiguration m_currentConfig;
    BuildState m_state;
    
    // Output buffering
    std::string m_fullOutput;
    std::string m_stdoutBuffer;
    std::string m_stderrBuffer;
    
    // Statistics
    int m_errorCount = 0;
    int m_warningCount = 0;
    int m_linesProcessed = 0;
    // DateTime m_buildStartTime;
    
    // Configuration
    bool m_autoScrollOutput = true;
    bool m_realTimeUpdates = true;
    
    // Progress tracking
    // Timer m_progressTimer;
    int m_estimatedTotalLines = 100;
    
    // Build history
    std::vector<BuildHistoryEntry> m_buildHistory;
    int m_maxHistoryEntries = 100;
};

/**
 * @class BuildManager
 * @brief High-level build manager with support for multiple build systems
 */
class BuildManager  {

public:
    enum BuildSystem {
        MASM,
        MSVC,
        GCC,
        Clang,
        CMake,
        Make,
        Ninja
    };

    explicit BuildManager(ProblemsPanel* problemsPanel);
    ~BuildManager();

    // Build operations
    bool buildFile(const std::string& sourceFile, BuildSystem system = MASM);
    bool buildProject(const std::string& projectPath, BuildSystem system = CMake);
    bool rebuildAll();
    bool cleanBuild();
    void cancelBuild();
    
    // Configuration
    void setProblemsPanel(ProblemsPanel* panel);
    void setBuildSystem(BuildSystem system);
    void setOutputDirectory(const std::string& dir) { m_outputDirectory = dir; }
    void setVerboseOutput(bool verbose) { m_verboseOutput = verbose; }
    
    // Auto-detect build system from file extension
    BuildSystem detectBuildSystem(const std::string& filePath);
    
    // Build configuration presets
    BuildConfiguration getDefaultConfig(BuildSystem system, const std::string& sourceFile);
    void saveConfiguration(const std::string& name, const BuildConfiguration& config);
    BuildConfiguration loadConfiguration(const std::string& name);
    std::stringList getAvailableConfigurations() const;

\npublic:\n    void buildStarted(const std::string& target);
    void buildProgress(int percentage);
    void buildCompleted(bool success);
    void buildSystemDetected(BuildSystem system);

private:
    BuildConfiguration createMASMConfig(const std::string& sourceFile);
    BuildConfiguration createMSVCConfig(const std::string& sourceFile);
    BuildConfiguration createCMakeConfig(const std::string& projectPath);
    
    std::string findBuildTool(BuildSystem system);
    std::stringList getStandardIncludePaths(BuildSystem system);
    std::stringList getStandardLibraryPaths(BuildSystem system);
    
    BuildOutputConnector* m_connector;
    BuildSystem m_currentBuildSystem;
    std::string m_outputDirectory;
    bool m_verboseOutput = false;
    
    // Configuration storage
    std::map<std::string, BuildConfiguration> m_savedConfigurations;
};

