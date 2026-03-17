#include "OrchestrationUI.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDateTime>

namespace RawrXD {

OrchestrationUI::OrchestrationUI(TaskOrchestrator* orchestrator, QWidget* parent)
    : QWidget(parent)
    , m_orchestrator(orchestrator)
{
    setupUI();
    
    // Connect orchestrator signals
    if (m_orchestrator) {
        connect(m_orchestrator, &TaskOrchestrator::taskSplitCompleted,
                this, &OrchestrationUI::onTaskSplitCompleted);
        connect(m_orchestrator, &TaskOrchestrator::modelSelectionCompleted,
                this, &OrchestrationUI::onModelSelectionCompleted);
        connect(m_orchestrator, &TaskOrchestrator::tabCreated,
                this, &OrchestrationUI::onTabCreated);
        connect(m_orchestrator, &TaskOrchestrator::taskStarted,
                this, &OrchestrationUI::onTaskStarted);
        connect(m_orchestrator, &TaskOrchestrator::taskProgress,
                this, &OrchestratorUI::onTaskProgress);
        connect(m_orchestrator, &TaskOrchestrator::taskCompleted,
                this, &OrchestrationUI::onTaskCompleted);
        connect(m_orchestrator, &TaskOrchestrator::orchestrationCompleted,
                this, &OrchestrationUI::onOrchestrationCompleted);
        connect(m_orchestrator, &TaskOrchestrator::errorOccurred,
                this, &OrchestrationUI::onErrorOccurred);
    }
}

void OrchestrationUI::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Memory settings section
    createMemorySettingsSection();
    mainLayout->addWidget(createMemorySettingsSection());
    
    // Input section
    createInputSection();
    mainLayout->addWidget(createInputSection());
    
    // Progress section
    createProgressSection();
    mainLayout->addWidget(createProgressSection());
    
    // Results section
    createResultsSection();
    mainLayout->addWidget(createResultsSection());
    
    mainLayout->addStretch();
}

QWidget* OrchestrationUI::createInputSection()
{
    QGroupBox* inputGroup = new QGroupBox("Task Orchestration");
    QVBoxLayout* layout = new QVBoxLayout(inputGroup);
    
    QLabel* instructionLabel = new QLabel(
        "Describe your task in natural language. The system will automatically split it into subtasks, "
        "select appropriate models, and execute them in parallel.");
    instructionLabel->setWordWrap(true);
    
    m_taskInput = new QLineEdit();
    m_taskInput->setPlaceholderText("e.g., Create a function to calculate prime numbers and write tests for it");
    
    m_orchestrateButton = new QPushButton("Orchestrate Task");
    m_orchestrateButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    
    connect(m_orchestrateButton, &QPushButton::clicked, this, &OrchestrationUI::onOrchestrateClicked);
    connect(m_taskInput, &QLineEdit::returnPressed, this, &OrchestrationUI::onOrchestrateClicked);
    
    layout->addWidget(instructionLabel);
    layout->addWidget(m_taskInput);
    layout->addWidget(m_orchestrateButton);
    
    return inputGroup;
}

QWidget* OrchestrationUI::createProgressSection()
{
    QGroupBox* progressGroup = new QGroupBox("Orchestration Progress");
    QVBoxLayout* layout = new QVBoxLayout(progressGroup);
    
    m_statusLabel = new QLabel("Ready to orchestrate tasks");
    m_overallProgress = new QProgressBar();
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    
    m_taskList = new QListWidget();
    m_taskList->setMaximumHeight(150);
    
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_overallProgress);
    layout->addWidget(new QLabel("Active Tasks:"));
    layout->addWidget(m_taskList);
    
    return progressGroup;
}

QWidget* OrchestrationUI::createResultsSection()
{
    QGroupBox* resultsGroup = new QGroupBox("Orchestration Results");
    QVBoxLayout* layout = new QVBoxLayout(resultsGroup);
    
    m_resultsDisplay = new QTextEdit();
    m_resultsDisplay->setReadOnly(true);
    m_resultsDisplay->setPlaceholderText("Results will appear here as tasks complete...");
    
    layout->addWidget(new QLabel("Results:"));
    layout->addWidget(m_resultsDisplay);
    
    return resultsGroup;
}

void OrchestrationUI::onOrchestrateClicked()
{
    QString taskDescription = m_taskInput->text().trimmed();
    
    if (taskDescription.isEmpty()) {
        QMessageBox::warning(this, "Input Required", "Please enter a task description");
        return;
    }
    
    if (m_orchestrator) {
        m_orchestrator->orchestrateTask(taskDescription);
        m_taskInput->clear();
        m_resultsDisplay->clear();
        m_taskList->clear();
        m_overallProgress->setValue(0);
        m_statusLabel->setText("Orchestrating task...");
    }
}

void OrchestrationUI::onTaskSplitCompleted(const QList<TaskDefinition>& tasks)
{
    m_statusLabel->setText(QString("Task split into %1 subtasks").arg(tasks.size()));
    
    for (const TaskDefinition& task : tasks) {
        QListWidgetItem* item = new QListWidgetItem(
            QString("%1: %2").arg(task.type).arg(task.description.left(50)));
        item->setData(Qt::UserRole, task.id);
        m_taskList->addItem(item);
    }
}

void OrchestrationUI::onModelSelectionCompleted(const QHash<QString, QString>& modelAssignments)
{
    m_statusLabel->setText("Models selected - starting execution...");
    
    // Update task list with model assignments
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        QString taskId = item->data(Qt::UserRole).toString();
        
        if (modelAssignments.contains(taskId)) {
            QString currentText = item->text();
            item->setText(currentText + QString(" [%1]").arg(modelAssignments[taskId]));
        }
    }
}

void OrchestrationUI::onTabCreated(const QString& tabName, const QString& model)
{
    m_resultsDisplay->append(QString("[%1] Created tab '%2' for model %3")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(tabName)
        .arg(model));
}

void OrchestrationUI::onTaskStarted(const QString& taskId, const QString& model)
{
    m_resultsDisplay->append(QString("[%1] Started task %2 with model %3")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(taskId.left(8))
        .arg(model));
    
    // Update progress
    int activeTasks = m_taskList->count();
    if (activeTasks > 0) {
        int progress = (m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0) * 100 / activeTasks;
        m_overallProgress->setValue(100 - progress);
    }
}

void OrchestrationUI::onTaskProgress(const QString& taskId, int progress)
{
    // Update individual task progress in the list
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        if (item->data(Qt::UserRole).toString() == taskId) {
            QString currentText = item->text();
            if (!currentText.contains("Progress:")) {
                item->setText(currentText + QString(" Progress: %1%").arg(progress));
            } else {
                // Update progress value
                QString newText = currentText.replace(QRegularExpression("Progress: \\d+%"), 
                                                    QString("Progress: %1%").arg(progress));
                item->setText(newText);
            }
            break;
        }
    }
}

void OrchestrationUI::onTaskCompleted(const OrchestrationResult& result)
{
    QString status = result.success ? "SUCCESS" : "FAILED";
    m_resultsDisplay->append(QString("[%1] Task %2 completed: %3 (%4ms)")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(result.taskId.left(8))
        .arg(status)
        .arg(result.executionTime));
    
    if (!result.success) {
        m_resultsDisplay->append(QString("Error: %1").arg(result.error));
    } else if (!result.result.isEmpty()) {
        m_resultsDisplay->append(QString("Result: %1").arg(result.result.left(100) + "..."));
    }
    
    // Remove completed task from list
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        if (item->data(Qt::UserRole).toString() == result.taskId) {
            delete m_taskList->takeItem(i);
            break;
        }
    }
    
    // Update overall progress
    int remainingTasks = m_taskList->count();
    int totalTasks = remainingTasks + (m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0);
    
    if (totalTasks > 0) {
        int progress = (totalTasks - remainingTasks) * 100 / totalTasks;
        m_overallProgress->setValue(progress);
    }
}

void OrchestrationUI::onOrchestrationCompleted(const QList<OrchestrationResult>& results)
{
    m_statusLabel->setText("Orchestration completed!");
    m_overallProgress->setValue(100);
    
    int successCount = 0;
    int totalCount = results.size();
    
    for (const OrchestrationResult& result : results) {
        if (result.success) successCount++;
    }
    
    m_resultsDisplay->append(QString("\n=== ORCHESTRATION COMPLETED ==="));
    m_resultsDisplay->append(QString("Success: %1/%2 tasks").arg(successCount).arg(totalCount));
    m_resultsDisplay->append(QString("Total execution time: Various (see individual tasks)"));
    
    if (successCount == totalCount) {
        m_resultsDisplay->append("All tasks completed successfully!");
    } else {
        m_resultsDisplay->append("Some tasks failed. Check individual results above.");
    }
}

void OrchestrationUI::onErrorOccurred(const QString& errorMessage)
{
    m_statusLabel->setText("Error occurred");
    m_resultsDisplay->append(QString("[ERROR] %1").arg(errorMessage));
    
    QMessageBox::critical(this, "Orchestration Error", errorMessage);
}

void OrchestrationUI::updateTaskList()
{
    // Implementation for updating task list display
}

void OrchestrationUI::showResults(const QList<OrchestrationResult>& results)
{
    // Implementation for showing final results
}

} // namespace RawrXD