/**
 * @file execution_visualizer.h
 * @brief Enterprise Execution Visualization - Workflow progress tracking system
 * 
 * Provides:
 * - Real-time step-by-step workflow visualization
 * - Timeline highlighting for execution bottlenecks
 * - Dependency graph representation
 * - Status indicators for active tasks
 * - Detailed logs per execution step
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QTimer>
#include <memory>

// Forward declarations
class QListWidget;
class QProgressBar;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QTextEdit;
class QSplitter;

/**
 * @enum StepStatus
 * @brief Status of an execution step
 */
enum class StepStatus {
    Pending,
    Running,
    Success,
    Failed,
    Skipped,
    Retrying
};

/**
 * @struct ExecutionStep
 * @brief Metadata for a single execution step
 */
struct ExecutionStep {
    QString id;
    QString parentId;
    QString description;
    QString type;
    StepStatus status;
    QDateTime startTime;
    QDateTime endTime;
    qint64 durationMs;
    QString output;
    QString error;
    QJsonObject metadata;
    int retryCount = 0;
};

/**
 * @class ExecutionVisualizer
 * @brief enterprise workflow progress visualization dashboard
 */
class ExecutionVisualizer : public QWidget {
    Q_OBJECT

public:
    explicit ExecutionVisualizer(QWidget* parent = nullptr);
    ~ExecutionVisualizer() override;

    void initialize();
    
    // Workflow lifecycle
    void startWorkflow(const QString& id, const QString& goal);
    void updateStep(const QString& stepId, StepStatus status, const QString& output = "");
    void addStep(const ExecutionStep& step);
    void finishWorkflow(bool success, const QString& finalSummary = "");

public slots:
    // Receiver slots for AgenticExecutor signals
    void onStepStarted(const QString& description);
    void onStepCompleted(const QString& description, bool success);
    void onExecutionPhaseChanged(const QString& phase);
    void onTaskProgress(int current, int total);
    void onLogMessage(const QString& message);
    void onErrorOccurred(const QString& error);

private:
    void setupUI();
    void updateTimeline();
    void updateProgressOverview();
    void updateDetailPanel(const QString& stepId);
    
    QString statusToString(StepStatus status) const;
    QColor statusToColor(StepStatus status) const;

    // UI Components
    QLabel* m_workflowTitleLabel = nullptr;
    QLabel* m_currentPhaseLabel = nullptr;
    QProgressBar* m_overallProgressBar = nullptr;
    QTreeWidget* m_stepTree = nullptr;
    QTextEdit* m_stepDetailLog = nullptr;
    QTableWidget* m_metricsTable = nullptr;
    
    // Data
    QString m_activeWorkflowId;
    QMap<QString, ExecutionStep> m_steps;
    QStringList m_stepIds; // For ordering
    int m_currentStepIndex = -1;
    QDateTime m_workflowStartTime;
};
