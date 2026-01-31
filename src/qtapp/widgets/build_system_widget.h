#pragma once


/**
 * @brief Production-ready Build System Widget with comprehensive build support
 * 
 * Features:
 * - Multi-build system support (CMake, QMake, Meson, Ninja, MSBuild, Make)
 * - Build configuration management (Debug, Release, RelWithDebInfo, MinSizeRel)
 * - Real-time build output with syntax highlighting
 * - Error/warning detection and navigation
 * - Build statistics and timing
 * - Parallel build configuration
 * - Clean and rebuild operations
 * - Build target selection
 * - Environment variable management
 * - Build history tracking
 */
class BuildSystemWidget
{

public:
    explicit BuildSystemWidget(void* parent = nullptr);
    ~BuildSystemWidget() override;

    // Project management
    void setProjectPath(const std::string& path);
    std::string projectPath() const { return m_projectPath; }
    
    // Build system configuration
    void setBuildSystem(const std::string& system);
    std::string buildSystem() const { return m_currentBuildSystem; }
    
    // Build configuration
    void setBuildConfig(const std::string& config);
    std::string buildConfig() const { return m_currentConfig; }
    
    // Build operations
    void startBuild();
    void stopBuild();
    void cleanBuild();
    void rebuildAll();
    
    // Build status for UI integration
    void setBuildStatus(bool building);
    bool isBuildInProgress() const { return m_buildInProgress; }
    
    // Build history
    std::stringList buildHistory() const;
    void clearHistory();
    
    // Statistics
    struct BuildStats {
        int totalBuilds = 0;
        int successfulBuilds = 0;
        int failedBuilds = 0;
        // DateTime lastBuildTime;
        int lastBuildDuration = 0; // milliseconds
        int totalErrors = 0;
        int totalWarnings = 0;
    };
    BuildStats statistics() const { return m_stats; }
\npublic:\n    void buildStarted(const std::string& buildSystem, const std::string& config);
    void buildFinished(bool success, int duration);
    void buildProgress(int percentage, const std::string& message);
    void errorDetected(const std::string& file, int line, const std::string& message);
    void warningDetected(const std::string& file, int line, const std::string& message);
    void buildOutputReceived(const std::string& output);
\npublic:\n    void configure();
    void generate();
    void install();
    void test();
\nprivate:\n    void onBuildSystemChanged(int index);
    void onConfigChanged(int index);
    void onTargetChanged(int index);
    void onBuildButtonClicked();
    void onStopButtonClicked();
    void onCleanButtonClicked();
    void onRebuildButtonClicked();
    void onConfigureButtonClicked();
    void onProcessStarted();
    void onProcessFinished(int exitCode, void*::ExitStatus exitStatus);
    void onProcessError(void*::ProcessError error);
    void onProcessReadyReadStdOut();
    void onProcessReadyReadStdErr();
    void onBuildOutputItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onClearOutputClicked();
    void onSaveOutputClicked();
    void updateBuildTargets();
    void updateParallelJobs(int value);

private:
    void setupUI();
    void createToolBar();
    void createBuildOutputView();
    void createBuildConfiguration();
    void connectSignals();
    void loadBuildSystems();
    void loadBuildConfigs();
    void detectBuildSystem();
    void parseBuildOutput(const std::string& output, bool isError);
    void addBuildMessage(const std::string& type, const std::string& file, 
                        int line, const std::string& message);
    void updateStatistics(bool success, int duration);
    void saveSettings();
    void loadSettings();
    std::string detectProjectBuildSystem(const std::string& path);
    std::stringList getBuildCommand();
    std::stringList getConfigureCommand();
    std::stringList getCleanCommand();
    void highlightErrors(const std::string& text);
    void logBuildEvent(const std::string& event, const void*& data = void*());

private:
    // UI Components
    void* m_buildSystemCombo{nullptr};
    void* m_configCombo{nullptr};
    void* m_targetCombo{nullptr};
    void* m_buildButton{nullptr};
    void* m_stopButton{nullptr};
    void* m_cleanButton{nullptr};
    void* m_rebuildButton{nullptr};
    void* m_configureButton{nullptr};
    void* m_outputText{nullptr};
    QTreeWidget* m_errorTree{nullptr};
    void* m_statusLabel{nullptr};
    void* m_progressBar{nullptr};
    void* m_toolBar{nullptr};
    void* m_splitter{nullptr};
    void* m_tabWidget{nullptr};
    
    // Build process
    void** m_buildProcess{nullptr};
    bool m_isBuilding{false};
    bool m_buildInProgress{false};
    // DateTime m_buildStartTime;
    
    // Configuration
    std::string m_projectPath;
    std::string m_currentBuildSystem{"CMake"};
    std::string m_currentConfig{"Debug"};
    std::string m_currentTarget{"all"};
    int m_parallelJobs{4};
    
    // Build data
    std::stringList m_buildSystemsList{"CMake", "QMake", "Meson", "Ninja", "MSBuild", "Make", "Custom"};
    std::stringList m_configsList{"Debug", "Release", "RelWithDebInfo", "MinSizeRel"};
    std::map<std::string, std::string> m_environmentVars;
    
    // Statistics and history
    BuildStats m_stats;
    std::vector<std::pair<// DateTime, std::string>> m_buildHistory;
    
    // Regex patterns for error detection
    std::regex m_errorRegex;
    std::regex m_warningRegex;
    std::regex m_fileLineRegex;
};

