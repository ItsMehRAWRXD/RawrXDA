#include "OrchestrationUI.h"
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
}

void OrchestrationUI::setupUI()
{
    createMemorySettingsSection();
    createInputSection();
    createProgressSection();
    createResultsSection();
}

void* OrchestrationUI::createInputSection()
{
    return nullptr;
}

void* OrchestrationUI::createProgressSection()
{
    return nullptr;
}

void* OrchestrationUI::createResultsSection()
{
    return nullptr;
}

void OrchestrationUI::onOrchestrateClicked()
{
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
        updateMemoryUsage();
    }
}

void OrchestrationUI::onTaskSplitCompleted(const std::vector<TaskDefinition>& tasks)
{
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
}

void OrchestrationUI::onModelSelectionCompleted(const std::map<std::string, std::string>& modelAssignments)
{
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
}

void OrchestrationUI::onTabCreated(const std::string& tabName, const std::string& model)
{
    (void)tabName;
    (void)model;
    setWindowText(m_resultsDisplay, "[Created tab for model]");
}

void OrchestrationUI::onTaskStarted(const std::string& taskId, const std::string& model)
{
    (void)taskId;
    (void)model;
    int activeTasks = static_cast<int>(m_taskItems.size());
    if (activeTasks > 0)
    {
        size_t current = m_orchestrator ? m_orchestrator->getCurrentTasks().size() : 0;
        int progress = static_cast<int>(current * 100 / m_taskItems.size());
        setProgress(m_overallProgress, 100 - progress);
    }
}

void OrchestrationUI::onTaskProgress(const std::string& taskId, int progress)
{
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
}

void OrchestrationUI::onTaskCompleted(const OrchestrationResult& result)
{
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
}

void OrchestrationUI::onOrchestrationCompleted(const std::vector<OrchestrationResult>& results)
{
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
}

void OrchestrationUI::onErrorOccurred(const std::string& errorMessage)
{
    setWindowText(m_statusLabel, "Error occurred");
    setWindowText(m_resultsDisplay, std::string("[ERROR] ") + errorMessage);
#ifdef _WIN32
    MessageBoxA(nullptr, errorMessage.c_str(), "Orchestration Error", MB_OK | MB_ICONERROR);
#endif
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
    return nullptr;
}

static const char* s_memoryProfiles[] = {"minimal", "standard", "large", "unlimited"};
static const char* s_memoryStrategies[] = {"conservative", "balanced", "aggressive"};

void OrchestrationUI::onMemoryProfileChanged(int index)
{
    if (m_orchestrator && index >= 0 && index < 4)
    {
        m_orchestrator->setMemoryProfile(s_memoryProfiles[index]);
        updateMemoryUsage();
    }
}

void OrchestrationUI::onMemoryStrategyChanged(int index)
{
    if (m_orchestrator && index >= 0 && index < 3)
        m_orchestrator->setTaskMemoryStrategy(s_memoryStrategies[index]);
}

void OrchestrationUI::updateMemoryUsage()
{
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
}

std::string OrchestrationUI::formatMemorySize(int64_t bytes) const
{
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
