// Win32IDE_AgenticPlanningPanel.hpp
// Header for IDE agentic planning panel with approval queue UI

#pragma once

#include <windows.h>
#include <commctrl.h>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>


namespace Agentic
{
class AgenticPlanningOrchestrator;
struct ExecutionPlan;
}  // namespace Agentic

namespace Win32IDE
{

class Win32IDE_AgenticPlanningPanel
{
  public:
    Win32IDE_AgenticPlanningPanel();
    ~Win32IDE_AgenticPlanningPanel();

    // Window creation
    HWND createWindow(HWND hParent);

    // Main public API
    Agentic::ExecutionPlan* createPlanFromTask(const std::string& task);
    std::vector<std::string> getLogSnapshot();

    // Getters
    Agentic::AgenticPlanningOrchestrator* getOrchestrator() { return m_orchestrator; }
    HWND getHwnd() const { return m_hWnd; }

  private:
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleCommand(WORD wID, WORD wCode, HWND hControl);

    // Control IDs
    enum ControlID
    {
        TAB_MAIN = 1001,
        LIST_PLANS = 1002,
        LIST_STEPS = 1003,
        BUTTON_APPROVE = 1004,
        BUTTON_REJECT = 1005,
        BUTTON_EXECUTE = 1006,
        BUTTON_ROLLBACK = 1007
    };

    // Control initialization
    void initializeControls();

    // UI Update methods
    void refresh();
    void updatePlansList();
    void updateApprovalQueue();
    void updateStatus();
    void updateStepsList();
    void onResize(int width, int height);

    // Event handlers
    void onApproveClicked();
    void onRejectClicked();
    void onExecuteClicked();
    void onRollbackClicked();
    void onPlanSelectionChanged();
    void onStepSelectionChanged();
    void onApprovalRequested(const Agentic::ExecutionPlan* plan, int step_idx);

    // Logging
    void addLogEntry(const std::string& entry);

    // Members
    HWND m_hWnd;
    HWND m_hTabControl;
    HWND m_hPlansList;
    HWND m_hStepsList;
    HWND m_hStatusText;

    Agentic::AgenticPlanningOrchestrator* m_orchestrator = nullptr;

    std::string m_selectedPlanId;
    int m_selectedStepIndex;

    std::vector<std::string> m_logEntries;
    std::mutex m_logMutex;

    long m_refreshIntervalMs;
    std::chrono::system_clock::time_point m_lastRefreshTime;
};

Win32IDE_AgenticPlanningPanel* GetAgenticPlanningPanel();

}  // namespace Win32IDE
