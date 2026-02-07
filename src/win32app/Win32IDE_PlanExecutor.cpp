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

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>

// ============================================================================
// PLAN STEP EXECUTION — runs a single step via the agent
// ============================================================================

static std::string executeSingleStep(AgenticBridge* bridge, const PlanStep& step) {
    if (!bridge || !bridge->IsInitialized()) return "[Error] Agent not initialized";

    std::string prompt;
    switch (step.type) {
        case PlanStepType::CodeEdit:
            prompt = "Execute exactly: " + step.description +
                     (step.targetFile.empty() ? "" : "\nFile: " + step.targetFile);
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

void Win32IDE::generateAgentPlan(const std::string& goal) {
    if (!m_agenticBridge || !m_agenticBridge->IsInitialized()) {
        appendToOutput("[Plan] Agent not initialized. Configure a model first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[Plan] Generating plan for: " + goal, "General", OutputSeverity::Info);

    // Reset current plan
    m_currentPlan.goal = goal;
    m_currentPlan.steps.clear();
    m_currentPlan.status = PlanStatus::Generating;
    m_currentPlan.currentStepIndex = -1;
    m_currentPlan.overallConfidence = 0.0f;

    // Background thread — ask agent for structured plan
    std::string goalCopy = goal;
    std::thread([this, goalCopy]() {
        std::string planPrompt =
            "Create a detailed step-by-step plan to accomplish the following goal.\n"
            "For each step, provide:\n"
            "  - A short title (one line)\n"
            "  - A description of what to do\n"
            "  - Estimated time (in minutes)\n"
            "  - Confidence level (0.0 to 1.0)\n"
            "  - Risk level (low/medium/high)\n"
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
            "Goal: " + goalCopy;

        AgentResponse response = m_agenticBridge->ExecuteAgentCommand(planPrompt);

        // Parse the plan from agent output
        std::vector<PlanStep> steps = parsePlanSteps(response.content);

        // Post to UI thread
        PlanStep* heapSteps = nullptr;
        int stepCount = (int)steps.size();
        if (stepCount > 0) {
            heapSteps = new PlanStep[stepCount];
            for (int i = 0; i < stepCount; i++) {
                heapSteps[i] = steps[i];
            }
        }
        PostMessageA(m_hwndMain, WM_PLAN_READY, (WPARAM)stepCount, (LPARAM)heapSteps);
    }).detach();
}

// ============================================================================
// PLAN READY — delivered to UI thread
// ============================================================================

void Win32IDE::onPlanReady(int stepCount, PlanStep* steps) {
    if (stepCount <= 0 || !steps) {
        m_currentPlan.status = PlanStatus::Failed;
        appendToOutput("[Plan] Failed to generate plan — no steps parsed.",
                       "General", OutputSeverity::Error);
        return;
    }

    m_currentPlan.steps.clear();
    float totalConf = 0.0f;
    int totalTime = 0;

    for (int i = 0; i < stepCount; i++) {
        steps[i].id = i + 1;
        steps[i].status = PlanStepStatus::Pending;
        m_currentPlan.steps.push_back(steps[i]);
        totalConf += steps[i].confidence;
        totalTime += steps[i].estimatedMinutes;
    }
    delete[] steps;

    m_currentPlan.overallConfidence = totalConf / stepCount;
    m_currentPlan.status = PlanStatus::AwaitingApproval;

    // Record plan generation event
    recordEvent(AgentEventType::PlanGenerated, "",
                m_currentPlan.goal,
                std::to_string(stepCount) + " steps, ~" + std::to_string(totalTime) + " min",
                0, true, "",
                "{\"steps\":" + std::to_string(stepCount) +
                ",\"confidence\":" + std::to_string((int)(m_currentPlan.overallConfidence * 100)) + "}");

    // Show plan approval dialog
    showPlanApprovalDialog();
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

void Win32IDE::showPlanApprovalDialog() {
    if (m_currentPlan.steps.empty()) return;

    // Close any existing plan dialog
    closePlanDialog();

    // Dialog dimensions
    const int dlgW = 720, dlgH = 560;

    // Center on main window
    RECT mainRC;
    GetWindowRect(m_hwndMain, &mainRC);
    int cx = (mainRC.left + mainRC.right) / 2 - dlgW / 2;
    int cy = (mainRC.top + mainRC.bottom) / 2 - dlgH / 2;

    // Create dark background brush
    if (m_planDialogBrush) DeleteObject(m_planDialogBrush);
    m_planDialogBrush = CreateSolidBrush(RGB(30, 30, 30));

    // Register a window class for the plan dialog (once)
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXA wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            switch (uMsg) {
            case WM_COMMAND:
                if (ide) ide->onPlanDialogCommand(LOWORD(wParam));
                return 0;
            case WM_NOTIFY: {
                NMHDR* nm = reinterpret_cast<NMHDR*>(lParam);
                if (nm && nm->code == LVN_ITEMCHANGED && ide) {
                    ide->onPlanListSelChanged();
                }
                return 0;
            }
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX: {
                HDC hdcCtrl = (HDC)wParam;
                SetTextColor(hdcCtrl, RGB(212, 212, 212));
                SetBkColor(hdcCtrl, RGB(37, 37, 38));
                if (ide && ide->m_planDialogBrush)
                    return (LRESULT)ide->m_planDialogBrush;
                return (LRESULT)GetStockObject(BLACK_BRUSH);
            }
            case WM_ERASEBKGND: {
                HDC hdc = (HDC)wParam;
                RECT rc;
                GetClientRect(hwnd, &rc);
                HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
                FillRect(hdc, &rc, br);
                DeleteObject(br);
                return 1;
            }
            case WM_CLOSE:
                if (ide) ide->closePlanDialog();
                return 0;
            }
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
        };
        wc.hInstance     = m_hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = "RawrXD_PlanDialog";
        RegisterClassExA(&wc);
        classRegistered = true;
    }

    // Create the dialog window
    m_hwndPlanDialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "RawrXD_PlanDialog", "Agent Plan — Review & Approve",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, dlgW, dlgH,
        m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!m_hwndPlanDialog) {
        LOG_ERROR("Failed to create Plan Approval Dialog");
        return;
    }

    SetWindowLongPtrA(m_hwndPlanDialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Margin constants
    const int M = 12;       // Outer margin
    const int W = dlgW - 2 * M - 16;  // Usable width (account for frame)

    int y = M;

    // ── Goal label ──
    m_hwndPlanGoalLabel = CreateWindowExA(0, "STATIC",
        ("Plan: " + m_currentPlan.goal).c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        M, y, W, 20, m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_GOAL_LABEL, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanGoalLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 24;

    // ── Summary label ──
    int totalMinutes = 0;
    for (const auto& s : m_currentPlan.steps) totalMinutes += s.estimatedMinutes;
    char summaryBuf[256];
    snprintf(summaryBuf, sizeof(summaryBuf),
             "%d steps  ·  ~%d min  ·  %d%% confidence",
             (int)m_currentPlan.steps.size(), totalMinutes,
             (int)(m_currentPlan.overallConfidence * 100));

    m_hwndPlanSummaryLabel = CreateWindowExA(0, "STATIC", summaryBuf,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        M, y, W, 18, m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_SUMMARY_LABEL, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanSummaryLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 26;

    // ── ListView for plan steps ──
    int listH = 200;
    m_hwndPlanList = CreateWindowExA(WS_EX_CLIENTEDGE,
        WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        M, y, W, listH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_LIST, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Extended styles: full-row select, gridlines
    ListView_SetExtendedListViewStyle(m_hwndPlanList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Dark background for ListView
    ListView_SetBkColor(m_hwndPlanList, RGB(37, 37, 38));
    ListView_SetTextBkColor(m_hwndPlanList, RGB(37, 37, 38));
    ListView_SetTextColor(m_hwndPlanList, RGB(212, 212, 212));

    // Add columns: #, Status, Title, Type, Time, Confidence, Risk
    auto addCol = [&](int idx, const char* text, int width) {
        LVCOLUMNA col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = LVCFMT_LEFT;
        col.cx = width;
        col.pszText = const_cast<char*>(text);
        ListView_InsertColumn(m_hwndPlanList, idx, &col);
    };
    addCol(0, "#",          32);
    addCol(1, "Status",     60);
    addCol(2, "Step",       240);
    addCol(3, "Type",       80);
    addCol(4, "Time",       50);
    addCol(5, "Conf",       50);
    addCol(6, "Risk",       60);

    y += listH + 6;

    // ── Detail panel (description + file + risk) ──
    int detailH = 100;
    m_hwndPlanDetail = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        M, y, W, detailH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_DETAIL, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanDetail, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += detailH + 6;

    // ── Progress bar ──
    m_hwndPlanProgress = CreateWindowExA(0, PROGRESS_CLASSA, "",
        WS_CHILD | PBS_SMOOTH,
        M, y, W - 200, 18,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_PROGRESS, m_hInstance, nullptr);
    SendMessage(m_hwndPlanProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
    SendMessage(m_hwndPlanProgress, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 122, 204));
    SendMessage(m_hwndPlanProgress, PBM_SETBKCOLOR, 0, (LPARAM)RGB(50, 50, 50));

    m_hwndPlanProgressLabel = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | SS_LEFT,
        M + W - 190, y, 190, 18,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_PROGRESS_LABEL, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanProgressLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 26;

    // ── Action buttons ──
    int btnW = 110, btnH = 30, btnGap = 8;
    int btnY = y;

    m_hwndPlanBtnApprove = CreateWindowExA(0, "BUTTON", "\xe2\x9c\x93 Approve",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        M, btnY, btnW, btnH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_APPROVE, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanBtnApprove, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hwndPlanBtnEdit = CreateWindowExA(0, "BUTTON", "\xe2\x9c\x8e Edit Step",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        M + btnW + btnGap, btnY, btnW, btnH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_EDIT, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanBtnEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hwndPlanBtnReject = CreateWindowExA(0, "BUTTON", "\xe2\x9c\x97 Reject",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        M + 2 * (btnW + btnGap), btnY, btnW, btnH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_REJECT, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanBtnReject, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hwndPlanBtnPause = CreateWindowExA(0, "BUTTON", "Pause",
        WS_CHILD | BS_PUSHBUTTON,
        M + 3 * (btnW + btnGap), btnY, 80, btnH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_PAUSE, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanBtnPause, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hwndPlanBtnCancel = CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | BS_PUSHBUTTON,
        M + 3 * (btnW + btnGap) + 80 + btnGap, btnY, 80, btnH,
        m_hwndPlanDialog, (HMENU)(LONG_PTR)IDC_PLAN_BTN_CANCEL, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(m_hwndPlanBtnCancel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

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
// EXECUTE PLAN — step-by-step with progress
// ============================================================================

void Win32IDE::executePlan() {
    if (m_currentPlan.status != PlanStatus::Approved) return;
    if (m_currentPlan.steps.empty()) return;

    m_currentPlan.status = PlanStatus::Executing;
    m_planExecutionCancelled.store(false);
    m_planExecutionPaused.store(false);

    // Show progress
    showModelProgressBar("Executing plan: " + m_currentPlan.goal);

    std::thread([this]() {
        int totalSteps = (int)m_currentPlan.steps.size();

        for (int i = 0; i < totalSteps; i++) {
            // Check for cancellation
            if (m_planExecutionCancelled.load()) {
                m_currentPlan.steps[i].status = PlanStepStatus::Skipped;
                for (int j = i + 1; j < totalSteps; j++) {
                    m_currentPlan.steps[j].status = PlanStepStatus::Skipped;
                }
                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)2); // 2 = cancelled
                break;
            }

            // Check for pause
            while (m_planExecutionPaused.load() && !m_planExecutionCancelled.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            m_currentPlan.currentStepIndex = i;
            m_currentPlan.steps[i].status = PlanStepStatus::Running;

            // Update plan dialog ListView live
            PostMessageA(m_hwndMain, WM_APP + 503, i, (LPARAM)PlanStepStatus::Running);

            // Update progress on UI thread
            float percent = ((float)i / totalSteps) * 100.0f;
            updateModelProgress(percent,
                "Step " + std::to_string(i + 1) + "/" + std::to_string(totalSteps) +
                ": " + m_currentPlan.steps[i].title);

            // Create file backup before code edits (for rollback)
            std::string backupContent;
            if (m_currentPlan.steps[i].type == PlanStepType::CodeEdit &&
                !m_currentPlan.steps[i].targetFile.empty()) {
                std::ifstream f(m_currentPlan.steps[i].targetFile);
                if (f) {
                    std::ostringstream buf;
                    buf << f.rdbuf();
                    backupContent = buf.str();
                }
            }

            // Execute the step
            std::string result = executeSingleStep(m_agenticBridge.get(), m_currentPlan.steps[i]);

            if (result.find("[Error]") != std::string::npos ||
                result.find("error") != std::string::npos) {
                m_currentPlan.steps[i].status = PlanStepStatus::Failed;
                m_currentPlan.steps[i].output = result;

                // Rollback file if we backed it up
                if (!backupContent.empty() && !m_currentPlan.steps[i].targetFile.empty()) {
                    std::ofstream restore(m_currentPlan.steps[i].targetFile);
                    if (restore) {
                        restore << backupContent;
                        restore.close();
                    }
                }

                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)0); // 0 = failed
                // Don't continue on failure — ask user
                break;
            } else {
                m_currentPlan.steps[i].status = PlanStepStatus::Completed;
                m_currentPlan.steps[i].output = result;
                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)1); // 1 = success
            }
        }

        // Check if all steps completed
        bool allDone = true;
        for (const auto& step : m_currentPlan.steps) {
            if (step.status != PlanStepStatus::Completed) {
                allDone = false;
                break;
            }
        }

        PostMessageA(m_hwndMain, WM_PLAN_COMPLETE, allDone ? 1 : 0, 0);
    }).detach();
}

// ============================================================================
// PLAN STEP DONE — UI thread handler
// ============================================================================

void Win32IDE::onPlanStepDone(int stepIndex, int result) {
    if (stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size()) return;

    const auto& step = m_currentPlan.steps[stepIndex];

    // Update plan dialog ListView with current step status
    updatePlanStepInDialog(stepIndex, step.status);

    if (result == 1) {
        appendToOutput("[Plan] ✓ Step " + std::to_string(stepIndex + 1) + ": " + step.title,
                       "General", OutputSeverity::Info);
    } else if (result == 0) {
        appendToOutput("[Plan] ✗ Step " + std::to_string(stepIndex + 1) + " failed: " + step.title,
                       "General", OutputSeverity::Error);
        appendToOutput("[Plan] Output: " + step.output, "General", OutputSeverity::Error);

        // Ask user whether to continue or abort
        int choice = MessageBoxA(m_hwndMain,
            ("Step " + std::to_string(stepIndex + 1) + " failed:\n" + step.title +
             "\n\nContinue with remaining steps?").c_str(),
            "Plan Step Failed", MB_YESNO | MB_ICONWARNING);

        if (choice == IDYES) {
            // Mark failed step and continue
            std::thread([this, stepIndex]() {
                // Resume from next step
                int totalSteps = (int)m_currentPlan.steps.size();
                for (int i = stepIndex + 1; i < totalSteps; i++) {
                    if (m_planExecutionCancelled.load()) break;
                    m_currentPlan.currentStepIndex = i;
                    m_currentPlan.steps[i].status = PlanStepStatus::Running;

                    float percent = ((float)i / totalSteps) * 100.0f;
                    updateModelProgress(percent,
                        "Step " + std::to_string(i + 1) + "/" + std::to_string(totalSteps) +
                        ": " + m_currentPlan.steps[i].title);

                    std::string result = executeSingleStep(m_agenticBridge.get(), m_currentPlan.steps[i]);
                    m_currentPlan.steps[i].status = PlanStepStatus::Completed;
                    m_currentPlan.steps[i].output = result;
                    PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)1);
                }
                PostMessageA(m_hwndMain, WM_PLAN_COMPLETE, 1, 0);
            }).detach();
        } else {
            cancelPlan();
        }
    } else {
        appendToOutput("[Plan] Cancelled at step " + std::to_string(stepIndex + 1),
                       "General", OutputSeverity::Warning);
    }
}

// ============================================================================
// PLAN COMPLETE — UI thread handler
// ============================================================================

void Win32IDE::onPlanComplete(bool success) {
    hideModelProgressBar();

    if (success) {
        m_currentPlan.status = PlanStatus::Completed;
        appendToOutput("[Plan] ✓ All steps completed successfully!",
                       "General", OutputSeverity::Info);
    } else {
        m_currentPlan.status = PlanStatus::Failed;
        appendToOutput("[Plan] Plan completed with failures.",
                       "General", OutputSeverity::Warning);
    }

    // Update the plan dialog if it's still open
    if (m_hwndPlanDialog && IsWindow(m_hwndPlanDialog)) {
        SetWindowTextA(m_hwndPlanDialog,
            success ? "Agent Plan - Completed" : "Agent Plan - Completed with Failures");
        // Hide execution buttons, show close
        if (m_hwndPlanBtnPause)  ShowWindow(m_hwndPlanBtnPause, SW_HIDE);
        if (m_hwndPlanBtnCancel) ShowWindow(m_hwndPlanBtnCancel, SW_HIDE);
        if (m_hwndPlanBtnReject) {
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
    if (m_planHistory.size() > MAX_PLAN_HISTORY) {
        m_planHistory.erase(m_planHistory.begin());
    }
}

// ============================================================================
// CANCEL / PAUSE / RESUME
// ============================================================================

void Win32IDE::cancelPlan() {
    m_planExecutionCancelled.store(true);
    m_currentPlan.status = PlanStatus::Failed;
    hideModelProgressBar();
    appendToOutput("[Plan] Execution cancelled by user.", "General", OutputSeverity::Warning);
}

void Win32IDE::pausePlan() {
    m_planExecutionPaused.store(true);
    appendToOutput("[Plan] Execution paused.", "General", OutputSeverity::Info);
}

void Win32IDE::resumePlan() {
    m_planExecutionPaused.store(false);
    appendToOutput("[Plan] Execution resumed.", "General", OutputSeverity::Info);
}

// ============================================================================
// PLAN STATUS STRING
// ============================================================================

std::string Win32IDE::getPlanStatusString() const {
    std::ostringstream oss;
    oss << "Plan: " << m_currentPlan.goal << "\r\n";
    oss << "Status: ";
    switch (m_currentPlan.status) {
        case PlanStatus::None:              oss << "No plan"; break;
        case PlanStatus::Generating:        oss << "Generating..."; break;
        case PlanStatus::AwaitingApproval:  oss << "Awaiting approval"; break;
        case PlanStatus::Approved:          oss << "Approved"; break;
        case PlanStatus::Rejected:          oss << "Rejected"; break;
        case PlanStatus::Executing:         oss << "Executing..."; break;
        case PlanStatus::Completed:         oss << "Completed"; break;
        case PlanStatus::Failed:            oss << "Failed"; break;
    }
    oss << "\r\nSteps: " << m_currentPlan.steps.size()
        << " | Confidence: " << (int)(m_currentPlan.overallConfidence * 100) << "%\r\n";

    for (const auto& step : m_currentPlan.steps) {
        oss << "  [" << step.id << "] ";
        switch (step.status) {
            case PlanStepStatus::Pending:   oss << "⬜ "; break;
            case PlanStepStatus::Running:   oss << "🔄 "; break;
            case PlanStepStatus::Completed: oss << "✅ "; break;
            case PlanStepStatus::Failed:    oss << "❌ "; break;
            case PlanStepStatus::Skipped:   oss << "⏭️ "; break;
        }
        oss << step.title << "\r\n";
    }

    return oss.str();
}

// ============================================================================
// PARSE PLAN STEPS — from agent output
// ============================================================================

std::vector<PlanStep> Win32IDE::parsePlanSteps(const std::string& agentOutput) {
    std::vector<PlanStep> steps;
    PlanStep current;
    bool inStep = false;
    int stepId = 0;

    std::istringstream stream(agentOutput);
    std::string line;

    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line.substr(0, 5) == "STEP:" || line.substr(0, 5) == "Step:") {
            if (inStep && !current.title.empty()) {
                current.id = ++stepId;
                steps.push_back(current);
            }
            current = PlanStep();
            current.title = line.substr(5);
            // Trim title
            size_t ts = current.title.find_first_not_of(" \t");
            if (ts != std::string::npos) current.title = current.title.substr(ts);
            inStep = true;
        } else if (inStep && (line.substr(0, 5) == "DESC:" || line.substr(0, 5) == "Desc:")) {
            current.description = line.substr(5);
            size_t ts = current.description.find_first_not_of(" \t");
            if (ts != std::string::npos) current.description = current.description.substr(ts);
        } else if (inStep && (line.substr(0, 5) == "TIME:" || line.substr(0, 5) == "Time:")) {
            std::string val = line.substr(5);
            try { current.estimatedMinutes = std::stoi(val); } catch (...) { current.estimatedMinutes = 1; }
        } else if (inStep && (line.substr(0, 5) == "CONF:" || line.substr(0, 5) == "Conf:")) {
            std::string val = line.substr(5);
            try { current.confidence = std::stof(val); } catch (...) { current.confidence = 0.5f; }
        } else if (inStep && (line.substr(0, 5) == "RISK:" || line.substr(0, 5) == "Risk:")) {
            current.risk = line.substr(5);
            size_t ts = current.risk.find_first_not_of(" \t");
            if (ts != std::string::npos) current.risk = current.risk.substr(ts);
        } else if (inStep && (line.substr(0, 5) == "TYPE:" || line.substr(0, 5) == "Type:")) {
            std::string val = line.substr(5);
            size_t ts = val.find_first_not_of(" \t");
            if (ts != std::string::npos) val = val.substr(ts);
            if (val.find("code_edit") != std::string::npos) current.type = PlanStepType::CodeEdit;
            else if (val.find("file_create") != std::string::npos) current.type = PlanStepType::FileCreate;
            else if (val.find("file_delete") != std::string::npos) current.type = PlanStepType::FileDelete;
            else if (val.find("shell") != std::string::npos) current.type = PlanStepType::ShellCommand;
            else if (val.find("analysis") != std::string::npos) current.type = PlanStepType::Analysis;
            else if (val.find("verif") != std::string::npos) current.type = PlanStepType::Verification;
        } else if (inStep && (line.substr(0, 5) == "FILE:" || line.substr(0, 5) == "File:")) {
            current.targetFile = line.substr(5);
            size_t ts = current.targetFile.find_first_not_of(" \t");
            if (ts != std::string::npos) current.targetFile = current.targetFile.substr(ts);
            if (current.targetFile == "none" || current.targetFile == "N/A") current.targetFile.clear();
        } else if (line.find("---") == 0) {
            if (inStep && !current.title.empty()) {
                current.id = ++stepId;
                steps.push_back(current);
                current = PlanStep();
                inStep = false;
            }
        }
    }

    // Push last step if not terminated by ---
    if (inStep && !current.title.empty()) {
        current.id = ++stepId;
        steps.push_back(current);
    }

    LOG_INFO("Parsed " + std::to_string(steps.size()) + " plan steps from agent output");
    return steps;
}

// ============================================================================
// PLAN STEP TYPE STRING
// ============================================================================

std::string Win32IDE::planStepTypeString(PlanStepType type) const {
    switch (type) {
        case PlanStepType::CodeEdit:     return "Code Edit";
        case PlanStepType::FileCreate:   return "File Create";
        case PlanStepType::FileDelete:   return "File Delete";
        case PlanStepType::ShellCommand: return "Shell Command";
        case PlanStepType::Analysis:     return "Analysis";
        case PlanStepType::Verification: return "Verification";
        default:                         return "General";
    }
}

// ============================================================================
// POPULATE PLAN LIST VIEW — fills ListView with plan step rows
// ============================================================================

void Win32IDE::populatePlanListView() {
    if (!m_hwndPlanList) return;

    ListView_DeleteAllItems(m_hwndPlanList);

    for (int i = 0; i < (int)m_currentPlan.steps.size(); i++) {
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
        switch (step.status) {
            case PlanStepStatus::Pending:   statusText = "Pending";   break;
            case PlanStepStatus::Running:   statusText = "Running";   break;
            case PlanStepStatus::Completed: statusText = "Done";      break;
            case PlanStepStatus::Failed:    statusText = "FAILED";    break;
            case PlanStepStatus::Skipped:   statusText = "Skipped";   break;
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
    }
}

// ============================================================================
// ON PLAN LIST SELECTION CHANGED — update detail panel
// ============================================================================

void Win32IDE::onPlanListSelChanged() {
    if (!m_hwndPlanList || !m_hwndPlanDetail) return;

    int sel = ListView_GetNextItem(m_hwndPlanList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_currentPlan.steps.size()) {
        SetWindowTextA(m_hwndPlanDetail, "");
        return;
    }

    const auto& step = m_currentPlan.steps[sel];

    std::ostringstream oss;
    oss << "Step " << step.id << ": " << step.title << "\r\n";
    oss << "Type: " << planStepTypeString(step.type) << "\r\n";
    if (!step.targetFile.empty()) {
        oss << "File: " << step.targetFile << "\r\n";
    }
    oss << "Risk: " << step.risk
        << "  |  Confidence: " << (int)(step.confidence * 100) << "%"
        << "  |  Est. time: ~" << step.estimatedMinutes << " min\r\n";
    oss << "\r\n" << step.description;

    if (!step.output.empty()) {
        oss << "\r\n\r\n--- Output ---\r\n" << step.output;
    }

    SetWindowTextA(m_hwndPlanDetail, oss.str().c_str());
}

// ============================================================================
// ON PLAN DIALOG COMMAND — button click dispatcher
// ============================================================================

void Win32IDE::onPlanDialogCommand(int controlId) {
    switch (controlId) {
    case IDC_PLAN_BTN_APPROVE: {
        m_currentPlan.status = PlanStatus::Approved;
        appendToOutput("[Plan] Approved. Starting execution...",
                       "General", OutputSeverity::Info);
        LOG_INFO("Plan approved by user: " + m_currentPlan.goal);

        // Show progress controls, hide approval buttons
        ShowWindow(m_hwndPlanBtnApprove, SW_HIDE);
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

        executePlan();
        break;
    }

    case IDC_PLAN_BTN_EDIT: {
        editSelectedPlanStep();
        break;
    }

    case IDC_PLAN_BTN_REJECT: {
        m_currentPlan.status = PlanStatus::Rejected;
        appendToOutput("[Plan] Rejected by user.", "General", OutputSeverity::Warning);
        LOG_INFO("Plan rejected by user: " + m_currentPlan.goal);
        closePlanDialog();
        break;
    }

    case IDC_PLAN_BTN_PAUSE: {
        if (m_planExecutionPaused.load()) {
            resumePlan();
            SetWindowTextA(m_hwndPlanBtnPause, "Pause");
            SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Executing...");
        } else {
            pausePlan();
            SetWindowTextA(m_hwndPlanBtnPause, "Resume");
            SetWindowTextA(m_hwndPlanDialog, "Agent Plan — Paused");
        }
        break;
    }

    case IDC_PLAN_BTN_CANCEL: {
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

void Win32IDE::updatePlanStepInDialog(int stepIndex, PlanStepStatus status) {
    if (!m_hwndPlanList || stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size()) return;

    // Update status text in ListView
    const char* statusText = "Pending";
    switch (status) {
        case PlanStepStatus::Pending:   statusText = "Pending";   break;
        case PlanStepStatus::Running:   statusText = ">> Run";    break;
        case PlanStepStatus::Completed: statusText = "Done";      break;
        case PlanStepStatus::Failed:    statusText = "FAILED";    break;
        case PlanStepStatus::Skipped:   statusText = "Skipped";   break;
    }
    ListView_SetItemText(m_hwndPlanList, stepIndex, 1, const_cast<char*>(statusText));

    // Scroll to current step and select it
    if (status == PlanStepStatus::Running) {
        ListView_SetItemState(m_hwndPlanList, stepIndex,
                              LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(m_hwndPlanList, stepIndex, FALSE);
    }

    // Update progress bar
    int total = (int)m_currentPlan.steps.size();
    int completed = 0;
    for (const auto& s : m_currentPlan.steps) {
        if (s.status == PlanStepStatus::Completed || s.status == PlanStepStatus::Skipped)
            completed++;
    }
    if (status == PlanStepStatus::Running) completed++;  // Count current as partial

    int progressVal = (total > 0) ? (completed * 1000) / total : 0;
    if (m_hwndPlanProgress)
        SendMessage(m_hwndPlanProgress, PBM_SETPOS, progressVal, 0);

    // Update progress label
    if (m_hwndPlanProgressLabel) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Step %d/%d", stepIndex + 1, total);
        SetWindowTextA(m_hwndPlanProgressLabel, buf);
    }
}

// ============================================================================
// CLOSE PLAN DIALOG — cleanup and re-enable main window
// ============================================================================

void Win32IDE::closePlanDialog() {
    if (m_hwndPlanDialog && IsWindow(m_hwndPlanDialog)) {
        EnableWindow(m_hwndMain, TRUE);
        DestroyWindow(m_hwndPlanDialog);
    }
    m_hwndPlanDialog = nullptr;
    m_hwndPlanList = nullptr;
    m_hwndPlanDetail = nullptr;
    m_hwndPlanGoalLabel = nullptr;
    m_hwndPlanSummaryLabel = nullptr;
    m_hwndPlanBtnApprove = nullptr;
    m_hwndPlanBtnEdit = nullptr;
    m_hwndPlanBtnReject = nullptr;
    m_hwndPlanBtnPause = nullptr;
    m_hwndPlanBtnCancel = nullptr;
    m_hwndPlanProgress = nullptr;
    m_hwndPlanProgressLabel = nullptr;

    if (m_planDialogBrush) {
        DeleteObject(m_planDialogBrush);
        m_planDialogBrush = nullptr;
    }

    SetForegroundWindow(m_hwndMain);
}

// ============================================================================
// EDIT SELECTED PLAN STEP — inline edit of description via input dialog
// ============================================================================

void Win32IDE::editSelectedPlanStep() {
    if (!m_hwndPlanList) return;

    int sel = ListView_GetNextItem(m_hwndPlanList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)m_currentPlan.steps.size()) {
        MessageBoxA(m_hwndPlanDialog, "Select a step to edit.", "Edit Step", MB_OK | MB_ICONINFORMATION);
        return;
    }

    PlanStep& step = m_currentPlan.steps[sel];

    // Create a simple edit dialog
    HWND hEditDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "STATIC", ("Edit Step " + std::to_string(step.id) + ": " + step.title).c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 320,
        m_hwndPlanDialog, nullptr, m_hInstance, nullptr);

    if (!hEditDlg) return;

    // Dark background
    SetClassLongPtrA(hEditDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Title label
    HWND hTitleLabel = CreateWindowExA(0, "STATIC", "Title:",
        WS_CHILD | WS_VISIBLE, 12, 12, 488, 16, hEditDlg, nullptr, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hTitleLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hTitleEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", step.title.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        12, 30, 488, 24, hEditDlg, (HMENU)201, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hTitleEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Description label
    HWND hDescLabel = CreateWindowExA(0, "STATIC", "Description:",
        WS_CHILD | WS_VISIBLE, 12, 62, 488, 16, hEditDlg, nullptr, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hDescLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hDescEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", step.description.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        12, 80, 488, 120, hEditDlg, (HMENU)202, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hDescEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Target file
    HWND hFileLabel = CreateWindowExA(0, "STATIC", "Target file:",
        WS_CHILD | WS_VISIBLE, 12, 208, 488, 16, hEditDlg, nullptr, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hFileLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hFileEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", step.targetFile.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        12, 226, 488, 24, hEditDlg, (HMENU)203, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hFileEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Save / Cancel buttons
    HWND hSaveBtn = CreateWindowExA(0, "BUTTON", "Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        310, 258, 90, 28, hEditDlg, (HMENU)IDOK, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hSaveBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        410, 258, 90, 28, hEditDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    if (m_hFontUI) SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    SetFocus(hTitleEdit);

    // Modal loop for the edit dialog
    EnableWindow(m_hwndPlanDialog, FALSE);
    bool done = false;
    bool saved = false;

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND && msg.hwnd == hEditDlg) {
            int wmId = LOWORD(msg.wParam);
            if (wmId == IDOK) { saved = true; done = true; continue; }
            if (wmId == IDCANCEL) { done = true; continue; }
        }
        if (msg.message == WM_CLOSE && msg.hwnd == hEditDlg) {
            done = true;
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            done = true;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (saved) {
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

        appendToOutput("[Plan] Step " + std::to_string(step.id) + " edited: " + step.title,
                       "General", OutputSeverity::Info);
    }

    EnableWindow(m_hwndPlanDialog, TRUE);
    DestroyWindow(hEditDlg);
    SetForegroundWindow(m_hwndPlanDialog);
}
