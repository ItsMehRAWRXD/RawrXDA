#pragma once

// <QWidget> removed (Qt-free build)
// <QProcess> removed (Qt-free build)
// <QStringList> removed (Qt-free build)
// <QMap> removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QJsonObject> removed (Qt-free build)

QT_BEGIN_NAMESPACE
class QComboBox;
class QTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QAction;
class QLabel;
class QProgressBar;
class QSplitter;
class QTabWidget;
QT_END_NAMESPACE

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
class BuildSystemWidget  {
    /* Q_OBJECT */

public:
    explicit BuildSystemWidget(QWidget* parent = nullptr);
    ~BuildSystemWidget() override;

    // Project management
    void setProjectPath(const QString& path);
    QString projectPath() const { return m_projectPath; }
    
    // Build system configuration
    void setBuildSystem(const QString& system);
    QString buildSystem() const { return m_currentBuildSystem; }
    
    // Build configuration
    void setBuildConfig(const QString& config);
    QString buildConfig() const { return m_currentConfig; }
    
    // Build operations
    void startBuild();
    void stopBuild();
    void cleanBuild();
    void rebuildAll();
    
    // Build history
    QStringList buildHistory() const;
    void clearHistory();
    
    // Statistics
    struct BuildStats {
        int totalBuilds = 0;
        int successfulBuilds = 0;
        int failedBuilds = 0;
        QDateTime lastBuildTime;
        int lastBuildDuration = 0; // milliseconds
        int totalErrors = 0;
        int totalWarnings = 0;
    };
    BuildStats statistics() const { return m_stats; }

signals:
    void buildStarted(const QString& buildSystem, const QString& config);
    void buildFinished(bool success, int duration);
    void buildProgress(int percentage, const QString& message);
    void errorDetected(const QString& file, int line, const QString& message);
    void warningDetected(const QString& file, int line, const QString& message);
    void buildOutputReceived(const QString& output);

public slots:
    void configure();
    void generate();
    void install();
    void test();

private slots:
    void onBuildSystemChanged(int index);
    void onConfigChanged(int index);
    void onTargetChanged(int index);
    void onBuildButtonClicked();
    void onStopButtonClicked();
    void onCleanButtonClicked();
    void onRebuildButtonClicked();
    void onConfigureButtonClicked();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
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
    void parseBuildOutput(const QString& output, bool isError);
    void addBuildMessage(const QString& type, const QString& file, 
                        int line, const QString& message);
    void updateStatistics(bool success, int duration);
    void saveSettings();
    void loadSettings();
    QString detectProjectBuildSystem(const QString& path);
    QStringList getBuildCommand();
    QStringList getConfigureCommand();
    QStringList getCleanCommand();
    void highlightErrors(const QString& text);
    void logBuildEvent(const QString& event, const QJsonObject& data = QJsonObject());

private:
    // UI Components
    QComboBox* m_buildSystemCombo{nullptr};
    QComboBox* m_configCombo{nullptr};
    QComboBox* m_targetCombo{nullptr};
    QPushButton* m_buildButton{nullptr};
    QPushButton* m_stopButton{nullptr};
    QPushButton* m_cleanButton{nullptr};
    QPushButton* m_rebuildButton{nullptr};
    QPushButton* m_configureButton{nullptr};
    QTextEdit* m_outputText{nullptr};
    QTreeWidget* m_errorTree{nullptr};
    QLabel* m_statusLabel{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QToolBar* m_toolBar{nullptr};
    QSplitter* m_splitter{nullptr};
    QTabWidget* m_tabWidget{nullptr};
    
    // Build process
    QProcess* m_buildProcess{nullptr};
    bool m_isBuilding{false};
    QDateTime m_buildStartTime;
    
    // Configuration
    QString m_projectPath;
    QString m_currentBuildSystem{"CMake"};
    QString m_currentConfig{"Debug"};
    QString m_currentTarget{"all"};
    int m_parallelJobs{4};
    
    // Build data
    QStringList m_buildSystemsList{"CMake", "QMake", "Meson", "Ninja", "MSBuild", "Make", "Custom"};
    QStringList m_configsList{"Debug", "Release", "RelWithDebInfo", "MinSizeRel"};
    QMap<QString, QString> m_environmentVars;
    
    // Statistics and history
    BuildStats m_stats;
    QList<QPair<QDateTime, QString>> m_buildHistory;
    
    // Regex patterns for error detection
    QRegularExpression m_errorRegex;
    QRegularExpression m_warningRegex;
    QRegularExpression m_fileLineRegex;
};
