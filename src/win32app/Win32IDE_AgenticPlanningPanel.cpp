// Win32IDE_AgenticPlanningPanel.cpp
// IDE panel for viewing execution plans, approval queue, and step-by-step progress
// Integrates AgenticPlanningOrchestrator with Win32 message pump

#include "Win32IDE_AgenticPlanningPanel.hpp"
#include "../agentic/agentic_orchestrator_integration.hpp"
#include "../agentic/agentic_planning_orchestrator.hpp"
#include <windowsx.h>
#include <iomanip>
#include <sstream>

namespace
{
std::wstring wideFromUtf8(const std::string& s)
{
    if (s.empty())
    {
        return std::wstring();
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0)
    {
        return std::wstring(s.begin(), s.end());
    }
    std::wstring out;
    out.resize(static_cast<size_t>(needed));
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), needed);
    return out;
}

std::string narrowFromWide(const std::wstring& ws)
{
    if (ws.empty())
    {
        return std::string();
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
    {
        int acpNeeded = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (acpNeeded <= 0)
        {
            return std::string();
        }
        std::string acpOut;
        acpOut.resize(static_cast<size_t>(acpNeeded));
        WideCharToMultiByte(CP_ACP, 0, ws.c_str(), static_cast<int>(ws.size()), acpOut.data(), acpNeeded, nullptr, nullptr);
        return acpOut;
    }
    std::string out;
    out.resize(static_cast<size_t>(needed));
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), needed, nullptr, nullptr);
    return out;
}
}  // namespace


namespace Win32IDE
{

// ============================================================================
// Panel Registration & Factory
// ============================================================================

Win32IDE_AgenticPlanningPanel* g_agenticPlanningPanel = nullptr;

Win32IDE_AgenticPlanningPanel* GetAgenticPlanningPanel()
{
    if (!g_agenticPlanningPanel)
    {
        g_agenticPlanningPanel = new Win32IDE_AgenticPlanningPanel();
    }
    return g_agenticPlanningPanel;
}

// ============================================================================
// Constructor / Initialization
// ============================================================================

Win32IDE_AgenticPlanningPanel::Win32IDE_AgenticPlanningPanel()
    : m_hWnd(nullptr), m_selectedPlanId(""), m_selectedStepIndex(-1), m_refreshIntervalMs(500),
      m_lastRefreshTime(std::chrono::system_clock::now())
{
    Agentic::OrchestratorIntegration::instance().initialize();
    m_orchestrator = Agentic::OrchestratorIntegration::instance().getOrchestrator();

    if (m_orchestrator)
    {
        m_orchestrator->setExecutionLogFn([this](const std::string& log_entry) { this->addLogEntry(log_entry); });

        m_orchestrator->setApprovalCallback([this](const Agentic::ExecutionPlan& plan, int step_idx)
                                            { this->onApprovalRequested(&plan, step_idx); });
    }
}

Win32IDE_AgenticPlanningPanel::~Win32IDE_AgenticPlanningPanel() {}

// ============================================================================
// Window Management
// ============================================================================

HWND Win32IDE_AgenticPlanningPanel::createWindow(HWND hParent)
{
    if (m_hWnd)
        return m_hWnd;

    // Register window class
    static WNDCLASSEXW wc = {};
    if (!wc.cbSize)
    {  // First time
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"Win32IDE_AgenticPlanningPanel";
        RegisterClassExW(&wc);
    }

    m_hWnd = CreateWindowExW(0, L"Win32IDE_AgenticPlanningPanel", L"Agentic Planning & Approval", WS_CHILD | WS_VISIBLE,
                             0, 0, 600, 400, hParent, nullptr, GetModuleHandle(nullptr), this);

    if (m_hWnd)
    {
        this->initializeControls();
        SetTimer(m_hWnd, 1, m_refreshIntervalMs, nullptr);
    }

    return m_hWnd;
}

LRESULT CALLBACK Win32IDE_AgenticPlanningPanel::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE_AgenticPlanningPanel* pThis = nullptr;

    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = static_cast<Win32IDE_AgenticPlanningPanel*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = reinterpret_cast<Win32IDE_AgenticPlanningPanel*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->handleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT Win32IDE_AgenticPlanningPanel::handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_TIMER:
            if (wParam == 1)
            {
                this->refresh();
            }
            return 0;

        case WM_COMMAND:
            return handleCommand(LOWORD(wParam), HIWORD(wParam), (HWND)lParam);

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);
            // Paint background
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(m_hWnd, &ps);
            return 0;
        }

        case WM_SIZE:
            this->onResize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_DESTROY:
            KillTimer(m_hWnd, 1);
            m_hWnd = nullptr;
            return 0;
    }

    return DefWindowProc(m_hWnd, uMsg, wParam, lParam);
}

LRESULT Win32IDE_AgenticPlanningPanel::handleCommand(WORD wID, WORD wCode, HWND hControl)
{
    switch (wID)
    {
        case BUTTON_APPROVE:
            this->onApproveClicked();
            return 0;

        case BUTTON_REJECT:
            this->onRejectClicked();
            return 0;

        case BUTTON_EXECUTE:
            this->onExecuteClicked();
            return 0;

        case BUTTON_ROLLBACK:
            this->onRollbackClicked();
            return 0;

        case BUTTON_RESUME:
            this->onResumeClicked();
            return 0;

        case LIST_PLANS:
            if (wCode == LBN_SELCHANGE)
            {
                this->onPlanSelectionChanged();
            }
            return 0;

        case LIST_STEPS:
            if (wCode == LBN_SELCHANGE)
            {
                this->onStepSelectionChanged();
            }
            return 0;
    }

    return 0;
}

// ============================================================================
// Control Initialization
// ============================================================================

void Win32IDE_AgenticPlanningPanel::initializeControls()
{
    if (!m_hWnd)
        return;

    // Create tabs
    HWND hTabs = CreateWindowExW(0, L"SysTabControl32", nullptr, WS_CHILD | WS_VISIBLE, 10, 10, 580, 380, m_hWnd,
                                 (HMENU)TAB_MAIN, GetModuleHandle(nullptr), nullptr);

    TCITEMW ti = {};
    ti.mask = TCIF_TEXT;

    wchar_t tabPlans[] = L"Plans";
    ti.pszText = tabPlans;
    TabCtrl_InsertItem(hTabs, 0, &ti);

    wchar_t tabApprovals[] = L"Approval Queue";
    ti.pszText = tabApprovals;
    TabCtrl_InsertItem(hTabs, 1, &ti);

    wchar_t tabExecution[] = L"Execution Log";
    ti.pszText = tabExecution;
    TabCtrl_InsertItem(hTabs, 2, &ti);

    m_hTabControl = hTabs;

    // Tab 1: Plans list
    RECT rc = {20, 40, 570, 360};
    m_hPlansList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | LBS_STANDARD, rc.left,
                                   rc.top, rc.right - rc.left, rc.bottom - rc.top, m_hWnd, (HMENU)LIST_PLANS,
                                   GetModuleHandle(nullptr), nullptr);

    // Tab 2: Steps in selected plan
    m_hStepsList =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | LBS_STANDARD, rc.left, rc.top,
                        rc.right - rc.left, 200, m_hWnd, (HMENU)LIST_STEPS, GetModuleHandle(nullptr), nullptr);

    // Approval buttons
    CreateWindowW(L"BUTTON", L"Approve", WS_CHILD | BS_PUSHBUTTON, rc.left, rc.top + 210, 80, 25, m_hWnd,
                  (HMENU)BUTTON_APPROVE, GetModuleHandle(nullptr), nullptr);

    CreateWindowW(L"BUTTON", L"Reject", WS_CHILD | BS_PUSHBUTTON, rc.left + 90, rc.top + 210, 80, 25, m_hWnd,
                  (HMENU)BUTTON_REJECT, GetModuleHandle(nullptr), nullptr);

    CreateWindowW(L"BUTTON", L"Execute", WS_CHILD | BS_PUSHBUTTON, rc.left + 180, rc.top + 210, 80, 25, m_hWnd,
                  (HMENU)BUTTON_EXECUTE, GetModuleHandle(nullptr), nullptr);

    CreateWindowW(L"BUTTON", L"Rollback", WS_CHILD | BS_PUSHBUTTON, rc.left + 270, rc.top + 210, 80, 25, m_hWnd,
                  (HMENU)BUTTON_ROLLBACK, GetModuleHandle(nullptr), nullptr);

    CreateWindowW(L"BUTTON", L"Resume", WS_CHILD | BS_PUSHBUTTON, rc.left + 360, rc.top + 210, 80, 25, m_hWnd,
                  (HMENU)BUTTON_RESUME, GetModuleHandle(nullptr), nullptr);

    // Status text
    m_hStatusText =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_LEFT, rc.left, rc.top + 245,
                        rc.right - rc.left, 100, m_hWnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

// ============================================================================
// Refresh & UI Updates
// ============================================================================

void Win32IDE_AgenticPlanningPanel::refresh()
{
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRefreshTime).count();

    if (elapsed < m_refreshIntervalMs)
    {
        return;
    }

    m_lastRefreshTime = now;

    // Update plans list
    updatePlansList();

    // Update approval queue
    updateApprovalQueue();

    // Update status
    updateStatus();
}

void Win32IDE_AgenticPlanningPanel::updatePlansList()
{
    if (!m_hPlansList)
        return;

    ListBox_ResetContent(m_hPlansList);

    auto plans = m_orchestrator->getActivePlans();
    for (const auto* plan : plans)
    {
        std::wstringstream ss;
        ss << L"[" << (plan->plan_id.empty() ? L"?" : std::wstring(plan->plan_id.begin(), plan->plan_id.end())) << L"] "
           << (plan->description.empty() ? L"<no desc>"
                                         : std::wstring(plan->description.begin(), plan->description.end()))
           << L" (" << plan->steps.size() << L" steps)";

        ListBox_AddString(m_hPlansList, ss.str().c_str());
    }
}

void Win32IDE_AgenticPlanningPanel::updateApprovalQueue()
{
    auto pending = m_orchestrator->getPendingApprovals();

    std::wstringstream ss;
    ss << L"Pending Approvals: " << pending.size();
    SetWindowTextW(m_hStatusText, ss.str().c_str());
}

void Win32IDE_AgenticPlanningPanel::updateStatus()
{
    auto status_json = m_orchestrator->getExecutionStatusJson();
    // Could display more detailed status here
}

void Win32IDE_AgenticPlanningPanel::onPlanSelectionChanged()
{
    int sel = ListBox_GetCurSel(m_hPlansList);
    if (sel >= 0 && sel < static_cast<int>(m_orchestrator->getActivePlans().size()))
    {
        auto plans = m_orchestrator->getActivePlans();
        m_selectedPlanId = plans[sel]->plan_id;
        m_selectedStepIndex = -1;
        updateStepsList();
    }
}

void Win32IDE_AgenticPlanningPanel::onStepSelectionChanged()
{
    int sel = ListBox_GetCurSel(m_hStepsList);
    m_selectedStepIndex = sel;
}

void Win32IDE_AgenticPlanningPanel::updateStepsList()
{
    if (!m_hStepsList || m_selectedPlanId.empty())
    {
        ListBox_ResetContent(m_hStepsList);
        return;
    }

    auto* plan = m_orchestrator->getPlan(m_selectedPlanId);
    if (!plan)
    {
        ListBox_ResetContent(m_hStepsList);
        return;
    }

    ListBox_ResetContent(m_hStepsList);

    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        const auto& step = plan->steps[i];
        std::wstringstream ss;
        ss << L"[" << i << L"] " << (step.title.empty() ? L"?" : std::wstring(step.title.begin(), step.title.end()))
           << L" (risk: " << static_cast<int>(step.risk_level) << L")";
        ListBox_AddString(m_hStepsList, ss.str().c_str());
    }
}

// ============================================================================
// Button Click Handlers
// ============================================================================

void Win32IDE_AgenticPlanningPanel::onApproveClicked()
{
    if (m_selectedPlanId.empty() || m_selectedStepIndex < 0)
    {
        MessageBoxW(m_hWnd, L"Select a plan and step first", L"Info", MB_OK);
        return;
    }

    auto* plan = m_orchestrator->getPlan(m_selectedPlanId);
    if (plan && m_selectedStepIndex < static_cast<int>(plan->steps.size()))
    {
        m_orchestrator->approveStep(plan, m_selectedStepIndex, "user", "Approved via UI");
        addLogEntry("Approved step: " + plan->steps[m_selectedStepIndex].id);
        showToastStatus(L"Step approved");
        updateApprovalQueue();
        updateStepsList();
    }
}

void Win32IDE_AgenticPlanningPanel::onRejectClicked()
{
    if (m_selectedPlanId.empty() || m_selectedStepIndex < 0)
    {
        MessageBoxW(m_hWnd, L"Select a plan and step first", L"Info", MB_OK);
        return;
    }

    auto* plan = m_orchestrator->getPlan(m_selectedPlanId);
    if (plan && m_selectedStepIndex < static_cast<int>(plan->steps.size()))
    {
        m_orchestrator->rejectStep(plan, m_selectedStepIndex, "user", "Rejected via UI");
        addLogEntry("Rejected step: " + plan->steps[m_selectedStepIndex].id);
        showToastStatus(L"Step rejected");
        updateApprovalQueue();
        updateStepsList();
    }
}

void Win32IDE_AgenticPlanningPanel::onExecuteClicked()
{
    if (m_selectedPlanId.empty())
    {
        MessageBoxW(m_hWnd, L"Select a plan first", L"Info", MB_OK);
        return;
    }

    auto* plan = m_orchestrator->getPlan(m_selectedPlanId);
    if (plan)
    {
        m_orchestrator->executeEntirePlan(plan);
        addLogEntry("Plan execution started: " + m_selectedPlanId);
        showToastStatus(L"Plan execution started");
    }
}

void Win32IDE_AgenticPlanningPanel::onRollbackClicked()
{
    if (m_selectedPlanId.empty() || m_selectedStepIndex < 0)
    {
        MessageBoxW(m_hWnd, L"Select a plan and step first", L"Info", MB_OK);
        return;
    }

    auto* plan = m_orchestrator->getPlan(m_selectedPlanId);
    if (plan && m_selectedStepIndex < static_cast<int>(plan->steps.size()))
    {
        m_orchestrator->rollbackStep(plan, m_selectedStepIndex);
        updateStepsList();
        addLogEntry("Step rolled back: " + plan->steps[m_selectedStepIndex].id);
        showToastStatus(L"Rollback applied");
    }
}

void Win32IDE_AgenticPlanningPanel::onResumeClicked()
{
    if (m_selectedPlanId.empty())
    {
        MessageBoxW(m_hWnd, L"Select a plan first", L"Resume Plan", MB_OK | MB_ICONINFORMATION);
        return;
    }

    auto* plan = m_orchestrator->getPlan(m_selectedPlanId);
    if (!plan)
    {
        MessageBoxW(m_hWnd, L"Selected plan is no longer available.", L"Resume Plan", MB_OK | MB_ICONWARNING);
        return;
    }

    if (plan->is_executing.load())
    {
        MessageBoxW(m_hWnd, L"The selected plan is already executing.", L"Resume Plan", MB_OK | MB_ICONINFORMATION);
        return;
    }

    bool has_resumable_step = false;
    bool has_pending_approval = false;
    bool has_failed_step = false;
    for (const auto& step : plan->steps)
    {
        if (step.status == Agentic::ExecutionStatus::Failed)
        {
            has_failed_step = true;
        }
        if (step.status != Agentic::ExecutionStatus::Waiting)
        {
            continue;
        }
        if (step.approval_status == Agentic::ApprovalStatus::Pending)
        {
            has_pending_approval = true;
            continue;
        }
        if (step.approval_status == Agentic::ApprovalStatus::Approved ||
            step.approval_status == Agentic::ApprovalStatus::ApprovedAuto)
        {
            has_resumable_step = true;
        }
    }

    if (!has_resumable_step)
    {
        if (has_pending_approval)
        {
            MessageBoxW(m_hWnd, L"This plan still has steps waiting for approval.", L"Resume Plan",
                        MB_OK | MB_ICONINFORMATION);
        }
        else if (has_failed_step)
        {
            MessageBoxW(m_hWnd, L"This plan already contains a failed step. Review or roll back before resuming.",
                        L"Resume Plan", MB_OK | MB_ICONWARNING);
        }
        else
        {
            MessageBoxW(m_hWnd, L"There are no remaining approved steps to resume.", L"Resume Plan",
                        MB_OK | MB_ICONINFORMATION);
        }
        return;
    }

    m_orchestrator->executeEntirePlan(plan);
    addLogEntry("Plan resumed: " + m_selectedPlanId);
    showToastStatus(L"Plan resumed");
    updateApprovalQueue();
    updateStepsList();
}

// ============================================================================
// Logging & Events
// ============================================================================

void Win32IDE_AgenticPlanningPanel::addLogEntry(const std::string& entry)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logEntries.push_back(entry);

    // Keep last 1000 entries
    if (m_logEntries.size() > 1000)
    {
        m_logEntries.erase(m_logEntries.begin());
    }
}

void Win32IDE_AgenticPlanningPanel::onApprovalRequested(const Agentic::ExecutionPlan* plan, int step_idx)
{
    if (!plan || step_idx < 0 || step_idx >= static_cast<int>(plan->steps.size()))
    {
        return;
    }

    const auto& step = plan->steps[step_idx];
    std::string msg = "Approval required for step: " + step.title;
    addLogEntry(msg);

    std::wstringstream prompt;
    prompt << L"Plan: " << wideFromUtf8(plan->plan_id) << L"\n"
           << L"Step: " << wideFromUtf8(step.title) << L"\n\n"
           << L"Approve this step now?\n"
           << L"Yes = Approve, No = Reject, Cancel = Keep Pending";

    int choice = MessageBoxW(m_hWnd, prompt.str().c_str(), L"Approval Required", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDYES)
    {
        m_orchestrator->approveStep(plan, step_idx, "user", "Approved via approval modal");

        addLogEntry("Approved step via modal: " + step.id);
        showToastStatus(L"Step approved");
    }
    else if (choice == IDNO)
    {
        m_orchestrator->rejectStep(plan, step_idx, "user", "Rejected via approval modal");

        addLogEntry("Rejected step via modal: " + step.id);
        showToastStatus(L"Step rejected");
    }
    else
    {
        addLogEntry("Approval left pending: " + step.id);
    }

    updateApprovalQueue();
    updateStepsList();
}

void Win32IDE_AgenticPlanningPanel::onResize(int width, int height)
{
    if (m_hTabControl)
    {
        MoveWindow(m_hTabControl, 10, 10, width - 20, height - 20, TRUE);
    }
}

// ============================================================================
// Public API
// ============================================================================

Agentic::ExecutionPlan* Win32IDE_AgenticPlanningPanel::createPlanFromTask(const std::string& task)
{
    return m_orchestrator->generatePlanForTask(task);
}

std::vector<std::string> Win32IDE_AgenticPlanningPanel::getLogSnapshot()
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_logEntries;
}

void Win32IDE_AgenticPlanningPanel::showToastStatus(const std::wstring& text)
{
    if (m_hStatusText)
    {
        SetWindowTextW(m_hStatusText, text.c_str());
    }
    addLogEntry(narrowFromWide(text));
}

}  // namespace Win32IDE

extern "C" HWND Win32IDE_ShowAgenticPlanningPanel(HWND hParent)
{
    auto* panel = Win32IDE::GetAgenticPlanningPanel();
    if (!panel)
    {
        return nullptr;
    }

    HWND hwnd = panel->createWindow(hParent);
    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOW);
        SetFocus(hwnd);
    }

    return hwnd;
}
