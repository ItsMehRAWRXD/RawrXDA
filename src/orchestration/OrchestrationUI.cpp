#include "OrchestrationUI.h"
namespace RawrXD {

OrchestrationUI::OrchestrationUI(TaskOrchestrator* orchestrator, void* parent)
    : // Widget(parent)
    , m_orchestrator(orchestrator)
{
    setupUI();
    
    // Connect orchestrator signals
    if (m_orchestrator) {  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}
}

void OrchestrationUI::setupUI()
{
    void* mainLayout = new void(this);
    
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

void* OrchestrationUI::createInputSection()
{
    void* inputGroup = new void("Task Orchestration");
    void* layout = new void(inputGroup);
    
    void* instructionLabel = new void(
        "Describe your task in natural language. The system will automatically split it into subtasks, "
        "select appropriate models, and execute them in parallel.");
    instructionLabel->setWordWrap(true);
    
    m_taskInput = new voidEdit();
    m_taskInput->setPlaceholderText("e.g., Create a function to calculate prime numbers and write tests for it");
    
    m_orchestrateButton = new void("Orchestrate Task");
    m_orchestrateButton->setStyleSheet("void { background-color: #4CAF50; color: white; font-weight: bold; }");  // Signal connection removed\n  // Signal connection removed\nlayout->addWidget(instructionLabel);
    layout->addWidget(m_taskInput);
    layout->addWidget(m_orchestrateButton);
    
    return inputGroup;
}

void* OrchestrationUI::createProgressSection()
{
    void* progressGroup = new void("Orchestration Progress");
    void* layout = new void(progressGroup);
    
    m_statusLabel = new void("Ready to orchestrate tasks");
    m_overallProgress = new void();
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    
    m_taskList = new QListWidget();
    m_taskList->setMaximumHeight(150);
    
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_overallProgress);
    layout->addWidget(new void("Active Tasks:"));
    layout->addWidget(m_taskList);
    
    return progressGroup;
}

void* OrchestrationUI::createResultsSection()
{
    void* resultsGroup = new void("Orchestration Results");
    void* layout = new void(resultsGroup);
    
    m_resultsDisplay = new void();
    m_resultsDisplay->setReadOnly(true);
    m_resultsDisplay->setPlaceholderText("Results will appear here as tasks complete...");
    
    layout->addWidget(new void("Results:"));
    layout->addWidget(m_resultsDisplay);
    
    return resultsGroup;
}

void OrchestrationUI::onOrchestrateClicked()
{
    std::string taskDescription = m_taskInput->text().trimmed();
    
    if (taskDescription.empty()) {
        void::warning(this, "Input Required", "Please enter a task description");
        return;
    }
    
    if (m_orchestrator) {
        m_orchestrator->orchestrateTask(taskDescription);
        m_taskInput->clear();
        m_resultsDisplay->clear();
        m_taskList->clear();
        m_overallProgress->setValue(0);
        m_statusLabel->setText("Orchestrating task...");
        
        // Update memory usage for new orchestration
        updateMemoryUsage();
    }
}

void OrchestrationUI::onTaskSplitCompleted(const std::vector<TaskDefinition>& tasks)
{
    m_statusLabel->setText(std::string("Task split into %1 subtasks").arg(tasks.size()));
    
    for (const TaskDefinition& task : tasks) {
        QListWidgetItem* item = new QListWidgetItem(
            std::string("%1: %2").arg(task.type).arg(task.description.left(50)));
        item->setData(UserRole, task.id);
        m_taskList->addItem(item);
    }
}

void OrchestrationUI::onModelSelectionCompleted(const std::map<std::string, std::string>& modelAssignments)
{
    m_statusLabel->setText("Models selected - starting execution...");
    
    // Update task list with model assignments
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        std::string taskId = item->data(UserRole).toString();
        
        if (modelAssignments.contains(taskId)) {
            std::string currentText = item->text();
            item->setText(currentText + std::string(" [%1]").arg(modelAssignments[taskId]));
        }
    }
}

void OrchestrationUI::onTabCreated(const std::string& tabName, const std::string& model)
{
    m_resultsDisplay->append(std::string("[%1] Created tab '%2' for model %3")
        .arg(// DateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(tabName)
        .arg(model));
}

void OrchestrationUI::onTaskStarted(const std::string& taskId, const std::string& model)
{
    m_resultsDisplay->append(std::string("[%1] Started task %2 with model %3")
        .arg(// DateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(taskId.left(8))
        .arg(model));
    
    // Update progress
    int activeTasks = m_taskList->count();
    if (activeTasks > 0) {
        int progress = (m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0) * 100 / activeTasks;
        m_overallProgress->setValue(100 - progress);
    }
}

void OrchestrationUI::onTaskProgress(const std::string& taskId, int progress)
{
    // Update individual task progress in the list
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        if (item->data(UserRole).toString() == taskId) {
            std::string currentText = item->text();
            if (!currentText.contains("Progress:")) {
                item->setText(currentText + std::string(" Progress: %1%").arg(progress));
            } else {
                // Update progress value
                std::string newText = currentText.replace(std::regex("Progress: \\d+%"), 
                                                    std::string("Progress: %1%").arg(progress));
                item->setText(newText);
            }
            break;
        }
    }
}

void OrchestrationUI::onTaskCompleted(const OrchestrationResult& result)
{
    std::string status = result.success ? "SUCCESS" : "FAILED";
    m_resultsDisplay->append(std::string("[%1] Task %2 completed: %3 (%4ms)")
        .arg(// DateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(result.taskId.left(8))
        .arg(status)
        .arg(result.executionTime));
    
    if (!result.success) {
        m_resultsDisplay->append(std::string("Error: %1").arg(result.error));
    } else if (!result.result.empty()) {
        m_resultsDisplay->append(std::string("Result: %1").arg(result.result.left(100) + "..."));
    }
    
    // Update memory usage display
    updateMemoryUsage();
    
    // Remove completed task from list
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        if (item->data(UserRole).toString() == result.taskId) {
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

void OrchestrationUI::onOrchestrationCompleted(const std::vector<OrchestrationResult>& results)
{
    m_statusLabel->setText("Orchestration completed!");
    m_overallProgress->setValue(100);
    
    int successCount = 0;
    int totalCount = results.size();
    
    for (const OrchestrationResult& result : results) {
        if (result.success) successCount++;
    }
    
    m_resultsDisplay->append(std::string("\n=== ORCHESTRATION COMPLETED ==="));
    m_resultsDisplay->append(std::string("Success: %1/%2 tasks").arg(successCount).arg(totalCount));
    m_resultsDisplay->append(std::string("Total execution time: Various (see individual tasks)"));
    
    if (successCount == totalCount) {
        m_resultsDisplay->append("All tasks completed successfully!");
    } else {
        m_resultsDisplay->append("Some tasks failed. Check individual results above.");
    }
}

void OrchestrationUI::onErrorOccurred(const std::string& errorMessage)
{
    m_statusLabel->setText("Error occurred");
    m_resultsDisplay->append(std::string("[ERROR] %1").arg(errorMessage));
    
    void::critical(this, "Orchestration Error", errorMessage);
}

void OrchestrationUI::updateTaskList()
{
    // Implementation for updating task list display
}

void OrchestrationUI::showResults(const std::vector<OrchestrationResult>& results)
{
    // Implementation for showing final results
}

void* OrchestrationUI::createMemorySettingsSection()
{
    void* memoryGroup = new void("Memory Management");
    void* layout = new void(memoryGroup);
    
    // Memory profile selection
    void* profileLayout = new void();
    void* profileLabel = new void("Memory Profile:");
    m_memoryProfileCombo = new void();
    m_memoryProfileCombo->addItem("Minimal (1KB-64KB per task)", "minimal");
    m_memoryProfileCombo->addItem("Standard (64KB-16MB per task)", "standard");
    m_memoryProfileCombo->addItem("Large (1MB-512MB per task)", "large");
    m_memoryProfileCombo->addItem("Unlimited (No limits)", "unlimited");
    m_memoryProfileCombo->setCurrentText("Standard (64KB-16MB per task)");
    
    profileLayout->addWidget(profileLabel);
    profileLayout->addWidget(m_memoryProfileCombo);
    profileLayout->addStretch();
    
    // Memory strategy selection
    void* strategyLayout = new void();
    void* strategyLabel = new void("Allocation Strategy:");
    m_memoryStrategyCombo = new void();
    m_memoryStrategyCombo->addItem("Conservative (70% of calculated)", "conservative");
    m_memoryStrategyCombo->addItem("Balanced (100% of calculated)", "balanced");
    m_memoryStrategyCombo->addItem("Aggressive (130% of calculated)", "aggressive");
    m_memoryStrategyCombo->setCurrentText("Balanced (100% of calculated)");
    
    strategyLayout->addWidget(strategyLabel);
    strategyLayout->addWidget(m_memoryStrategyCombo);
    strategyLayout->addStretch();
    
    // Memory usage display
    void* usageLayout = new void();
    m_memoryUsageLabel = new void("Memory Usage: 0 B / 1 GB");
    m_memoryUsageBar = new void();
    m_memoryUsageBar->setRange(0, 100);
    m_memoryUsageBar->setValue(0);
    
    usageLayout->addWidget(m_memoryUsageLabel);
    usageLayout->addWidget(m_memoryUsageBar);
    
    layout->addLayout(profileLayout);
    layout->addLayout(strategyLayout);
    layout->addLayout(usageLayout);
    
    // Connect signals  // Signal connection removed\n  // Signal connection removed\nreturn memoryGroup;
}

void OrchestrationUI::onMemoryProfileChanged(int index)
{
    if (m_orchestrator) {
        std::string profile = m_memoryProfileCombo->itemData(index).toString();
        m_orchestrator->setMemoryProfile(profile);
        updateMemoryUsage();
    }
}

void OrchestrationUI::onMemoryStrategyChanged(int index)
{
    if (m_orchestrator) {
        std::string strategy = m_memoryStrategyCombo->itemData(index).toString();
        m_orchestrator->setTaskMemoryStrategy(strategy);
    }
}

void OrchestrationUI::updateMemoryUsage()
{
    if (!m_orchestrator) return;
    
    int64_t used = m_orchestrator->getTotalMemoryUsage();
    int64_t available = m_orchestrator->getAvailableMemory();
    
    std::string usedStr = formatMemorySize(used);
    std::string availableStr = (available == LLONG_MAX) ? "Unlimited" : formatMemorySize(available);
    
    m_memoryUsageLabel->setText(std::string("Memory Usage: %1 / %2").arg(usedStr).arg(availableStr));
    
    if (available != LLONG_MAX && available > 0) {
        int percentage = (used * 100) / (used + available);
        m_memoryUsageBar->setValue(percentage);
    } else {
        m_memoryUsageBar->setValue(0);
    }
}

std::string OrchestrationUI::formatMemorySize(int64_t bytes) const
{
    if (bytes == 0) return "0 B";
    
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;
    
    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }
    
    return std::string("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
}

} // namespace RawrXD






