#ifndef EXECAI_WIDGET_H
#define EXECAI_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QFileDialog>
#include <QProcess>
#include <QTimer>
#include <QCheckBox>
#include <QListWidget>
#include <QGroupBox>

class ExecAIWidget : public QWidget {
    Q_OBJECT

public:
    explicit ExecAIWidget(QWidget* parent = nullptr);
    ~ExecAIWidget();

private slots:
    void browseInputFiles(); // Changed to browse multiple
    void browseOutputDir();
    void startBatchProcessing();
    void processNextInQueue();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void updateProgress();
    void clearQueue();
    void onToggleGpu(bool checked);

private:
    void setupUI();
    void setupConnections();
    bool validateInputs();
    void runAnalyzer(const QString& input, const QString& output);
    void registerModelToRegistry(const QString& execPath);

    // UI Components
    QVBoxLayout* mainLayout;

    // Batch management
    QGroupBox* batchGroup;
    QListWidget* modelQueueList;
    QPushButton* addFilesButton;
    QPushButton* removeFilesButton;
    QPushButton* clearQueueButton;

    // Output settings
    QGroupBox* outputGroup;
    QLineEdit* outputDirEdit;
    QPushButton* outputDirButton;

    // Advanced features
    QGroupBox* advancedGroup;
    QCheckBox* gpuAccelCheck;
    QCheckBox* deepValidationCheck;
    QCheckBox* autoRegisterCheck;

    // Control buttons
    QHBoxLayout* controlLayout;
    QPushButton* processButton;
    QPushButton* cancelButton;

    // Progress and status
    QProgressBar* batchProgressBar;
    QProgressBar* currentProgressBar;
    QLabel* statusLabel;
    QTextEdit* logOutput;

    // Internal state
    QStringList workQueue;
    QString currentProcessingFile;
    int completedCount;
    int totalBatchSize;

    // Process management
    QProcess* analyzerProcess;
    QTimer* progressTimer;

    // Constants
    static const QString ANALYZER_EXE;
};

#endif // EXECAI_WIDGET_H
