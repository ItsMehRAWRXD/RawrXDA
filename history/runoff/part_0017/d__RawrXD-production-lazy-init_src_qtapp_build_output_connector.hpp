/**
 * @file build_output_connector.hpp
 * @brief Connects MASM build process output to Problems Panel in real-time
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QTextStream;
QT_END_NAMESPACE

class ProblemsPanel;

/**
 * @struct BuildError
 * @brief Represents a single build error/warning
 */
struct BuildError {
    QString file;
    int line = 0;
    int column = 0;
    QString severity;   // "error", "warning", "info"
    QString code;       // e.g., "ML2005", "LNK1104"
    QString message;
    QString fullText;   // Original line from output
};

/**
 * @struct BuildConfiguration
 * @brief Build system configuration
 */
struct BuildConfiguration {
    QString buildTool = "ml64.exe";  // ml64.exe, cl.exe, link.exe, etc.
    QString sourceFile;
    QString outputFile;
    QStringList includePaths;
    QStringList libraryPaths;
    QStringList defines;
    QStringList additionalFlags;
    QString workingDirectory;
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
class BuildOutputConnector : public QObject {
    Q_OBJECT

public:
    enum BuildState {
        Idle,
        Running,
        Completed,
        Failed,
        Cancelled
    };
    Q_ENUM(BuildState)

    explicit BuildOutputConnector(ProblemsPanel* problemsPanel, QObject* parent = nullptr);
    ~BuildOutputConnector();

    // Build control
    bool startBuild(const BuildConfiguration& config);
    void cancelBuild();
    
    // State queries
    BuildState getBuildState() const { return m_state; }
    bool isBuilding() const { return m_state == Running; }
    int getErrorCount() const { return m_errorCount; }
    int getWarningCount() const { return m_warningCount; }
    QString getBuildOutput() const { return m_fullOutput; }
    
    // Configuration
    void setProblemsPanel(ProblemsPanel* panel);
    void setAutoScrollOutput(bool enabled) { m_autoScrollOutput = enabled; }
    void setRealTimeUpdates(bool enabled) { m_realTimeUpdates = enabled; }
    
    // Build history
    struct BuildHistoryEntry {
        QDateTime timestamp;
        BuildConfiguration config;
        BuildState result;
        int errorCount;
        int warningCount;
        qint64 durationMs;
        QString output;
    };
    
    QVector<BuildHistoryEntry> getBuildHistory(int maxEntries = 50) const;
    void clearBuildHistory();

signals:
    void buildStarted(const QString& sourceFile);
    void buildProgress(int percentage, const QString& status);
    void buildOutputReceived(const QString& output);
    void buildErrorDetected(const BuildError& error);
    void buildCompleted(bool success, int errorCount, int warningCount, qint64 durationMs);
    void buildCancelled();
    void buildFailed(const QString& error);
    
    // Real-time diagnostics
    void diagnosticAdded(const QString& file, int line, const QString& severity, const QString& message);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onStdoutReadyRead();
    void onStderrReadyRead();
    void updateProgress();

private:
    void processOutputLine(const QString& line, bool isStderr);
    void parseErrorLine(const QString& line);
    BuildError parseMASMError(const QString& line);
    BuildError parseCppError(const QString& line);
    BuildError parseLinkerError(const QString& line);
    BuildError parseGenericError(const QString& line);
    
    void sendToProblemsPanel(const BuildError& error);
    void updateBuildProgress();
    void recordBuildInHistory(BuildState result, qint64 durationMs);
    
    ProblemsPanel* m_problemsPanel;
    QProcess* m_buildProcess;
    BuildConfiguration m_currentConfig;
    BuildState m_state;
    
    // Output buffering
    QString m_fullOutput;
    QString m_stdoutBuffer;
    QString m_stderrBuffer;
    
    // Statistics
    int m_errorCount = 0;
    int m_warningCount = 0;
    int m_linesProcessed = 0;
    QDateTime m_buildStartTime;
    
    // Configuration
    bool m_autoScrollOutput = true;
    bool m_realTimeUpdates = true;
    
    // Progress tracking
    QTimer* m_progressTimer;
    int m_estimatedTotalLines = 100;
    
    // Build history
    QVector<BuildHistoryEntry> m_buildHistory;
    int m_maxHistoryEntries = 100;
};

/**
 * @class BuildManager
 * @brief High-level build manager with support for multiple build systems
 */
class BuildManager : public QObject {
    Q_OBJECT

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
    Q_ENUM(BuildSystem)

    explicit BuildManager(ProblemsPanel* problemsPanel, QObject* parent = nullptr);
    ~BuildManager();

    // Build operations
    bool buildFile(const QString& sourceFile, BuildSystem system = MASM);
    bool buildProject(const QString& projectPath, BuildSystem system = CMake);
    bool rebuildAll();
    bool cleanBuild();
    void cancelBuild();
    
    // Configuration
    void setProblemsPanel(ProblemsPanel* panel);
    void setBuildSystem(BuildSystem system);
    void setOutputDirectory(const QString& dir) { m_outputDirectory = dir; }
    void setVerboseOutput(bool verbose) { m_verboseOutput = verbose; }
    
    // Auto-detect build system from file extension
    BuildSystem detectBuildSystem(const QString& filePath);
    
    // Build configuration presets
    BuildConfiguration getDefaultConfig(BuildSystem system, const QString& sourceFile);
    void saveConfiguration(const QString& name, const BuildConfiguration& config);
    BuildConfiguration loadConfiguration(const QString& name);
    QStringList getAvailableConfigurations() const;

signals:
    void buildStarted(const QString& target);
    void buildProgress(int percentage);
    void buildCompleted(bool success);
    void buildSystemDetected(BuildSystem system);

private:
    BuildConfiguration createMASMConfig(const QString& sourceFile);
    BuildConfiguration createMSVCConfig(const QString& sourceFile);
    BuildConfiguration createCMakeConfig(const QString& projectPath);
    
    QString findBuildTool(BuildSystem system);
    QStringList getStandardIncludePaths(BuildSystem system);
    QStringList getStandardLibraryPaths(BuildSystem system);
    
    BuildOutputConnector* m_connector;
    BuildSystem m_currentBuildSystem;
    QString m_outputDirectory;
    bool m_verboseOutput = false;
    
    // Configuration storage
    QMap<QString, BuildConfiguration> m_savedConfigurations;
};

