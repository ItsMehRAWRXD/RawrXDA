// ============================================================================
// Win32IDE_PlanExecutor.cpp — Plan → Approve → Execute Agent Workflow
// ============================================================================
// Provides structured agent planning with user trust controls:
//   - Agent generates a structured plan (JSON steps)
//   - User sees plan in a dialog: steps, time estimates, confidence
//   - Approve / Reject / Modify workflow
//   - Step-by-step execution with progress, pause, skip, cancel
//   - Rollback on failure (restores file backups)
//   - Plan history for auditability
//
// This is the feature that builds agent trust — the user is always in control.
// ============================================================================

#include "../agentic/agentic_audit_sink.hpp"
#include "../agentic/agentic_orchestrator_integration.hpp"
#include "../full_agentic_ide/AgenticPlanningOrchestrator.h"
#include "IDELogger.h"
#include "Win32IDE.h"
#include "rawrxd/ide/omega_sdlc_phase.hpp"
#include <algorithm>
#include <chrono>
#include <commctrl.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <richedit.h>
#include <sstream>
#include <thread>

namespace
{

static Agentic::ExecutionPlan buildExecutionPlanFromWin32AgentPlan(const AgentPlan& ap,
                                                                   const std::string& workspaceRoot)
{
    Agentic::ExecutionPlan p;
    p.description = ap.goal;
    p.source_task = ap.goal;
    p.planner_model = "win32_full_agentic_ide";
    p.confidence_score = ap.overallConfidence;
    p.workspace_root = workspaceRoot;
    int totalMs = 0;
    for (const auto& s : ap.steps)
    {
        Agentic::PlanStep as;
        as.id = "step_" + std::to_string(s.id);
        as.title = s.title;
        as.description = s.description;
        if (!s.targetFile.empty())
        {
            as.affected_files.push_back(s.targetFile);
        }
        const int estMin = (std::max)(0, s.estimatedMinutes);
        as.estimated_duration_ms = (std::max)(1000, estMin * 60 * 1000);
        totalMs += as.estimated_duration_ms;

        switch (s.type)
        {
            case PlanStepType::CodeEdit:
            case PlanStepType::FileCreate:
            case PlanStepType::FileDelete:
            case PlanStepType::ShellCommand:
                as.is_mutating = true;
                break;
            default:
                as.is_mutating = false;
                break;
        }
        as.is_rollbackable = as.is_mutating && !s.targetFile.empty();
        as.complexity_score = std::clamp(1.0f - s.confidence, 0.0f, 1.0f);

        switch (s.riskTier)
        {
            case PlanRiskTier::Low:
                as.risk_level = Agentic::StepRisk::Low;
                break;
            case PlanRiskTier::Medium:
                as.risk_level = Agentic::StepRisk::Medium;
                break;
            case PlanRiskTier::High:
                as.risk_level = Agentic::StepRisk::High;
                break;
            case PlanRiskTier::Critical:
                as.risk_level = Agentic::StepRisk::Critical;
                break;
            default:
                as.risk_level = Agentic::StepRisk::Medium;
                break;
        }

        for (int dep : s.dependsOn)
        {
            as.dependencies.push_back("step_" + std::to_string(dep));
        }

        nlohmann::json toolj;
        toolj["agenticTool"] = "win32_bridge_plan_step";
        toolj["planStepType"] = static_cast<int>(s.type);
        toolj["title"] = s.title;
        toolj["description"] = s.description;
        toolj["targetFile"] = s.targetFile;
        as.actions.push_back(toolj.dump());

        p.steps.push_back(std::move(as));
    }
    p.total_estimated_duration_ms = totalMs;
    return p;
}

static void syncAgenticStepExecution(Agentic::AgenticPlanningOrchestrator* orch, const std::string& planId,
                                     int stepIndex, Agentic::ExecutionStatus st, const std::string& output)
{
    if (!orch || planId.empty())
    {
        return;
    }
    Agentic::ExecutionPlan* ep = orch->getPlan(planId);
    if (!ep || stepIndex < 0 || stepIndex >= static_cast<int>(ep->steps.size()))
    {
        return;
    }
    ep->steps[stepIndex].status = st;
    ep->steps[stepIndex].execution_result = output;
}

}  // namespace

// SCAFFOLD_081: generateAgentPlan and WM_PLAN_READY


// SCAFFOLD_080: Plan rollback and file backup


// SCAFFOLD_079: Plan execution progress and pause


// SCAFFOLD_078: Plan dialog Approve/Reject/Edit


// SCAFFOLD_077: Plan step parsing (STEP/DESC/TYPE)


// SCAFFOLD_019: Plan step execution and rollback


// SCAFFOLD_018: Plan approval dialog and list view


// ============================================================================
// PLAN STEP EXECUTION — runs a single step via the agent (shared: UI plan + Agentic tools)
// ============================================================================

std::string Win32IDE::executeAgentPlanStepViaBridge(const PlanStep& step)
{
    AgenticBridge* bridge = m_agenticBridge;
    if (!bridge || !bridge->IsInitialized())
        return "[Error] Agent not initialized";

    std::string prompt;
    switch (step.type)
    {
        case PlanStepType::CodeEdit:
            prompt =
                "Execute exactly: " + step.description + (step.targetFile.empty() ? "" : "\nFile: " + step.targetFile);
            break;
        case PlanStepType::FileCreate:
            prompt = "Create file: " + step.targetFile + "\n" + step.description;
            break;
        case PlanStepType::FileDelete:
            prompt = "Delete file: " + step.targetFile;
            break;
        case PlanStepType::ShellCommand:
            prompt = "Execute shell command: " + step.description;
            break;
        case PlanStepType::Analysis:
            prompt = "Analyze: " + step.description;
            break;
        case PlanStepType::Verification:
            prompt = "Verify: " + step.description;
            break;
        default:
            prompt = step.description;
            break;
    }

    AgentResponse response = bridge->ExecuteAgentCommand(prompt);
    return response.content;
}

// ============================================================================
// GENERATE PLAN — ask the agent to create a structured plan
// ============================================================================

void Win32IDE::generateAgentPlan(const std::string& goal)
{
    if (!m_agenticBridge || !m_agenticBridge->IsInitialized())
    {
        appendToOutput("[Plan] Agent not initialized. Configure a model first.", "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[Plan] Generating plan for: " + goal, "General", OutputSeverity::Info);
    LOG_INFO(std::string("OmegaSDLC ") +
             rawrxd::ide::omegaStructuredLog(rawrxd::ide::OmegaSdlcPhase::Plan, "generateAgentPlan"));

    // Reset current plan
    m_activeAgenticPlanId.clear();
    m_currentPlan.goal = goal;
    m_currentPlan.steps.clear();
    m_currentPlan.status = PlanStatus::Generating;
    m_currentPlan.currentStepIndex = -1;
    m_currentPlan.overallConfidence = 0.0f;

    // Background thread — ask agent for structured plan
    std::string goalCopy = goal;
    std::thread(
        [this, goalCopy]()
        {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled)
                return;
            std::string planPrompt =
                "Create a detailed step-by-step plan to accomplish the following goal.\n"
                "For each step, provide:\n"
                "  - A short title (one line)\n"
                "  - A description of what to do\n"
                "  - Estimated time (in minutes)\n"
                "  - Confidence level (0.0 to 1.0)\n"
                "  - Risk level (low/medium/high) — REQUIRED; used for safety gates (low-risk-only execution)\n"
                "  - Type: code_edit, file_create, file_delete, shell_command, analysis, or verification\n"
                "  - Target file (if applicable)\n\n"
                "Format each step as:\n"
                "STEP: <title>\n"
                "DESC: <description>\n"
                "TIME: <minutes>\n"
                "CONF: <0.0-1.0>\n"
                "RISK: <low|medium|high>\n"
                "TYPE: <type>\n"
                "FILE: <path or none>\n"
                "---\n\n"
                "Goal: " +
                goalCopy;

            AgentResponse response = m_agenticBridge->ExecuteAgentCommand(planPrompt);

            // Parse the plan from agent output
            std::vector<PlanStep> steps = parsePlanSteps(response.content);

            // Post to UI thread
            PlanStep* heapSteps = nullptr;
            int stepCount = (int)steps.size();
            if (stepCount > 0)
            {
                heapSteps = new PlanStep[stepCount];
                for (int i = 0; i < stepCount; i++)
                {
                    heapSteps[i] = steps[i];
                }
            }
            PostMessageA(m_hwndMain, WM_PLAN_READY, (WPARAM)stepCount, (LPARAM)heapSteps);
        })
        .detach();
}

// ============================================================================
// PLAN READY — delivered to UI thread
// ============================================================================

void Win32IDE::onPlanReady(int stepCount, PlanStep* steps)
{
    if (stepCount <= 0 || !steps)
    {
        m_currentPlan.status = PlanStatus::Failed;
        appendToOutput("[Plan] Failed to generate plan — no steps parsed.", "General", OutputSeverity::Error);
        return;
    }

    m_currentPlan.steps.clear();
    float totalConf = 0.0f;
    int totalTime = 0;

    for (int i = 0; i < stepCount; i++)
    {
        steps[i].id = i + 1;
        steps[i].status = PlanStepStatus::Pending;
        m_currentPlan.steps.push_back(steps[i]);
        totalConf += steps[i].confidence;
        totalTime += steps[i].estimatedMinutes;
    }
    delete[] steps;

    m_currentPlan.overallConfidence = totalConf / stepCount;
    m_currentPlan.status = PlanStatus::AwaitingApproval;

    const std::string wsRoot = m_projectRoot.empty() ? m_currentDirectory : m_projectRoot;
    {
        full_agentic_ide::AgenticPlanningOrchestrator::classifyPlan(m_currentPlan);
        full_agentic_ide::AgenticPlanningOrchestrator::applyWorkspaceSafetyGates(m_currentPlan, wsRoot);
        appendToOutput(std::string("[Plan] ") +
                           full_agentic_ide::AgenticPlanningOrchestrator::formatGateSummary(m_currentPlan),
                       "General", OutputSeverity::Info);
    }

    // Unified Agentic orchestrator (approval queue + JSON export) — same plan as full_agentic_ide gates
    {
        Agentic::OrchestratorIntegration::instance().initialize();
        Agentic::AgenticPlanningOrchestrator* orch = Agentic::OrchestratorIntegration::instance().getOrchestrator();
        if (orch)
        {
            Agentic::ExecutionPlan ep = buildExecutionPlanFromWin32AgentPlan(m_currentPlan, wsRoot);
            Agentic::ExecutionPlan* stored = orch->ingestExternalPlan(std::move(ep));
            m_activeAgenticPlanId = stored ? stored->plan_id : std::string();
            if (stored)
            {
                std::vector<std::uint8_t> gates;
                std::vector<std::string> details;
                gates.reserve(m_currentPlan.steps.size());
                details.reserve(m_currentPlan.steps.size());
                for (const auto& s : m_currentPlan.steps)
                {
                    gates.push_back(static_cast<std::uint8_t>(s.mutationGate));
                    details.push_back(s.gateDetail);
                }
                orch->applyWin32MutationGateSnapshot(stored, gates, details);
                appendToOutput(std::string("[Plan] Agentic orchestrator: ") + stored->toSummary(), "General",
                               OutputSeverity::Info);
            }
        }
    }

    // Record plan generation event
    recordEvent(AgentEventType::PlanGenerated, "", m_currentPlan.goal,
                std::to_string(stepCount) + " steps, ~" + std::to_string(totalTime) + " min", 0, true, "",
                "{\"steps\":" + std::to_string(stepCount) +
                    ",\"confidence\":" + std::to_string((int)(m_currentPlan.overallConfidence * 100)) + "}");

    // Show plan approval dialog
    showPlanApprovalDialog();

    // Notify chat that plan is ready for review (design spec #5)
    appendToOutput("[Plan] Plan ready. Approve or reject in the dialog.\n", "Agent", OutputSeverity::Info);
}

// ============================================================================
// PLAN APPROVAL DIALOG — full custom Win32 dialog
// ============================================================================
// Layout (dark themed, 700x520):
//   ┌─────────────────────────────────────────────────────┐
//   │ [Goal label]                                        │
//   │ [Summary: 5 steps · ~12 min · 87% confidence]       │
//   ├─────────────────────────────────────────────────────┤
//   │  # │ Status │ Step Title        │ Type  │ Time │ %  │ ← ListView
//   │  1 │   ⬜   │ Parse config      │ Code  │  2m  │85% │
//   │  2 │   ⬜   │ Add new handler   │ Code  │  3m  │90% │
//   │  ...                                                │
//   ├─────────────────────────────────────────────────────┤
//   │ [Detail panel — description + target file + risk]   │
//   ├─────────────────────────────────────────────────────┤
//   │ [Progress bar]  [Progress label]                    │
//   │ [✓ Approve]  [✎ Edit Step]  [✗ Reject]  [Pause] [X]│
//   └─────────────────────────────────────────────────────┘

void Win32IDE::showPlanApprovalDialog()
{
    if (m_currentPlan.steps.empty())
        return;

    // Close any existing plan dialog
    closePlanDialog();

    // Dialog dimensions (extra width for dual approval buttons)
    const int dlgW = 780, dlgH = 560;

    // Center on main window
    RECT mainRC;
    GetWindowRect(m_hwndMain, &mainRC);
    int cx = (mainRC.left + mainRC.right) / 2 - dlgW / 2;
    int cy = (mainRC.top + mainRC.bottom) / 2 - dlgH / 2;

    // Create dark background brush
    if (m_planDialogBrush)
        DeleteObject(m_planDialogBrush);
    m_planDialogBrush = CreateSolidBrush(RGB(30, 30, 30));

    // Register a window class for the plan dialog (once)
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT
        {
            Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            switch (uMsg)
            {
                case WM_COMMAND:
                    if (ide)
                        ide->onPlanDialogCommand(LOWORD(wParam));
                    return 0;
                case WM_NOTIFY:
                {
                    NMHDR* nm = reinterpret_cast<NMHDR*>(lParam);
                    if (nm && nm->code == LVN_ITEMCHANGED && ide)
                    {
                        ide->onPlanListSelChanged();
                    }
                    return 0;
                }
                case WM_CTLCOLORSTATIC:
                case WM_CTLCOLOREDIT:
                case WM_CTLCOLORLISTBOX:
                {
                    HDC hdcCtrl = (HDC)wParam;
                    SetTextColor(hdcCtrl, RGB(212, 212, 212));
                    SetBkColor(hdcCtrl, RGB(37, 37, 38));
                    if (ide && ide->m_planDialogBrush)
                        return (LRESULT)ide->m_planDialogBrush;
                    return (LRESULT)GetStockObject(BLACK_BRUSH);
                }
                case WM_ERASEBKGND:
                {
                    HDC hdc = (HDC)wParam;
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
                    FillRect(hdc, &rc, br);
                    DeleteObject(br);
                    return 1;
                }
                case WM_CLOSE:
                    if (ide)
                        ide->closePlanDialog();
                    return 0;
            }
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
        };
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = "RawrXD_PlanDialog";
        RegisterClassExA(&wc);
        classRegistered = true;
    }

    // Create the dialog window
    m_hwndPlanDialog = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "RawrXD_PlanDialog",
                                       "Agent Plan — Review & Approve", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                       cx, cy, dlgW, dlgH, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!m_hwndPlanDialog)
    {
        LOG_ERROR("Failed to create Plan Approval Dialog");
        return;
    }

    SetWindowLongPtrA(m_hwndPlanDialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Margin constants
    const int M = 12;                 // Outer margin
    const int W = dlgW - 2 * M - 16;  // Usable width (account for frame)

    int y = M;

    // ── Goal label ──
    m_hwndPlanGoalLabel = CreateWindowExA(0, "STATIC", ("Plan: " + m_currentPlan.goal).c_str(),
                                          WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, M, y, W, 20, m_hwndPlanDialog,
                                          (HMENU)(LONG_PTR)IDC_PLAN_GOAL_LABEL, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanGoalLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 24;

    // ── Summary label ──
    int totalMinutes = 0;
    for (const auto& s : m_currentPlan.steps)
        totalMinutes += s.estimatedMinutes;
    char summaryBuf[384];
    snprintf(summaryBuf, sizeof(summaryBuf), "%d steps  ·  ~%d min  ·  %d%% conf  ·  %s",
             (int)m_currentPlan.steps.size(), totalMinutes, (int)(m_currentPlan.overallConfidence * 100),
             full_agentic_ide::AgenticPlanningOrchestrator::formatGateSummary(m_currentPlan).c_str());

    m_hwndPlanSummaryLabel =
        CreateWindowExA(0, "STATIC", summaryBuf, WS_CHILD | WS_VISIBLE | SS_LEFT, M, y, W, 18, m_hwndPlanDialog,
                        (HMENU)(LONG_PTR)IDC_PLAN_SUMMARY_LABEL, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanSummaryLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 26;

    // ── ListView for plan steps ──
    int listH = 200;
    m_hwndPlanList =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER, M, y,
                        W, listH, m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_LIST, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Extended styles: full-row select, gridlines
    ListView_SetExtendedListViewStyle(m_hwndPlanList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Dark background for ListView
    ListView_SetBkColor(m_hwndPlanList, RGB(37, 37, 38));
    ListView_SetTextBkColor(m_hwndPlanList, RGB(37, 37, 38));
    ListView_SetTextColor(m_hwndPlanList, RGB(212, 212, 212));

    // Add columns: #, Status, Title, Type, Time, Confidence, Risk
    auto addCol = [&](int idx, const char* text, int width)
    {
        LVCOLUMNA col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = LVCFMT_LEFT;
        col.cx = width;
        col.pszText = const_cast<char*>(text);
        ListView_InsertColumn(m_hwndPlanList, idx, &col);
    };
    addCol(0, "#", 32);
    addCol(1, "Status", 56);
    addCol(2, "Step", 200);
    addCol(3, "Type", 72);
    addCol(4, "Time", 44);
    addCol(5, "Conf", 44);
    addCol(6, "Risk", 52);
    addCol(7, "Gate", 44);

    y += listH + 6;

    // ── Detail panel (description + file + risk) ──
    int detailH = 100;
    m_hwndPlanDetail = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        M, y, W, detailH, m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_DETAIL, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanDetail, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += detailH + 6;

    // ── Progress bar ──
    m_hwndPlanProgress = CreateWindowExA(0, PROGRESS_CLASSA, "", WS_CHILD | PBS_SMOOTH, M, y, W - 200, 18,
                                         m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_PROGRESS, m_hInstance, nullptr);
    SendMessage(m_hwndPlanProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
    SendMessage(m_hwndPlanProgress, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 122, 204));
    SendMessage(m_hwndPlanProgress, PBM_SETBKCOLOR, 0, (LPARAM)RGB(50, 50, 50));

    m_hwndPlanProgressLabel =
        CreateWindowExA(0, "STATIC", "", WS_CHILD | SS_LEFT, M + W - 190, y, 190, 18, m_hwndPlanDialog,
                        (HMENU)(LONG_PTR)IDC_PLAN_PROGRESS_LABEL, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanProgressLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 26;

    // ── Action buttons ──
    int btnW = 102, btnH = 30, btnGap = 6;
    int btnY = y;
    int bx = M;
    const int btnSafeW = btnW + 18;

    m_hwndPlanBtnApprove =
        CreateWindowExA(0, "BUTTON", "Approve all", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, bx, btnY, btnW, btnH,
                        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_APPROVE, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanBtnApprove, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    bx += btnW + btnGap;

    m_hwndPlanBtnApproveSafe =
        CreateWindowExA(0, "BUTTON", "Low-risk only", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, bx, btnY, btnSafeW, btnH,
                        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_APPROVE_SAFE, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanBtnApproveSafe, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    bx += btnSafeW + btnGap;

    m_hwndPlanBtnEdit =
        CreateWindowExA(0, "BUTTON", "\xe2\x9c\x8e Edit Step", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, bx, btnY, btnW,
                        btnH, m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_EDIT, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanBtnEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    bx += btnW + btnGap;

    m_hwndPlanBtnReject =
        CreateWindowExA(0, "BUTTON", "\xe2\x9c\x97 Reject", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, bx, btnY, btnW, btnH,
                        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_REJECT, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanBtnReject, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    bx += btnW + btnGap;

    m_hwndPlanBtnPause = CreateWindowExA(0, "BUTTON", "Pause", WS_CHILD | BS_PUSHBUTTON, bx, btnY, 68, btnH,
                                         m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_PAUSE, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanBtnPause, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    bx += 68 + btnGap;

    m_hwndPlanBtnCancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | BS_PUSHBUTTON, bx, btnY, 68, btnH,
                                          m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_CANCEL, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(m_hwndPlanBtnCancel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Populate the list view with plan steps
    populatePlanListView();

    // Select first step to show detail
    ListView_SetItemState(m_hwndPlanList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    // Make modal
    EnableWindow(m_hwndMain, FALSE);
    SetForegroundWindow(m_hwndPlanDialog);

    LOG_INFO("Plan Approval Dialog shown: " + std::to_string(m_currentPlan.steps.size()) + " steps");
}

// ============================================================================
// AGENTIC ORCHESTRATOR SYNC — Win32 AgentPlan ↔ Agentic::ExecutionPlan
// ============================================================================

void Win32IDE::syncActiveAgenticPlanApprovalsFromUi()
{
    Agentic::OrchestratorIntegration::instance().initialize();
    Agentic::AgenticPlanningOrchestrator* orch = Agentic::OrchestratorIntegration::instance().getOrchestrator();
    if (!orch || m_activeAgenticPlanId.empty())
        return;
    Agentic::ExecutionPlan* stored = orch->getPlan(m_activeAgenticPlanId);
    if (!stored || stored->steps.size() != m_currentPlan.steps.size())
        return;
    std::vector<std::uint8_t> gates;
    std::vector<std::string> details;
    gates.reserve(m_currentPlan.steps.size());
    details.reserve(m_currentPlan.steps.size());
    for (const auto& s : m_currentPlan.steps)
    {
        gates.push_back(static_cast<std::uint8_t>(s.mutationGate));
        details.push_back(s.gateDetail);
    }
    orch->applyWin32MutationGateSnapshot(stored, gates, details);
}

void Win32IDE::rejectPendingStepsInActiveAgenticPlan()
{
    Agentic::OrchestratorIntegration::instance().initialize();
    Agentic::AgenticPlanningOrchestrator* orch = Agentic::OrchestratorIntegration::instance().getOrchestrator();
    if (!orch || m_activeAgenticPlanId.empty())
        return;
    Agentic::ExecutionPlan* stored = orch->getPlan(m_activeAgenticPlanId);
    if (!stored)
        return;
    for (int i = 0; i < static_cast<int>(stored->steps.size()); ++i)
    {
        if (stored->steps[i].approval_status == Agentic::ApprovalStatus::Pending)
        {
            orch->rejectStep(stored, i, "ide_user", "Plan rejected in approval dialog");
        }
    }
}

// ============================================================================
// EXECUTE PLAN — step-by-step with progress
// ============================================================================

void Win32IDE::executePlan()
{
    if (m_currentPlan.status != PlanStatus::Approved)
        return;
    if (m_currentPlan.steps.empty())
        return;

    m_currentPlan.status = PlanStatus::Executing;
    m_planExecutionCancelled.store(false);
    m_planExecutionPaused.store(false);

    // Show progress
    showModelProgressBar("Executing plan: " + m_currentPlan.goal);

    std::thread(
        [this]()
        {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled)
                return;
            LOG_INFO(std::string("OmegaSDLC ") +
                     rawrxd::ide::omegaStructuredLog(rawrxd::ide::OmegaSdlcPhase::Mutate, "executePlan_thread"));
            int totalSteps = (int)m_currentPlan.steps.size();
            const std::string agenticPlanIdCopy = m_activeAgenticPlanId;
            Agentic::OrchestratorIntegration::instance().initialize();
            Agentic::AgenticPlanningOrchestrator* agenticOrch =
                Agentic::OrchestratorIntegration::instance().getOrchestrator();

            for (int i = 0; i < totalSteps; i++)
            {
                // Check for cancellation or shutdown
                if (m_planExecutionCancelled.load() || isShuttingDown())
                {
                    m_currentPlan.steps[i].status = PlanStepStatus::Skipped;
                    for (int j = i + 1; j < totalSteps; j++)
                    {
                        m_currentPlan.steps[j].status = PlanStepStatus::Skipped;
                    }
                    PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)2);  // 2 = cancelled
                    break;
                }

                if (!full_agentic_ide::AgenticPlanningOrchestrator::stepShouldExecute(m_currentPlan, i))
                {
                    PlanStep& st = m_currentPlan.steps[i];
                    st.status = PlanStepStatus::Skipped;
                    if (st.mutationGate == PlanMutationGate::SafetyBlocked)
                        st.output = std::string("[Skipped] ") + st.gateDetail;
                    else if (st.mutationGate == PlanMutationGate::SkippedByPolicy)
                        st.output = "[Skipped] Not in low-risk execution scope.";
                    else
                        st.output = "[Skipped] Step not approved for execution.";
                    PostMessageA(m_hwndMain, WM_APP + 503, i, (LPARAM)PlanStepStatus::Skipped);
                    PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)3);  // 3 = gated skip
                    continue;
                }

                // Check for pause
                while (m_planExecutionPaused.load() && !m_planExecutionCancelled.load())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                m_currentPlan.currentStepIndex = i;
                m_currentPlan.steps[i].status = PlanStepStatus::Running;

                // Update plan dialog ListView live
                PostMessageA(m_hwndMain, WM_APP + 503, i, (LPARAM)PlanStepStatus::Running);

                // Update progress on UI thread
                float percent = ((float)i / totalSteps) * 100.0f;
                updateModelProgress(percent, "Step " + std::to_string(i + 1) + "/" + std::to_string(totalSteps) + ": " +
                                                 m_currentPlan.steps[i].title);

                // Create file backup before code edits (for rollback)
                std::string backupContent;
                if (m_currentPlan.steps[i].type == PlanStepType::CodeEdit && !m_currentPlan.steps[i].targetFile.empty())
                {
                    std::ifstream f(m_currentPlan.steps[i].targetFile);
                    if (f)
                    {
                        std::ostringstream buf;
                        buf << f.rdbuf();
                        backupContent = buf.str();
                    }
                }

                // Execute the step
                std::string result = executeAgentPlanStepViaBridge(m_currentPlan.steps[i]);

                // Phase 4B: Choke Point 1 — hookPlanStepOutput after each step
                FailureClassification stepFailure = hookPlanStepOutput(i, result);

                if (stepFailure.reason != AgentFailureType::None)
                {
                    // Failure detected — attempt bounded retry with user approval
                    if (showRetryApprovalInPlanDialog(i, stepFailure))
                    {
                        std::string retryPrompt = buildRetryPrompt(stepFailure, m_currentPlan.steps[i].title);
                        std::string retryResult = executeAgentPlanStepViaBridge(m_currentPlan.steps[i]);
                        FailureClassification retryCheck = hookPlanStepOutput(i, retryResult);
                        if (retryCheck.reason == AgentFailureType::None)
                        {
                            // Retry succeeded
                            result = retryResult;
                            recordSimpleEvent(AgentEventType::FailureCorrected,
                                              "Step " + std::to_string(i + 1) +
                                                  " retry succeeded: " + failureTypeString(stepFailure.reason));
                        }
                        else
                        {
                            // Retry also failed — fall through to normal error handling
                            result = retryResult;
                            recordSimpleEvent(AgentEventType::FailureFailed,
                                              "Step " + std::to_string(i + 1) +
                                                  " retry also failed: " + failureTypeString(retryCheck.reason));
                        }
                    }
                    // If user declined retry, fall through with original result
                }

                if (result.find("[Error]") != std::string::npos || result.find("error") != std::string::npos)
                {
                    m_currentPlan.steps[i].status = PlanStepStatus::Failed;
                    m_currentPlan.steps[i].output = result;

                    // Rollback file if we backed it up
                    if (!backupContent.empty() && !m_currentPlan.steps[i].targetFile.empty())
                    {
                        std::ofstream restore(m_currentPlan.steps[i].targetFile);
                        if (restore)
                        {
                            restore << backupContent;
                            restore.close();
                        }
                    }

                    syncAgenticStepExecution(agenticOrch, agenticPlanIdCopy, i, Agentic::ExecutionStatus::Failed,
                                             result);
                    if (agenticOrch)
                    {
                        Agentic::ExecutionPlan* ep = agenticOrch->getPlan(agenticPlanIdCopy);
                        if (ep)
                            agenticOrch->rollbackStep(ep, i);
                    }

                    PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)0);  // 0 = failed
                    // Don't continue on failure — ask user
                    break;
                }
                else
                {
                    m_currentPlan.steps[i].status = PlanStepStatus::Completed;
                    m_currentPlan.steps[i].output = result;
                    syncAgenticStepExecution(agenticOrch, agenticPlanIdCopy, i, Agentic::ExecutionStatus::Success,
                                             result);
                    PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)1);  // 1 = success
                }
            }

            bool anyFailed = false;
            for (const auto& step : m_currentPlan.steps)
            {
                if (step.status == PlanStepStatus::Failed)
                    anyFailed = true;
            }
            const bool cancelled = m_planExecutionCancelled.load();
            PostMessageA(m_hwndMain, WM_PLAN_COMPLETE, (!cancelled && !anyFailed) ? 1 : 0, 0);
        })
        .detach();
}

// ============================================================================
// PLAN STEP DONE — UI thread handler
// ============================================================================

void Win32IDE::onPlanStepDone(int stepIndex, int result)
{
    if (stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size())
        return;

    const auto& step = m_currentPlan.steps[stepIndex];

    // Update plan dialog ListView with current step status
    updatePlanStepInDialog(stepIndex, step.status);

    if (result == 1)
    {
        appendToOutput("[Plan] ✓ Step " + std::to_string(stepIndex + 1) + ": " + step.title, "General",
                       OutputSeverity::Info);
    }
    else if (result == 3)
    {
        appendToOutput("[Plan] ⊘ Step " + std::to_string(stepIndex + 1) + " skipped (gate/policy): " + step.title,
                       "General", OutputSeverity::Info);
    }
    else if (result == 0)
    {
        appendToOutput("[Plan] ✗ Step " + std::to_string(stepIndex + 1) + " failed: " + step.title, "General",
                       OutputSeverity::Error);
        appendToOutput("[Plan] Output: " + step.output, "General", OutputSeverity::Error);

        // Ask user whether to continue or abort
        int choice = MessageBoxA(
            m_hwndMain,
            ("Step " + std::to_string(stepIndex + 1) + " failed:\n" + step.title + "\n\nContinue with remaining steps?")
                .c_str(),
            "Plan Step Failed", MB_YESNO | MB_ICONWARNING);

        if (choice == IDYES)
        {
            // Mark failed step and continue
            std::thread(
                [this, stepIndex]()
                {
                    DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                    if (_guard.cancelled)
                        return;
                    // Resume from next step
                    int totalSteps = (int)m_currentPlan.steps.size();
                    for (int i = stepIndex + 1; i < totalSteps; i++)
                    {
                        if (m_planExecutionCancelled.load() || isShuttingDown())
                            break;
                        m_currentPlan.currentStepIndex = i;
                        m_currentPlan.steps[i].status = PlanStepStatus::Running;

                        float percent = ((float)i / totalSteps) * 100.0f;
                        updateModelProgress(percent, "Step " + std::to_string(i + 1) + "/" +
                                                         std::to_string(totalSteps) + ": " +
                                                         m_currentPlan.steps[i].title);

                        std::string result = executeAgentPlanStepViaBridge(m_currentPlan.steps[i]);

                        // Phase 4B: Choke Point 2 — hookPlanStepOutput in resume loop
                        FailureClassification resumeFailure = hookPlanStepOutput(i, result);
                        if (resumeFailure.reason != AgentFailureType::None)
                        {
                            if (showRetryApprovalInPlanDialog(i, resumeFailure))
                            {
                                std::string retryResult = executeAgentPlanStepViaBridge(m_currentPlan.steps[i]);
                                FailureClassification retryCheck = hookPlanStepOutput(i, retryResult);
                                if (retryCheck.reason == AgentFailureType::None)
                                {
                                    result = retryResult;
                                    recordSimpleEvent(AgentEventType::FailureCorrected,
                                                      "Resume step " + std::to_string(i + 1) + " retry succeeded");
                                }
                                else
                                {
                                    result = retryResult;
                                    recordSimpleEvent(AgentEventType::FailureFailed,
                                                      "Resume step " + std::to_string(i + 1) + " retry failed");
                                }
                            }
                        }

                        m_currentPlan.steps[i].status = PlanStepStatus::Completed;
                        m_currentPlan.steps[i].output = result;
                        PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)1);
                    }
                    PostMessageA(m_hwndMain, WM_PLAN_COMPLETE, 1, 0);
                })
                .detach();
        }
        else
        {
            cancelPlan();
        }
    }
    else
    {
        appendToOutput("[Plan] Cancelled at step " + std::to_string(stepIndex + 1), "General", OutputSeverity::Warning);
    }
}

// ============================================================================
// PLAN COMPLETE — UI thread handler
// ============================================================================

void Win32IDE::onPlanComplete(bool success)
{
    hideModelProgressBar();

    if (success)
    {
        m_currentPlan.status = PlanStatus::Completed;
        appendToOutput("[Plan] ✓ All steps completed successfully!", "General", OutputSeverity::Info);
        LOG_INFO(std::string("OmegaSDLC ") +
                 rawrxd::ide::omegaStructuredLog(rawrxd::ide::OmegaSdlcPhase::Verify, "plan_verify_ok"));
        LOG_INFO(std::string("OmegaSDLC ") +
                 rawrxd::ide::omegaStructuredLog(rawrxd::ide::OmegaSdlcPhase::Ship, "plan_ship_complete"));
    }
    else
    {
        m_currentPlan.status = PlanStatus::Failed;
        appendToOutput("[Plan] Plan completed with failures.", "General", OutputSeverity::Warning);
        LOG_INFO(std::string("OmegaSDLC ") +
                 rawrxd::ide::omegaStructuredLog(rawrxd::ide::OmegaSdlcPhase::Verify, "plan_verify_failed"));
    }

    // Update the plan dialog if it's still open
    if (m_hwndPlanDialog && IsWindow(m_hwndPlanDialog))
    {
        SetWindowTextA(m_hwndPlanDialog, success ? "Agent Plan - Completed" : "Agent Plan - Completed with Failures");
        // Hide execution buttons, show close
        if (m_hwndPlanBtnPause)
            ShowWindow(m_hwndPlanBtnPause, SW_HIDE);
        if (m_hwndPlanBtnCancel)
            ShowWindow(m_hwndPlanBtnCancel, SW_HIDE);
        if (m_hwndPlanBtnReject)
        {
            SetWindowTextA(m_hwndPlanBtnReject, "Close");
            ShowWindow(m_hwndPlanBtnReject, SW_SHOW);
        }
        // Set progress bar to 100% on success
        if (success && m_hwndPlanProgress)
            SendMessage(m_hwndPlanProgress, PBM_SETPOS, 1000, 0);
        if (m_hwndPlanProgressLabel)
            SetWindowTextA(m_hwndPlanProgressLabel, success ? "Complete" : "Failed");
    }

    // Store in plan history
    m_planHistory.push_back(m_currentPlan);
    if (m_planHistory.size() > MAX_PLAN_HISTORY)
    {
        m_planHistory.erase(m_planHistory.begin());
    }
}

// ============================================================================
// CANCEL / PAUSE / RESUME
// ============================================================================

void Win32IDE::cancelPlan()
{
    m_planExecutionCancelled.store(true);
    m_currentPlan.status = PlanStatus::Failed;
    hideModelProgressBar();
    appendToOutput("[Plan] Execution cancelled by user.", "General", OutputSeverity::Warning);
}

void Win32IDE::pausePlan()
{
    m_planExecutionPaused.store(true);
    appendToOutput("[Plan] Execution paused.", "General", OutputSeverity::Info);
}

void Win32IDE::resumePlan()
{
    m_planExecutionPaused.store(false);
    appendToOutput("[Plan] Execution resumed.", "General", OutputSeverity::Info);
}

// ============================================================================
// PLAN STATUS STRING
// ============================================================================

std::string Win32IDE::getPlanStatusString() const
{
    std::ostringstream oss;
    oss << "Plan: " << m_currentPlan.goal << "\r\n";
    oss << "Status: ";
    switch (m_currentPlan.status)
    {
        case PlanStatus::None:
            oss << "No plan";
            break;
        case PlanStatus::Generating:
            oss << "Generating...";
            break;
        case PlanStatus::AwaitingApproval:
            oss << "Awaiting approval";
            break;
        case PlanStatus::Approved:
            oss << "Approved";
            break;
        case PlanStatus::Rejected:
            oss << "Rejected";
            break;
        case PlanStatus::Executing:
            oss << "Executing...";
            break;
        case PlanStatus::Completed:
            oss << "Completed";
            break;
        case PlanStatus::Failed:
            oss << "Failed";
            break;
    }
    oss << "\r\nSteps: " << m_currentPlan.steps.size()
        << " | Confidence: " << (int)(m_currentPlan.overallConfidence * 100) << "%\r\n";

    for (const auto& step : m_currentPlan.steps)
    {
        oss << "  [" << step.id << "] ";
        switch (step.status)
        {
            case PlanStepStatus::Pending:
                oss << "⬜ ";
                break;
            case PlanStepStatus::Running:
                oss << "🔄 ";
                break;
            case PlanStepStatus::Completed:
                oss << "✅ ";
                break;
            case PlanStepStatus::Failed:
                oss << "❌ ";
                break;
            case PlanStepStatus::Skipped:
                oss << "⏭️ ";
                break;
        }
        oss << step.title << "\r\n";
    }

    return oss.str();
}

// ============================================================================
// PARSE PLAN STEPS — from agent output
// ============================================================================

std::vector<PlanStep> Win32IDE::parsePlanSteps(const std::string& agentOutput)
{
    std::vector<PlanStep> steps;
    PlanStep current;
    bool inStep = false;
    int stepId = 0;

    std::istringstream stream(agentOutput);
    std::string line;

    while (std::getline(stream, line))
    {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);

        if (line.substr(0, 5) == "STEP:" || line.substr(0, 5) == "Step:")
        {
            if (inStep && !current.title.empty())
            {
                current.id = ++stepId;
                steps.push_back(current);
            }
            current = PlanStep();
            current.title = line.substr(5);
            // Trim title
            size_t ts = current.title.find_first_not_of(" \t");
            if (ts != std::string::npos)
                current.title = current.title.substr(ts);
            inStep = true;
        }
        else if (inStep && (line.substr(0, 5) == "DESC:" || line.substr(0, 5) == "Desc:"))
        {
            current.description = line.substr(5);
            size_t ts = current.description.find_first_not_of(" \t");
            if (ts != std::string::npos)
                current.description = current.description.substr(ts);
        }
        else if (inStep && (line.substr(0, 5) == "TIME:" || line.substr(0, 5) == "Time:"))
        {
            std::string val = line.substr(5);
            try
            {
                current.estimatedMinutes = std::stoi(val);
            }
            catch (...)
            {
                current.estimatedMinutes = 1;
            }
        }
        else if (inStep && (line.substr(0, 5) == "CONF:" || line.substr(0, 5) == "Conf:"))
        {
            std::string val = line.substr(5);
            try
            {
                current.confidence = std::stof(val);
            }
            catch (...)
            {
                current.confidence = 0.5f;
            }
        }
        else if (inStep && (line.substr(0, 5) == "RISK:" || line.substr(0, 5) == "Risk:"))
        {
            current.risk = line.substr(5);
            size_t ts = current.risk.find_first_not_of(" \t");
            if (ts != std::string::npos)
                current.risk = current.risk.substr(ts);
        }
        else if (inStep && (line.substr(0, 5) == "TYPE:" || line.substr(0, 5) == "Type:"))
        {
            std::string val = line.substr(5);
            size_t ts = val.find_first_not_of(" \t");
            if (ts != std::string::npos)
                val = val.substr(ts);
            if (val.find("code_edit") != std::string::npos)
                current.type = PlanStepType::CodeEdit;
            else if (val.find("file_create") != std::string::npos)
                current.type = PlanStepType::FileCreate;
            else if (val.find("file_delete") != std::string::npos)
                current.type = PlanStepType::FileDelete;
            else if (val.find("shell") != std::string::npos)
                current.type = PlanStepType::ShellCommand;
            else if (val.find("analysis") != std::string::npos)
                current.type = PlanStepType::Analysis;
            else if (val.find("verif") != std::string::npos)
                current.type = PlanStepType::Verification;
        }
        else if (inStep && (line.substr(0, 5) == "FILE:" || line.substr(0, 5) == "File:"))
        {
            current.targetFile = line.substr(5);
            size_t ts = current.targetFile.find_first_not_of(" \t");
            if (ts != std::string::npos)
                current.targetFile = current.targetFile.substr(ts);
            if (current.targetFile == "none" || current.targetFile == "N/A")
                current.targetFile.clear();
        }
        else if (line.find("---") == 0)
        {
            if (inStep && !current.title.empty())
            {
                current.id = ++stepId;
                steps.push_back(current);
                current = PlanStep();
                inStep = false;
            }
        }
    }

    // Push last step if not terminated by ---
    if (inStep && !current.title.empty())
    {
        current.id = ++stepId;
        steps.push_back(current);
    }

    LOG_INFO("Parsed " + std::to_string(steps.size()) + " plan steps from agent output");
    return steps;
}

// ============================================================================
// PLAN STEP TYPE STRING
// ============================================================================

std::string Win32IDE::planStepTypeString(PlanStepType type) const
{
    switch (type)
    {
        case PlanStepType::CodeEdit:
            return "Code Edit";
        case PlanStepType::FileCreate:
            return "File Create";
        case PlanStepType::FileDelete:
            return "File Delete";
        case PlanStepType::ShellCommand:
            return "Shell Command";
        case PlanStepType::Analysis:
            return "Analysis";
        case PlanStepType::Verification:
            return "Verification";
        default:
            return "General";
    }
}

// ============================================================================
// POPULATE PLAN LIST VIEW — fills ListView with plan step rows
// ============================================================================

void Win32IDE::populatePlanListView()
{
    if (!m_hwndPlanList)
        return;

    ListView_DeleteAllItems(m_hwndPlanList);

    for (int i = 0; i < (int)m_currentPlan.steps.size(); i++)
    {
        const auto& step = m_currentPlan.steps[i];

        // Column 0: step number
        char numBuf[16];
        snprintf(numBuf, sizeof(numBuf), "%d", step.id);

        LVITEMA item = {};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = numBuf;
        ListView_InsertItem(m_hwndPlanList, &item);

        // Column 1: status icon
        const char* statusText = "Pending";
        switch (step.status)
        {
            case PlanStepStatus::Pending:
                statusText = "Pending";
                break;
            case PlanStepStatus::Running:
                statusText = "Running";
                break;
            case PlanStepStatus::Completed:
                statusText = "Done";
                break;
            case PlanStepStatus::Failed:
                statusText = "FAILED";
                break;
            case PlanStepStatus::Skipped:
                statusText = "Skipped";
                break;
        }
        ListView_SetItemText(m_hwndPlanList, i, 1, const_cast<char*>(statusText));

        // Column 2: title
        ListView_SetItemText(m_hwndPlanList, i, 2, const_cast<char*>(step.title.c_str()));

        // Column 3: type
        std::string typeStr = planStepTypeString(step.type);
        ListView_SetItemText(m_hwndPlanList, i, 3, const_cast<char*>(typeStr.c_str()));

        // Column 4: estimated time
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%dm", step.estimatedMinutes);
        ListView_SetItemText(m_hwndPlanList, i, 4, timeBuf);

        // Column 5: confidence
        char confBuf[16];
        snprintf(confBuf, sizeof(confBuf), "%d%%", (int)(step.confidence * 100));
        ListView_SetItemText(m_hwndPlanList, i, 5, confBuf);

        // Column 6: risk
        ListView_SetItemText(m_hwndPlanList, i, 6, const_cast<char*>(step.risk.c_str()));

        {
            const char* gl = full_agentic_ide::AgenticPlanningOrchestrator::mutationGateLabel(step.mutationGate);
            ListView_SetItemText(m_hwndPlanList, i, 7, const_cast<char*>(gl));
        }
    }
}

// ============================================================================
// ON PLAN LIST SELECTION CHANGED — update detail panel
// ============================================================================

void Win32IDE::onPlanListSelChanged()
{
    if (!m_hwndPlanList || !m_hwndPlanDetail)
        return;

    int sel = ListView_GetNextItem(m_hwndPlanList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_currentPlan.steps.size())
    {
        SetWindowTextA(m_hwndPlanDetail, "");
        return;
    }

    const auto& step = m_currentPlan.steps[sel];

    std::ostringstream oss;
    oss << "Step " << step.id << ": " << step.title << "\r\n";
    oss << "Type: " << planStepTypeString(step.type) << "\r\n";
    if (!step.targetFile.empty())
    {
        oss << "File: " << step.targetFile << "\r\n";
    }
    oss << "Risk: " << step.risk << " (tier " << static_cast<int>(step.riskTier) << ")"
        << "  |  Gate: " << full_agentic_ide::AgenticPlanningOrchestrator::mutationGateLabel(step.mutationGate)
        << "  |  Confidence: " << (int)(step.confidence * 100) << "%"
        << "  |  Est. time: ~" << step.estimatedMinutes << " min\r\n";
    if (!step.gateDetail.empty())
        oss << "Gate detail: " << step.gateDetail << "\r\n";
    oss << "\r\n" << step.description;

    if (!step.output.empty())
    {
        oss << "\r\n\r\n--- Output ---\r\n" << step.output;
    }

    SetWindowTextA(m_hwndPlanDetail, oss.str().c_str());
}

// ============================================================================
// ON PLAN DIALOG COMMAND — button click dispatcher
// ============================================================================

void Win32IDE::onPlanDialogCommand(int controlId)
{
    switch (controlId)
    {
        case IDC_PLAN_BTN_APPROVE:
        {
            m_currentPlan.executionPolicy = PlanExecutionPolicy::FullApproval;
            full_agentic_ide::AgenticPlanningOrchestrator::approveAllPendingMutations(m_currentPlan);
            syncActiveAgenticPlanApprovalsFromUi();
            m_currentPlan.status = PlanStatus::Approved;
            appendToOutput("[Plan] Approved (full). Starting execution...", "General", OutputSeverity::Info);
            LOG_INFO("Plan approved by user: " + m_currentPlan.goal);

            // Show progress controls, hide approval buttons
            ShowWindow(m_hwndPlanBtnApprove, SW_HIDE);
            if (m_hwndPlanBtnApproveSafe)
                ShowWindow(m_hwndPlanBtnApproveSafe, SW_HIDE);
            ShowWindow(m_hwndPlanBtnEdit, SW_HIDE);
            ShowWindow(m_hwndPlanBtnReject, SW_HIDE);
            ShowWindow(m_hwndPlanBtnPause, SW_SHOW);
            ShowWindow(m_hwndPlanBtnCancel, SW_SHOW);
            ShowWindow(m_hwndPlanProgress, SW_SHOW);
            ShowWindow(m_hwndPlanProgressLabel, SW_SHOW);

            // Update dialog title
            SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Executing...");

            // Re-enable main window (dialog stays open for progress)
            EnableWindow(m_hwndMain, TRUE);

            populatePlanListView();

            executePlan();
            break;
        }

        case IDC_PLAN_BTN_APPROVE_SAFE:
        {
            m_currentPlan.executionPolicy = PlanExecutionPolicy::SafeStepsOnly;
            full_agentic_ide::AgenticPlanningOrchestrator::approveLowRiskOnly(m_currentPlan);
            syncActiveAgenticPlanApprovalsFromUi();
            m_currentPlan.status = PlanStatus::Approved;
            appendToOutput("[Plan] Approved low-risk steps only; medium/high excluded.", "General",
                           OutputSeverity::Info);
            LOG_INFO("Plan low-risk approval: " + m_currentPlan.goal);

            ShowWindow(m_hwndPlanBtnApprove, SW_HIDE);
            if (m_hwndPlanBtnApproveSafe)
                ShowWindow(m_hwndPlanBtnApproveSafe, SW_HIDE);
            ShowWindow(m_hwndPlanBtnEdit, SW_HIDE);
            ShowWindow(m_hwndPlanBtnReject, SW_HIDE);
            ShowWindow(m_hwndPlanBtnPause, SW_SHOW);
            ShowWindow(m_hwndPlanBtnCancel, SW_SHOW);
            ShowWindow(m_hwndPlanProgress, SW_SHOW);
            ShowWindow(m_hwndPlanProgressLabel, SW_SHOW);

            SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Executing (low-risk)...");
            EnableWindow(m_hwndMain, TRUE);
            populatePlanListView();

            executePlan();
            break;
        }

        case IDC_PLAN_BTN_EDIT:
        {
            editSelectedPlanStep();
            break;
        }

        case IDC_PLAN_BTN_REJECT:
        {
            rejectPendingStepsInActiveAgenticPlan();
            m_currentPlan.status = PlanStatus::Rejected;
            appendToOutput("[Plan] Rejected by user.", "General", OutputSeverity::Warning);
            appendToOutput("[Plan] Plan rejected by user.\n", "Agent", OutputSeverity::Info);
            LOG_INFO("Plan rejected by user: " + m_currentPlan.goal);
            closePlanDialog();
            break;
        }

        case IDC_PLAN_BTN_PAUSE:
        {
            if (m_planExecutionPaused.load())
            {
                resumePlan();
                SetWindowTextA(m_hwndPlanBtnPause, "Pause");
                SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Executing...");
            }
            else
            {
                pausePlan();
                SetWindowTextA(m_hwndPlanBtnPause, "Resume");
                SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Paused");
            }
            break;
        }

        case IDC_PLAN_BTN_CANCEL:
        {
            cancelPlan();
            SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Cancelled");
            // Switch buttons back for review
            ShowWindow(m_hwndPlanBtnPause, SW_HIDE);
            ShowWindow(m_hwndPlanBtnCancel, SW_HIDE);
            // Show a close button via the reject button repurposed
            SetWindowTextA(m_hwndPlanBtnReject, "Close");
            ShowWindow(m_hwndPlanBtnReject, SW_SHOW);
            break;
        }
    }
}

// ============================================================================
// UPDATE PLAN STEP IN DIALOG — live progress during execution
// ============================================================================

void Win32IDE::updatePlanStepInDialog(int stepIndex, PlanStepStatus status)
{
    if (!m_hwndPlanList || stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size())
        return;

    // Update status text in ListView
    const char* statusText = "Pending";
    switch (status)
    {
        case PlanStepStatus::Pending:
            statusText = "Pending";
            break;
        case PlanStepStatus::Running:
            statusText = ">> Run";
            break;
        case PlanStepStatus::Completed:
            statusText = "Done";
            break;
        case PlanStepStatus::Failed:
            statusText = "FAILED";
            break;
        case PlanStepStatus::Skipped:
            statusText = "Skipped";
            break;
    }
    ListView_SetItemText(m_hwndPlanList, stepIndex, 1, const_cast<char*>(statusText));

    // Scroll to current step and select it
    if (status == PlanStepStatus::Running)
    {
        ListView_SetItemState(m_hwndPlanList, stepIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(m_hwndPlanList, stepIndex, FALSE);
    }

    // Update progress bar
    int total = (int)m_currentPlan.steps.size();
    int completed = 0;
    for (const auto& s : m_currentPlan.steps)
    {
        if (s.status == PlanStepStatus::Completed || s.status == PlanStepStatus::Skipped)
            completed++;
    }
    if (status == PlanStepStatus::Running)
        completed++;  // Count current as partial

    int progressVal = (total > 0) ? (completed * 1000) / total : 0;
    if (m_hwndPlanProgress)
        SendMessage(m_hwndPlanProgress, PBM_SETPOS, progressVal, 0);

    // Update progress label
    if (m_hwndPlanProgressLabel)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Step %d/%d", stepIndex + 1, total);
        SetWindowTextA(m_hwndPlanProgressLabel, buf);
    }
}

// ============================================================================
// CLOSE PLAN DIALOG — cleanup and re-enable main window
// ============================================================================

void Win32IDE::closePlanDialog()
{
    if (m_hwndPlanDialog && IsWindow(m_hwndPlanDialog))
    {
        EnableWindow(m_hwndMain, TRUE);
        DestroyWindow(m_hwndPlanDialog);
    }
    m_hwndPlanDialog = nullptr;
    m_hwndPlanList = nullptr;
    m_hwndPlanDetail = nullptr;
    m_hwndPlanGoalLabel = nullptr;
    m_hwndPlanSummaryLabel = nullptr;
    m_hwndPlanBtnApprove = nullptr;
    m_hwndPlanBtnApproveSafe = nullptr;
    m_hwndPlanBtnEdit = nullptr;
    m_hwndPlanBtnReject = nullptr;
    m_hwndPlanBtnPause = nullptr;
    m_hwndPlanBtnCancel = nullptr;
    m_hwndPlanProgress = nullptr;
    m_hwndPlanProgressLabel = nullptr;

    if (m_planDialogBrush)
    {
        DeleteObject(m_planDialogBrush);
        m_planDialogBrush = nullptr;
    }

    SetForegroundWindow(m_hwndMain);
}

// ============================================================================
// EDIT SELECTED PLAN STEP — inline edit of description via input dialog
// ============================================================================

void Win32IDE::editSelectedPlanStep()
{
    if (!m_hwndPlanList)
        return;

    int sel = ListView_GetNextItem(m_hwndPlanList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_currentPlan.steps.size())
    {
        MessageBoxA(m_hwndPlanDialog, "Select a step to edit.", "Edit Step", MB_OK | MB_ICONINFORMATION);
        return;
    }

    PlanStep& step = m_currentPlan.steps[sel];

    // Create a simple edit dialog
    HWND hEditDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "STATIC",
                                    ("Edit Step " + std::to_string(step.id) + ": " + step.title).c_str(),
                                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 520,
                                    320, m_hwndPlanDialog, nullptr, m_hInstance, nullptr);

    if (!hEditDlg)
        return;

    // Dark background
    SetClassLongPtrA(hEditDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Title label
    HWND hTitleLabel = CreateWindowExA(0, "STATIC", "Title:", WS_CHILD | WS_VISIBLE, 12, 12, 488, 16, hEditDlg, nullptr,
                                       m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hTitleLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hTitleEdit =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", step.title.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, 30,
                        488, 24, hEditDlg, (HMENU)201, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hTitleEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Description label
    HWND hDescLabel = CreateWindowExA(0, "STATIC", "Description:", WS_CHILD | WS_VISIBLE, 12, 62, 488, 16, hEditDlg,
                                      nullptr, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hDescLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hDescEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", step.description.c_str(),
                                     WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 12, 80, 488,
                                     120, hEditDlg, (HMENU)202, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hDescEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Target file
    HWND hFileLabel = CreateWindowExA(0, "STATIC", "Target file:", WS_CHILD | WS_VISIBLE, 12, 208, 488, 16, hEditDlg,
                                      nullptr, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hFileLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hFileEdit =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", step.targetFile.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12,
                        226, 488, 24, hEditDlg, (HMENU)203, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hFileEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Save / Cancel buttons
    HWND hSaveBtn = CreateWindowExA(0, "BUTTON", "Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 310, 258, 90, 28,
                                    hEditDlg, (HMENU)IDOK, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hSaveBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 410, 258, 90, 28,
                                      hEditDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    if (m_hFontUI)
        SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    SetFocus(hTitleEdit);

    // Modal loop for the edit dialog
    EnableWindow(m_hwndPlanDialog, FALSE);
    bool done = false;
    bool saved = false;

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_COMMAND && msg.hwnd == hEditDlg)
        {
            int wmId = LOWORD(msg.wParam);
            if (wmId == IDOK)
            {
                saved = true;
                done = true;
                continue;
            }
            if (wmId == IDCANCEL)
            {
                done = true;
                continue;
            }
        }
        if (msg.message == WM_CLOSE && msg.hwnd == hEditDlg)
        {
            done = true;
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        {
            done = true;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (saved)
    {
        // Read edited values back into the step
        char buf[4096];
        GetWindowTextA(hTitleEdit, buf, sizeof(buf));
        step.title = buf;

        GetWindowTextA(hDescEdit, buf, sizeof(buf));
        step.description = buf;

        GetWindowTextA(hFileEdit, buf, sizeof(buf));
        step.targetFile = buf;

        // Refresh the ListView row
        ListView_SetItemText(m_hwndPlanList, sel, 2, const_cast<char*>(step.title.c_str()));

        // Refresh detail
        onPlanListSelChanged();

        appendToOutput("[Plan] Step " + std::to_string(step.id) + " edited: " + step.title, "General",
                       OutputSeverity::Info);
    }

    EnableWindow(m_hwndPlanDialog, TRUE);
    DestroyWindow(hEditDlg);
    SetForegroundWindow(m_hwndPlanDialog);
}

// ============================================================================
// E05 / E07 — Agentic orchestrator integration (tools + SQLite audit sink)
// ============================================================================

void Win32IDE::wireAgenticOrchestratorIntegration()
{
    Agentic::setAgenticAuditSink([this](const std::string& kind, const std::string& jsonLine)
                                 { recordAgenticApprovalAudit(kind, jsonLine); });

    Agentic::OrchestratorIntegration::instance().initialize();
    Agentic::OrchestratorIntegration::instance().setToolExecutor(
        [this](const std::string& action, const std::string& args, std::string& out) -> bool
        {
            (void)args;
            try
            {
                nlohmann::json j = nlohmann::json::parse(action);
                if (j.value("agenticTool", "") != "win32_bridge_plan_step")
                {
                    out = "Unknown agentic tool payload";
                    return false;
                }
                PlanStep ps;
                ps.type = static_cast<PlanStepType>(j.value("planStepType", 0));
                ps.title = j.value("title", "");
                ps.description = j.value("description", "");
                ps.targetFile = j.value("targetFile", "");
                out = executeAgentPlanStepViaBridge(ps);
                return out.find("[Error]") == std::string::npos;
            }
            catch (...)
            {
                out = "Invalid tool JSON for plan step";
                return false;
            }
        });
}
