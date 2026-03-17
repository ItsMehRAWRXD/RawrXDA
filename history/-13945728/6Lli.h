/**
 * @file build_system_widget.h
 * @brief Production implementation of BuildSystemWidget
 * 
 * Provides a fully functional build system interface including:
 * - Build configuration management (Debug, Release, etc.)
 * - Build target selection
 * - Real-time build output with error parsing
 * - Build progress tracking
 * - CMake/QMake/Custom build system support
 * 
 * Per AI Toolkit Production Readiness Instructions:
 * - NO SIMPLIFICATIONS - all logic must remain intact
 * - Full structured logging for observability
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QTreeWidget>
#include <QToolBar>
#include <QMenu>
#include <QAction>

class BuildSystemWidget : public QWidget {
    Q_OBJECT

public:
    enum class BuildConfig {
        Debug,
        Release,
        RelWithDebInfo,
        MinSizeRel,
        Custom
    };
    Q_ENUM(BuildConfig)

    enum class BuildSystem {
        CMake,
        QMake,
        Make,
        MSBuild,
        Ninja,
        Custom
    };
    Q_ENUM(BuildSystem)

    struct BuildTarget {
        QString name;
        QString path;
        QString description;
        bool isDefault;
    };

    struct BuildError {
        QString file;
        int line;
        int column;
        QString message;
        QString severity;  // error, warning, note
    };

    explicit BuildSystemWidget(QWidget* parent = nullptr);
    ~BuildSystemWidget() override;

    // Configuration
    void setProjectPath(const QString& path);
    void setBuildSystem(BuildSystem system);
    void setBuildConfig(BuildConfig config);
    void setCustomBuildCommand(const QString& command);
    
    // Build control
    void startBuild();
    void stopBuild();
    void cleanBuild();
    void rebuildAll();
    
    // Target management
    void addBuildTarget(const BuildTarget& target);
    void removeBuildTarget(const QString& name);
    QList<BuildTarget> getTargets() const;
    void setActiveTarget(const QString& name);
    
    // Status
    bool isBuilding() const;
    int getLastExitCode() const;
    QList<BuildError> getLastErrors() const;
    qint64 getLastBuildDuration() const;

signals:
    void buildStarted();
    void buildProgress(int percent, const QString& stage);
    void buildOutput(const QString& line);
    void buildError(const BuildError& error);
    void buildFinished(bool success, int exitCode);
    void targetChanged(const QString& target);
    void configChanged(BuildConfig config);
    void errorDoubleClicked(const QString& file, int line, int column);

public slots:
    void onBuildButtonClicked();
    void onCleanButtonClicked();
    void onRebuildButtonClicked();
    void onConfigChanged(int index);
    void onTargetChanged(int index);
    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void clearOutput();
    void copyOutput();
    void exportBuildLog();

private:
    void setupUI();
    void setupToolbar();
    void setupConnections();
    void detectBuildSystem();
    void loadBuildTargets();
    void parseBuildOutput(const QString& line);
    void parseErrorLine(const QString& line);
    void updateProgress(const QString& line);
    QString getBuildCommand() const;
    QStringList getBuildArguments() const;
    void appendOutput(const QString& text, const QString& color = QString());
    void highlightError(const BuildError& error);
    int countErrors() const;
    int countWarnings() const;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QComboBox* m_configCombo;
    QComboBox* m_targetCombo;
    QPushButton* m_buildButton;
    QPushButton* m_stopButton;
    QPushButton* m_cleanButton;
    QPushButton* m_rebuildButton;
    QSplitter* m_splitter;
    QTextEdit* m_outputView;
    QTreeWidget* m_errorTree;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // Build process
    QProcess* m_buildProcess;
    bool m_isBuilding;
    int m_lastExitCode;
    QElapsedTimer m_buildTimer;
    qint64 m_lastBuildDuration;

    // Configuration
    QString m_projectPath;
    BuildSystem m_buildSystem;
    BuildConfig m_buildConfig;
    QString m_customCommand;
    QList<BuildTarget> m_targets;
    QString m_activeTarget;
    QList<BuildError> m_errors;

    // File watching
    QFileSystemWatcher* m_fileWatcher;
};

