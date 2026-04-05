// ============================================================================
// Win32IDE_StreamingUX.cpp — Streaming / Model UX
// ============================================================================
// Provides user-facing progress, cancellation, and status feedback for
// model loading, downloading, and inference operations.
//
// Components:
//   - Progress bar overlay (top of editor area)
//   - Cancel button
//   - Status text label
//   - Timer-driven UI updates from background threads
//   - Integration with model download callbacks
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "Win32IDE_SubAgent.h"
#include <richedit.h>

// ============================================================================
// SHOW PROGRESS BAR — creates the progress overlay UI
// ============================================================================
void Win32IDE::showModelProgressBar(const std::string& operation) {
    LOG_INFO("Model progress: starting — " + operation);
    
    m_modelOperationActive.store(true);
    m_modelOperationCancelled.store(false);
    m_modelProgressPercent.store(0.0f);
    
    {
        std::lock_guard<std::mutex> lock(m_modelProgressMutex);
        m_modelProgressStatus = operation;
    }
    
    if (!m_hwndMain) return;
    
    // Get the editor area rect to position the progress bar
    RECT mainRC;
    GetClientRect(m_hwndMain, &mainRC);
    
    int barHeight = 32;
    int barY = 0;
    
    // If editor exists, position just above it
    if (m_hwndEditor) {
        RECT editorRC;
        GetWindowRect(m_hwndEditor, &editorRC);
        POINT pt = {editorRC.left, editorRC.top};
        ScreenToClient(m_hwndMain, &pt);
        barY = pt.y;
    }
    
    // Create container panel
    if (!m_hwndModelProgressContainer) {
        m_hwndModelProgressContainer = CreateWindowExA(
            0, "STATIC", "",
            WS_CHILD | SS_OWNERDRAW,
            0, barY, mainRC.right, barHeight,
            m_hwndMain, nullptr, m_hInstance, nullptr);
        
        SetPropA(m_hwndModelProgressContainer, "IDE_PTR", (HANDLE)this);
    }
    
    // Position and resize container
    SetWindowPos(m_hwndModelProgressContainer, HWND_TOP,
                 0, barY, mainRC.right, barHeight,
                 SWP_SHOWWINDOW);
    
    // Create progress bar control (use Win32 PROGRESS_CLASS)
    if (!m_hwndModelProgressBar) {
        m_hwndModelProgressBar = CreateWindowExA(
            0, PROGRESS_CLASSA, "",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            8, 4, mainRC.right - 180, 24,
            m_hwndModelProgressContainer, nullptr, m_hInstance, nullptr);
        
        // VS Code dark theme colors
        SendMessage(m_hwndModelProgressBar, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 122, 204));
        SendMessage(m_hwndModelProgressBar, PBM_SETBKCOLOR, 0, (LPARAM)RGB(37, 37, 38));
        SendMessage(m_hwndModelProgressBar, PBM_SETRANGE32, 0, 1000);
    } else {
        SetWindowPos(m_hwndModelProgressBar, nullptr,
                     8, 4, mainRC.right - 180, 24, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    SendMessage(m_hwndModelProgressBar, PBM_SETPOS, 0, 0);
    
    // Create status label
    if (!m_hwndModelProgressLabel) {
        m_hwndModelProgressLabel = CreateWindowExA(
            0, "STATIC", operation.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            8, 4, mainRC.right - 180, 24,
            m_hwndModelProgressContainer, nullptr, m_hInstance, nullptr);
        
        if (m_hFontUI) {
            SendMessage(m_hwndModelProgressLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        }
    }
    SetWindowTextA(m_hwndModelProgressLabel, operation.c_str());
    // Position label on top of progress bar (overlaying text)
    SetWindowPos(m_hwndModelProgressLabel, HWND_TOP,
                 12, 4, mainRC.right - 184, 24, SWP_SHOWWINDOW);
    
    // Create cancel button
    if (!m_hwndModelCancelBtn) {
        m_hwndModelCancelBtn = CreateWindowExA(
            0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            mainRC.right - 160, 4, 80, 24,
            m_hwndModelProgressContainer, (HMENU)9903, m_hInstance, nullptr);
        
        if (m_hFontUI) {
            SendMessage(m_hwndModelCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        }
    } else {
        SetWindowPos(m_hwndModelCancelBtn, nullptr,
                     mainRC.right - 160, 4, 80, 24, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    EnableWindow(m_hwndModelCancelBtn, TRUE);
    
    // Paint the container with dark background
    ShowWindow(m_hwndModelProgressContainer, SW_SHOW);
    
    // Paint container background
    HDC hdc = GetDC(m_hwndModelProgressContainer);
    RECT containerRC;
    GetClientRect(m_hwndModelProgressContainer, &containerRC);
    HBRUSH bgBrush = CreateSolidBrush(RGB(37, 37, 38));
    FillRect(hdc, &containerRC, bgBrush);
    DeleteObject(bgBrush);
    ReleaseDC(m_hwndModelProgressContainer, hdc);
    
    // Start timer for polling progress updates from background threads
    SetTimer(m_hwndMain, MODEL_PROGRESS_TIMER_ID, 100, nullptr);
    
    LOG_INFO("Model progress bar displayed.");
}

// ============================================================================
// UPDATE PROGRESS — thread-safe, called from any thread
// ============================================================================
void Win32IDE::updateModelProgress(float percent, const std::string& statusText) {
    m_modelProgressPercent.store(percent);
    
    {
        std::lock_guard<std::mutex> lock(m_modelProgressMutex);
        m_modelProgressStatus = statusText;
    }
    
    // Signal the main thread to update UI
    if (m_hwndMain) {
        PostMessage(m_hwndMain, WM_MODEL_PROGRESS_UPDATE, (WPARAM)(int)(percent * 10.0f), 0);
    }
}

// ============================================================================
// HIDE PROGRESS BAR
// ============================================================================
void Win32IDE::hideModelProgressBar() {
    m_modelOperationActive.store(false);
    
    KillTimer(m_hwndMain, MODEL_PROGRESS_TIMER_ID);
    
    if (m_hwndModelProgressContainer) {
        ShowWindow(m_hwndModelProgressContainer, SW_HIDE);
    }
    
    // Trigger layout update to reclaim editor space
    if (m_hwndMain) {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        SendMessage(m_hwndMain, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
    }
    
    LOG_INFO("Model progress bar hidden.");
}

// ============================================================================
// CANCEL MODEL OPERATION
// ============================================================================
void Win32IDE::cancelModelOperation() {
    if (!m_modelOperationActive.load()) return;
    
    LOG_INFO("Model operation cancellation requested.");
    m_modelOperationCancelled.store(true);
    
    // Also set the inference stop flag if inference is running
    m_inferenceStopRequested = true;
    
    // Update the UI
    if (m_hwndModelProgressLabel) {
        SetWindowTextA(m_hwndModelProgressLabel, "Cancelling...");
    }
    if (m_hwndModelCancelBtn) {
        EnableWindow(m_hwndModelCancelBtn, FALSE);
    }
    
    // Update status bar
    showModelStatus("Model operation cancelled.", 3000);
}

// ============================================================================
// IS MODEL OPERATION IN PROGRESS
// ============================================================================
bool Win32IDE::isModelOperationInProgress() const {
    return m_modelOperationActive.load();
}

// ============================================================================
// SHOW STATUS MESSAGE — timed status bar message
// ============================================================================
void Win32IDE::showModelStatus(const std::string& text, int durationMs) {
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)text.c_str());
    }
    
    // Flash the status text in the output panel too
    appendToOutput(text + "\n", "Output", OutputSeverity::Info);
    
    // Set a timer to clear the status
    if (durationMs > 0 && m_hwndMain) {
        // Use timer ID 42 (IDT_STATUS_FLASH) to auto-clear
        SetTimer(m_hwndMain, 42, durationMs, nullptr);
    }
}

void Win32IDE::showModelLoadError(const std::string& detail) {
    const std::string msg = "Model load failed: " + detail;
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
    }
    appendToOutput(msg + "\n", "Errors", OutputSeverity::Error);
}

// ============================================================================
// PROGRESS WINDOW PROC — handles painting for the progress container
// ============================================================================
LRESULT CALLBACK Win32IDE::ModelProgressProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Dark background
            HBRUSH bgBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            
            // Top border accent
            HPEN accentPen = CreatePen(PS_SOLID, 1, RGB(0, 122, 204));
            HPEN oldPen = (HPEN)SelectObject(hdc, accentPen);
            MoveToEx(hdc, rc.left, rc.top, nullptr);
            LineTo(hdc, rc.right, rc.top);
            SelectObject(hdc, oldPen);
            DeleteObject(accentPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == 9903 && ide) { // Cancel button ID
                ide->cancelModelOperation();
            }
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// SUBAGENT / SWARM STATUS DISPLAY
// ============================================================================

void Win32IDE::showSubAgentProgress(const std::string& operation, int totalTasks) {
    if (!m_agenticBridge) return;

    showModelProgressBar("🔀 " + operation + " [0/" + std::to_string(totalTasks) + "]");
    LOG_INFO("SubAgent progress started: " + operation + " (" + std::to_string(totalTasks) + " tasks)");
}

void Win32IDE::updateSubAgentProgress(int completedTasks, int totalTasks, const std::string& currentTask) {
    if (totalTasks <= 0) return;
    float pct = (float)completedTasks / (float)totalTasks * 100.0f;
    std::string status = "🔀 " + currentTask + " [" +
                          std::to_string(completedTasks) + "/" +
                          std::to_string(totalTasks) + "]";
    updateModelProgress(pct, status);
}

void Win32IDE::hideSubAgentProgress() {
    hideModelProgressBar();
}

void Win32IDE::showSwarmStatus() {
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        appendToOutput("SubAgent system not initialized.\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto* mgr = m_agenticBridge->GetSubAgentManager();
    if (!mgr) {
        m_agenticBridge->Initialize("", m_agenticBridge->GetCurrentModel());
        mgr = m_agenticBridge->GetSubAgentManager();
    }
    if (!mgr) {
        appendToOutput("SubAgentManager not initialized.\n", "Output", OutputSeverity::Warning);
        return;
    }

    std::string summary = mgr->getStatusSummary();

    // Build detailed view
    std::ostringstream out;
    out << "=== SubAgent / Swarm Status ===\n" << summary << "\n\n";

    // Show todo list if any
    auto todos = mgr->getTodoList();
    if (!todos.empty()) {
        out << "--- Todo List ---\n";
        for (const auto& item : todos) {
            std::string icon;
            switch (item.status) {
                case TodoItem::Status::NotStarted: icon = "⬜"; break;
                case TodoItem::Status::InProgress: icon = "🔄"; break;
                case TodoItem::Status::Completed:  icon = "✅"; break;
                case TodoItem::Status::Failed:     icon = "❌"; break;
            }
            out << icon << " [" << item.id << "] " << item.title
                << " — " << item.statusString() << "\n";
            if (!item.description.empty()) {
                out << "    " << item.description << "\n";
            }
        }
        out << "\n";
    }

    // Show active sub-agents
    auto agents = mgr->getAllSubAgents();
    if (!agents.empty()) {
        out << "--- Sub-Agents ---\n";
        for (const auto& agent : agents) {
            std::string icon;
            switch (agent.state) {
                case SubAgent::State::Pending:   icon = "⏳"; break;
                case SubAgent::State::Running:   icon = "🔄"; break;
                case SubAgent::State::Completed: icon = "✅"; break;
                case SubAgent::State::Failed:    icon = "❌"; break;
                case SubAgent::State::Cancelled: icon = "🚫"; break;
            }
            out << icon << " " << agent.id << " [" << agent.stateString() << "] "
                << agent.description << " (" << agent.elapsedMs() << "ms)\n";
        }
        out << "\n";
    }

    // Show chain progress
    auto chainSteps = mgr->getChainSteps();
    if (!chainSteps.empty()) {
        out << "--- Chain Progress ---\n";
        for (const auto& step : chainSteps) {
            std::string icon;
            switch (step.state) {
                case SubAgent::State::Pending:   icon = "⏳"; break;
                case SubAgent::State::Running:   icon = "🔄"; break;
                case SubAgent::State::Completed: icon = "✅"; break;
                case SubAgent::State::Failed:    icon = "❌"; break;
                default:                         icon = "⬜"; break;
            }
            out << icon << " Step " << (step.index + 1)
                << ": " << step.promptTemplate.substr(0, 80)
                << (step.promptTemplate.size() > 80 ? "..." : "") << "\n";
        }
        out << "\n";
    }

    appendToOutput(out.str(), "Output", OutputSeverity::Info);
    showModelStatus("SubAgent status displayed", 3000);
}

// ============================================================================
// CONTEXT SLIDER LABEL UPDATE
// Synchronizes the context slider UI label with the current context size.
// ============================================================================
void Win32IDE::updateContextSliderLabel() {
    if (!m_hwndContextLabel) return;

    static const struct { size_t tokens; const char* label; } contextLabels[] = {
        { 4096,    "4K"   },
        { 32768,   "32K"  },
        { 65536,   "64K"  },
        { 131072,  "128K" },
        { 262144,  "256K" },
        { 524288,  "512K" },
        { 1048576, "1M"   },
    };

    std::string label = std::to_string(m_currentContextSize);
    for (const auto& entry : contextLabels) {
        if (entry.tokens == m_currentContextSize) {
            label = entry.label;
            break;
        }
    }

    SetWindowTextA(m_hwndContextLabel, label.c_str());

    // Sync the slider position (0=4K, 1=32K, 2=64K, ...)
    if (m_hwndContextSlider) {
        int pos = 0;
        for (int i = 0; i < 7; ++i) {
            if (contextLabels[i].tokens == m_currentContextSize) {
                pos = i;
                break;
            }
        }
        SendMessage(m_hwndContextSlider, TBM_SETPOS, TRUE, pos);
    }
}

// ============================================================================
// onContextSizeChanged — Handler for context slider value changes
// Maps slider position (0-6) to token count and propagates to engine.
// ============================================================================
void Win32IDE::onContextSizeChanged(int newValue) {
    static const size_t contextSizes[] = { 4096, 32768, 65536, 131072, 262144, 524288, 1048576 };
    
    if (newValue < 0 || newValue > 6) {
        newValue = 0;
    }
    
    m_currentContextSize = contextSizes[newValue];
    m_inferenceConfig.contextWindow = static_cast<int>(m_currentContextSize);

    if (m_nativeEngine) {
        m_nativeEngine->SetContextSize(m_currentContextSize);
    }
    if (m_agenticBridge) {
        std::string label;
        switch (newValue) {
            case 0: label = "4k"; break;
            case 1: label = "32k"; break;
            case 2: label = "64k"; break;
            case 3: label = "128k"; break;
            case 4: label = "256k"; break;
            case 5: label = "512k"; break;
            case 6: label = "1m"; break;
            default: label = "4k"; break;
        }
        m_agenticBridge->SetContextSize(label);
    }

    updateContextSliderLabel();
    appendToOutput("Context size changed to " + std::to_string(m_currentContextSize) + " tokens\n", "Output", OutputSeverity::Info);

    // Update context window display with new max
    setContextWindowMax(static_cast<int>(m_currentContextSize));
}

// ============================================================================
// CONTEXT WINDOW TOKEN USAGE DISPLAY
// ============================================================================
// Shows a live breakdown of token budget consumption in the status bar:
//   Ctx: 97.5K/128K  76%
// Categories: System | Tools | UserContext | Messages | ToolResults
// ============================================================================

std::string Win32IDE::formatTokenCount(int tokens) const {
    if (tokens >= 1000000) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1fM", tokens / 1000000.0);
        return buf;
    }
    if (tokens >= 1000) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1fK", tokens / 1000.0);
        return buf;
    }
    return std::to_string(tokens);
}

void Win32IDE::setContextWindowMax(int maxTokens) {
    m_contextUsage.maxTokens = maxTokens;
    updateContextWindowDisplay();
}

void Win32IDE::addContextTokens(const std::string& category, int tokens) {
    if (category == "system")       m_contextUsage.systemTokens      = tokens;
    else if (category == "tools")   m_contextUsage.toolDefTokens     = tokens;
    else if (category == "user")    m_contextUsage.userContextTokens = tokens;
    else if (category == "messages") m_contextUsage.messageTokens    = tokens;
    else if (category == "results") m_contextUsage.toolResultTokens  = tokens;
    updateContextWindowDisplay();
}

void Win32IDE::updateContextWindowDisplay() {
    if (!m_hwndStatusBar) return;

    int total = m_contextUsage.totalUsed();
    float pct = m_contextUsage.percentage();
    int pctInt = static_cast<int>(pct + 0.5f);

    std::string label = "Ctx: " + formatTokenCount(total) + "/" +
                        formatTokenCount(m_contextUsage.maxTokens) +
                        "  " + std::to_string(pctInt) + "%";

    if (m_contextUsage.isDanger()) {
        label += " !! DANGER";
    } else if (m_contextUsage.isWarning()) {
        label += " ! WARN";
    }

    SendMessage(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)label.c_str());
}

// ============================================================================
// STREAMING ENGINE SELECTION — Phase 9 Auto Engine Selection
// ============================================================================
// Auto-select the optimal ASM streaming engine based on model profile
// Uses the StreamingEngineRegistry scoring algorithm
// ============================================================================

#include "../core/streaming_engine_registry.h"

std::string Win32IDE::selectStreamingEngine(const std::string& modelPath) {
    LOG_INFO("Auto-selecting streaming engine for: " + modelPath);
    
    auto& registry = RawrXD::getStreamingEngineRegistry();
    
    // Detect model profile from file
    RawrXD::ModelProfile modelProfile = registry.detectModelProfile(modelPath);
    
    // Detect hardware if not cached
    RawrXD::HardwareProfile hwProfile = registry.detectHardware();
    
    // Select optimal engine
    RawrXD::EngineSelectionResult result = registry.selectEngine(modelProfile, hwProfile);
    
    if (result.success) {
        LOG_INFO("Selected engine: " + result.selectedEngine + " — " + result.reason);
        
        // Auto-switch to the selected engine
        bool switched = registry.selectEngineByName(result.selectedEngine);
        if (!switched) {
            LOG_ERROR("Failed to switch to selected engine: " + result.selectedEngine);
            return "";
        }
        
        // Update UI status
        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            appendToOutput("\n═══════════════════════════════════════════════════════\n", "ModelLoader", OutputSeverity::Success);
            appendToOutput(" Streaming Engine Auto-Selection\n", "ModelLoader", OutputSeverity::Success);
            appendToOutput("═══════════════════════════════════════════════════════\n", "ModelLoader", OutputSeverity::Success);
            appendToOutput("  Engine: " + result.selectedEngine + "\n", "ModelLoader", OutputSeverity::Info);
            appendToOutput("  Reason: " + result.reason + "\n", "ModelLoader", OutputSeverity::Info);
            
            if (!result.alternatives.empty()) {
                appendToOutput("  Alternatives: ", "ModelLoader", OutputSeverity::Info);
                for (size_t i = 0; i < result.alternatives.size(); i++) {
                    if (i > 0) appendToOutput(", ", "ModelLoader", OutputSeverity::Info);
                    appendToOutput(result.alternatives[i], "ModelLoader", OutputSeverity::Info);
                }
                appendToOutput("\n", "ModelLoader", OutputSeverity::Info);
            }
            
            appendToOutput("═══════════════════════════════════════════════════════\n\n", "ModelLoader", OutputSeverity::Success);
        }
        
        return result.selectedEngine;
    } else {
        LOG_ERROR("Engine selection failed: " + result.reason);
        appendToOutput("⚠ Engine selection failed: " + result.reason + "\n", "ModelLoader", OutputSeverity::Error);
        return "";
    }
}

std::string Win32IDE::getStreamingEngineDiagnostics() const {
    auto& registry = RawrXD::getStreamingEngineRegistry();
    return registry.getFullDiagnostics();
}

std::vector<std::string> Win32IDE::getAvailableStreamingEngines() const {
    auto& registry = RawrXD::getStreamingEngineRegistry();
    return registry.getRegisteredEngineNames();
}

bool Win32IDE::switchStreamingEngine(const std::string& engineName) {
    LOG_INFO("Manually switching streaming engine to: " + engineName);
    
    auto& registry = RawrXD::getStreamingEngineRegistry();
    bool success = registry.selectEngineByName(engineName);
    
    if (success) {
        appendToOutput("✓ Switched to streaming engine: " + engineName + "\n", "ModelLoader", OutputSeverity::Success);
    } else {
        appendToOutput("✗ Failed to switch to: " + engineName + "\n", "ModelLoader", OutputSeverity::Error);
    }
    
    return success;
}
