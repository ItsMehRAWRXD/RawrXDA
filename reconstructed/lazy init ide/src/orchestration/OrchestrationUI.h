#pragma once

#include <QWidget>

#include <QHash>
#include <QList>
#include <QString>

#include "TaskOrchestrator.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTextEdit;
class QComboBox;

namespace RawrXD {

class OrchestrationUI : public QWidget {
    Q_OBJECT

public:
    explicit OrchestrationUI(TaskOrchestrator* orchestrator, QWidget* parent = nullptr);

private slots:
    void onOrchestrateClicked();
    void onTaskSplitCompleted(const QList<TaskDefinition>& tasks);
    void onModelSelectionCompleted(const QHash<QString, QString>& modelAssignments);
    void onTabCreated(const QString& tabName, const QString& model);
    void onTaskStarted(const QString& taskId, const QString& model);
    void onTaskProgress(const QString& taskId, int progress);
    void onTaskCompleted(const OrchestrationResult& result);
    void onOrchestrationCompleted(const QList<OrchestrationResult>& results);
    void onErrorOccurred(const QString& errorMessage);
    void onMemoryProfileChanged(int index);
    void onMemoryStrategyChanged(int index);

private:
    void setupUI();
    QWidget* createInputSection();
    QWidget* createProgressSection();
    QWidget* createResultsSection();
    QWidget* createMemorySettingsSection();
    void updateTaskList();
    void showResults(const QList<OrchestrationResult>& results);
    void updateMemoryUsage();
    QString formatMemorySize(qint64 bytes) const;

    TaskOrchestrator* m_orchestrator = nullptr;
    QLineEdit* m_taskInput = nullptr;
    QPushButton* m_orchestrateButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_overallProgress = nullptr;
    QListWidget* m_taskList = nullptr;
    QTextEdit* m_resultsDisplay = nullptr;
    QComboBox* m_memoryProfileCombo = nullptr;
    QComboBox* m_memoryStrategyCombo = nullptr;
    QLabel* m_memoryUsageLabel = nullptr;
    QProgressBar* m_memoryUsageBar = nullptr;
};

} // namespace RawrXD
