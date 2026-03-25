#include "OrchestrationUI.h"
<<<<<<< HEAD
#include <algorithm>
#include <cstdio>
#ifdef _WIN32
#include <commctrl.h>
#include <windows.h>
#endif
namespace RawrXD
{

namespace
{
void setWindowText(void* hwnd, const std::string& s)
{
#ifdef _WIN32
    if (hwnd)
    {
        std::string local = s;
        SetWindowTextA(static_cast<HWND>(hwnd), local.c_str());
    }
#endif
}
void setProgress(void* hwnd, int value)
{
#ifdef _WIN32
    if (hwnd)
        SendMessage(static_cast<HWND>(hwnd), PBM_SETPOS, value, 0);
#endif
    (void)value;
}
}  // namespace

OrchestrationUI::OrchestrationUI(TaskOrchestrator* orchestrator, void* parent) : m_orchestrator(orchestrator)
{
    (void)parent;
    setupUI();
=======
namespace RawrXD {

OrchestrationUI::OrchestrationUI(TaskOrchestrator* orchestrator, void* parent)
    : // Widget(parent)
    , m_orchestrator(orchestrator)
{
    setupUI();
    
    // Connect orchestrator signals
    if (m_orchestrator) {  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}
>>>>>>> origin/main
}

void OrchestrationUI::setupUI()
{
<<<<<<< HEAD
    createMemorySettingsSection();
    createInputSection();
    createProgressSection();
    createResultsSection();
=======
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
>>>>>>> origin/main
}

void* OrchestrationUI::createInputSection()
{
<<<<<<< HEAD
    return nullptr;
=======
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
>>>>>>> origin/main
}

void* OrchestrationUI::createProgressSection()
{
<<<<<<< HEAD
    return nullptr;
=======
    void* progressGroup = new void("Orchestration Progress");
    void* layout = new void(progressGroup);
    
    m_statusLabel = new void("Ready to orchestrate tasks");
    m_overallProgress = new void();
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    
    m_taskList = nullptr;
    m_taskList->setMaximumHeight(150);
    
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_overallProgress);
    layout->addWidget(new void("Active Tasks:"));
    layout->addWidget(m_taskList);
    
    return progressGroup;
>>>>>>> origin/main
}

void* OrchestrationUI::createResultsSection()
{
<<<<<<< HEAD
    return nullptr;
=======
    void* resultsGroup = new void("Orchestration Results");
    void* layout = new void(resultsGroup);
    
    m_resultsDisplay = new void();
    m_resultsDisplay->setReadOnly(true);
    m_resultsDisplay->setPlaceholderText("Results will appear here as tasks complete...");
    
    layout->addWidget(new void("Results:"));
    layout->addWidget(m_resultsDisplay);
    
    return resultsGroup;
>>>>>>> origin/main
}

void OrchestrationUI::onOrchestrateClicked()
{
<<<<<<< HEAD
    std::string taskDescription;
#ifdef _WIN32
    if (m_taskInput)
    {
        char buf[4096] = {};
        if (GetWindowTextA(static_cast<HWND>(m_taskInput), buf, sizeof(buf)))
            taskDescription = buf;
        while (!taskDescription.empty() && (taskDescription.back() == ' ' || taskDescription.back() == '\t'))
            taskDescription.pop_back();
    }
#endif
    if (taskDescription.empty())
    {
#ifdef _WIN32
        MessageBoxA(nullptr, "Please enter a task description", "Input Required", MB_OK | MB_ICONEXCLAMATION);
#endif
        return;
    }
    if (m_orchestrator)
    {
        m_orchestrator->orchestrateTask(taskDescription);
#ifdef _WIN32
        if (m_taskInput)
            SetWindowTextA(static_cast<HWND>(m_taskInput), "");
        if (m_resultsDisplay)
            SetWindowTextA(static_cast<HWND>(m_resultsDisplay), "");
#endif
        m_taskItems.clear();
        setProgress(m_overallProgress, 0);
        setWindowText(m_statusLabel, "Orchestrating task...");
=======
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
>>>>>>> origin/main
        updateMemoryUsage();
    }
}

void OrchestrationUI::onTaskSplitCompleted(const std::vector<TaskDefinition>& tasks)
{
<<<<<<< HEAD
    setWindowText(m_statusLabel, "Task split into " + std::to_string(tasks.size()) + " subtasks");
    m_taskItems.clear();
    for (const TaskDefinition& task : tasks)
        m_taskItems.push_back({task.id, task.description.empty() ? task.id : task.description});
#ifdef _WIN32
    if (m_taskList)
    {
        SendMessage(static_cast<HWND>(m_taskList), LVM_DELETEALLITEMS, 0, 0);
        for (size_t i = 0; i < m_taskItems.size(); ++i)
        {
            LVITEMA lv = {};
            lv.mask = LVIF_TEXT;
            lv.iItem = static_cast<int>(i);
            lv.pszText = const_cast<char*>(m_taskItems[i].second.c_str());
            SendMessage(static_cast<HWND>(m_taskList), LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&lv));
        }
    }
#endif
=======
    m_statusLabel->setText(std::string("Task split into %1 subtasks")));
    
    for (const TaskDefinition& task : tasks) {
        QListWidgetItem* item = nullptr));
        item->setData(UserRole, task.id);
        m_taskList->addItem(item);
    }
>>>>>>> origin/main
}

void OrchestrationUI::onModelSelectionCompleted(const std::map<std::string, std::string>& modelAssignments)
{
<<<<<<< HEAD
    setWindowText(m_statusLabel, "Models selected - starting execution...");
    for (size_t i = 0; i < m_taskItems.size(); ++i)
    {
        auto it = modelAssignments.find(m_taskItems[i].first);
        if (it != modelAssignments.end())
            m_taskItems[i].second += " [" + it->second + "]";
    }
#ifdef _WIN32
    if (m_taskList)
        for (size_t i = 0; i < m_taskItems.size(); ++i)
        {
            LVITEMA lv = {};
            lv.mask = LVIF_TEXT;
            lv.iItem = static_cast<int>(i);
            lv.pszText = const_cast<char*>(m_taskItems[i].second.c_str());
            SendMessage(static_cast<HWND>(m_taskList), LVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&lv));
        }
#endif
=======
    m_statusLabel->setText("Models selected - starting execution...");
    
    // Update task list with model assignments
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        std::string taskId = item->data(UserRole).toString();
        
        if (modelAssignments.contains(taskId)) {
            std::string currentText = item->text();
            item->setText(currentText + std::string(" [%1]"));
        }
    }
>>>>>>> origin/main
}

void OrchestrationUI::onTabCreated(const std::string& tabName, const std::string& model)
{
<<<<<<< HEAD
    (void)tabName;
    (void)model;
    setWindowText(m_resultsDisplay, "[Created tab for model]");
=======
    m_resultsDisplay->append(std::string("[%1] Created tab '%2' for model %3")
        .toString("hh:mm:ss"))
        
        );
>>>>>>> origin/main
}

void OrchestrationUI::onTaskStarted(const std::string& taskId, const std::string& model)
{
<<<<<<< HEAD
    (void)taskId;
    (void)model;
    int activeTasks = static_cast<int>(m_taskItems.size());
    if (activeTasks > 0)
    {
        size_t current = m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0;
        int progress = static_cast<int>(current * 100 / m_taskItems.size());
        setProgress(m_overallProgress, 100 - progress);
=======
    m_resultsDisplay->append(std::string("[%1] Started task %2 with model %3")
        .toString("hh:mm:ss"))
        )
        );
    
    // Update progress
    int activeTasks = m_taskList->count();
    if (activeTasks > 0) {
        int progress = (m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0) * 100 / activeTasks;
        m_overallProgress->setValue(100 - progress);
>>>>>>> origin/main
    }
}

void OrchestrationUI::onTaskProgress(const std::string& taskId, int progress)
{
<<<<<<< HEAD
    for (auto& p : m_taskItems)
    {
        if (p.first == taskId)
        {
            size_t pos = p.second.find(" Progress:");
            if (pos != std::string::npos)
                p.second.resize(pos);
            p.second += " Progress: " + std::to_string(progress) + "%";
            break;
        }
    }
#ifdef _WIN32
    if (m_taskList)
        for (size_t i = 0; i < m_taskItems.size(); ++i)
        {
            LVITEMA lv = {};
            lv.mask = LVIF_TEXT;
            lv.iItem = static_cast<int>(i);
            lv.pszText = const_cast<char*>(m_taskItems[i].second.c_str());
            SendMessage(static_cast<HWND>(m_taskList), LVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&lv));
        }
#endif
=======
    // Update individual task progress in the list
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem* item = m_taskList->item(i);
        if (item->data(UserRole).toString() == taskId) {
            std::string currentText = item->text();
            if (!currentText.contains("Progress:")) {
                item->setText(currentText + std::string(" Progress: %1%"));
            } else {
                // Update progress value
                std::string newText = currentText.replace(std::regex("Progress: \\d+%"), 
                                                    std::string("Progress: %1%"));
                item->setText(newText);
            }
            break;
        }
    }
>>>>>>> origin/main
}

void OrchestrationUI::onTaskCompleted(const OrchestrationResult& result)
{
<<<<<<< HEAD
    setWindowText(m_resultsDisplay, std::string("[Task completed] ") + (result.success ? "SUCCESS" : "FAILED"));
    updateMemoryUsage();
    auto it = std::find_if(m_taskItems.begin(), m_taskItems.end(),
                           [&](const std::pair<std::string, std::string>& p) { return p.first == result.taskId; });
    if (it != m_taskItems.end())
        m_taskItems.erase(it);
#ifdef _WIN32
    if (m_taskList)
    {
        SendMessage(static_cast<HWND>(m_taskList), LVM_DELETEALLITEMS, 0, 0);
        for (size_t i = 0; i < m_taskItems.size(); ++i)
        {
            LVITEMA lv = {};
            lv.mask = LVIF_TEXT;
            lv.iItem = static_cast<int>(i);
            lv.pszText = const_cast<char*>(m_taskItems[i].second.c_str());
            SendMessage(static_cast<HWND>(m_taskList), LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&lv));
        }
    }
#endif
    size_t remaining = m_taskItems.size();
    size_t total = remaining + (m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0);
    if (total > 0)
        setProgress(m_overallProgress, static_cast<int>((total - remaining) * 100 / total));
=======
    std::string status = result.success ? "SUCCESS" : "FAILED";
    m_resultsDisplay->append(std::string("[%1] Task %2 completed: %3 (%4ms)")
        .toString("hh:mm:ss"))
        )
        
        );
    
    if (!result.success) {
        m_resultsDisplay->append(std::string("Error: %1"));
    } else if (!result.result.empty()) {
        m_resultsDisplay->append(std::string("Result: %1") + "..."));
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
>>>>>>> origin/main
}

void OrchestrationUI::onOrchestrationCompleted(const std::vector<OrchestrationResult>& results)
{
<<<<<<< HEAD
    setWindowText(m_statusLabel, "Orchestration completed!");
    setProgress(m_overallProgress, 100);
    size_t successCount = 0;
    for (const auto& r : results)
        if (r.success)
            ++successCount;
    std::string msg = "\n=== ORCHESTRATION COMPLETED ===\nSuccess: " + std::to_string(successCount) + "/" +
                      std::to_string(results.size()) + " tasks\n";
    if (successCount == results.size())
        msg += "All tasks completed successfully!";
    else
        msg += "Some tasks failed. Check individual results above.";
    setWindowText(m_resultsDisplay, msg);
=======
    m_statusLabel->setText("Orchestration completed!");
    m_overallProgress->setValue(100);
    
    int successCount = 0;
    int totalCount = results.size();
    
    for (const OrchestrationResult& result : results) {
        if (result.success) successCount++;
    }
    
    m_resultsDisplay->append(std::string("\n=== ORCHESTRATION COMPLETED ==="));
    m_resultsDisplay->append(std::string("Success: %1/%2 tasks"));
    m_resultsDisplay->append(std::string("Total execution time: Various (see individual tasks)"));
    
    if (successCount == totalCount) {
        m_resultsDisplay->append("All tasks completed successfully!");
    } else {
        m_resultsDisplay->append("Some tasks failed. Check individual results above.");
    }
>>>>>>> origin/main
}

void OrchestrationUI::onErrorOccurred(const std::string& errorMessage)
{
<<<<<<< HEAD
    setWindowText(m_statusLabel, "Error occurred");
    setWindowText(m_resultsDisplay, std::string("[ERROR] ") + errorMessage);
#ifdef _WIN32
    MessageBoxA(nullptr, errorMessage.c_str(), "Orchestration Error", MB_OK | MB_ICONERROR);
#endif
=======
    m_statusLabel->setText("Error occurred");
    m_resultsDisplay->append(std::string("[ERROR] %1"));
    
    void::critical(this, "Orchestration Error", errorMessage);
>>>>>>> origin/main
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
<<<<<<< HEAD
    return nullptr;
}

static const char* s_memoryProfiles[] = {"minimal", "standard", "large", "unlimited"};
static const char* s_memoryStrategies[] = {"conservative", "balanced", "aggressive"};

void OrchestrationUI::onMemoryProfileChanged(int index)
{
    if (m_orchestrator && index >= 0 && index < 4)
    {
        m_orchestrator->setMemoryProfile(s_memoryProfiles[index]);
=======
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
>>>>>>> origin/main
        updateMemoryUsage();
    }
}

void OrchestrationUI::onMemoryStrategyChanged(int index)
{
<<<<<<< HEAD
    if (m_orchestrator && index >= 0 && index < 3)
        m_orchestrator->setTaskMemoryStrategy(s_memoryStrategies[index]);
=======
    if (m_orchestrator) {
        std::string strategy = m_memoryStrategyCombo->itemData(index).toString();
        m_orchestrator->setTaskMemoryStrategy(strategy);
    }
>>>>>>> origin/main
}

void OrchestrationUI::updateMemoryUsage()
{
<<<<<<< HEAD
    if (!m_orchestrator)
        return;
    int64_t used = m_orchestrator->getTotalMemoryUsage();
    int64_t available = m_orchestrator->getAvailableMemory();
    std::string usedStr = formatMemorySize(used);
    std::string availableStr = (available == LLONG_MAX) ? "Unlimited" : formatMemorySize(available);
    setWindowText(m_memoryUsageLabel, "Memory Usage: " + usedStr + " / " + availableStr);
    if (available != LLONG_MAX && available > 0)
        setProgress(m_memoryUsageBar, static_cast<int>((used * 100) / (used + available)));
    else
        setProgress(m_memoryUsageBar, 0);
=======
    if (!m_orchestrator) return;
    
    int64_t used = m_orchestrator->getTotalMemoryUsage();
    int64_t available = m_orchestrator->getAvailableMemory();
    
    std::string usedStr = formatMemorySize(used);
    std::string availableStr = (available == LLONG_MAX) ? "Unlimited" : formatMemorySize(available);
    
    m_memoryUsageLabel->setText(std::string("Memory Usage: %1 / %2"));
    
    if (available != LLONG_MAX && available > 0) {
        int percentage = (used * 100) / (used + available);
        m_memoryUsageBar->setValue(percentage);
    } else {
        m_memoryUsageBar->setValue(0);
    }
>>>>>>> origin/main
}

std::string OrchestrationUI::formatMemorySize(int64_t bytes) const
{
<<<<<<< HEAD
    if (bytes == 0)
        return "0 B";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024 && unitIndex < 4)
    {
        size /= 1024;
        unitIndex++;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f %s", size, units[unitIndex]);
    return buf;
}

}  // namespace RawrXD
=======
    if (bytes == 0) return "0 B";
    
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;
    
    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }
    
    return std::string("%1 %2");
}

} // namespace RawrXD

>>>>>>> origin/main
