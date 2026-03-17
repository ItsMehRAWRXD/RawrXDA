#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QLineEdit>
#include <QThread>
#include <QMap>
#include <memory>

/**
 * @brief ProjectAnalysisDialog - Analyze project structure and statistics
 * 
 * Features:
 * - Recursive directory scanning
 * - File count, size, and line count analysis
 * - Extension breakdown with percentage
 * - Build artifact cleanup option
 * - Progress display
 * - Results export
 */
class ProjectAnalysisWorker : public QObject {
    Q_OBJECT

public:
    void analyzeProject(const QString& projectPath, bool excludeArtifacts);

signals:
    void progress(int current, int total);
    void resultsReady(const QString& results);
    void finished();
    void error(const QString& errorMsg);

private:
    struct FileStats {
        int count = 0;
        qint64 totalSize = 0;
        int totalLines = 0;
    };

    QString analyzeDirectory(const QString& path, QMap<QString, int>& extensionCounts, bool excludeArtifacts);
};

class ProjectAnalysisDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectAnalysisDialog(QWidget* parent = nullptr);
    ~ProjectAnalysisDialog();

private slots:
    void onBrowseClicked();
    void onAnalyzeClicked();
    void onAnalysisProgress(int current, int total);
    void onAnalysisResults(const QString& results);
    void onAnalysisFinished();
    void onAnalysisError(const QString& errorMsg);
    void onExportClicked();
    void onCleanupClicked();

private:
    void setupUI();
    void setupConnections();
    void enableAnalysisControls(bool enable);

    // UI Components
    QLineEdit* m_projectPathEdit;
    QPushButton* m_browseButton;
    QCheckBox* m_excludeArtifactsCheckbox;
    QPushButton* m_analyzeButton;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QTextEdit* m_resultsDisplay;
    QPushButton* m_exportButton;
    QPushButton* m_cleanupButton;

    // Worker
    ProjectAnalysisWorker* m_worker;
    QThread* m_workerThread;

    QString m_lastProjectPath;
    QString m_lastResults;
};
