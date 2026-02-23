// Win32IDE_Quantum.cpp — Quantum Agent Orchestrator Integration for Win32IDE
// Phase 50: Multi-Model (1x-99x), Auto-Adjusting Timeouts, Balance/Max/Auto Modes

#include "Win32IDE.h"
#include "../agent/quantum_agent_orchestrator.hpp"
#include "../cli/quantum_cli_commands.hpp"

void Win32IDE::initQuantumOrchestrator() {
    if (m_quantumOrchestratorInitialized) return;
    
    LOG_INFO("Initializing Quantum Agent Orchestrator...");
    
    try {
        // Initialize the global orchestrator
        auto& orchestrator = RawrXD::Quantum::globalQuantumOrchestrator();
        
        // Set default strategy (can be overridden via menu/CLI)
        RawrXD::Quantum::ExecutionStrategy defaultStrategy = 
            RawrXD::Quantum::ExecutionStrategy::defaultStrategy();
        
        orchestrator.setStrategy(defaultStrategy);
        
        // Log initializ status
        LOG_INFO("Quantum Orchestrator initialized:");
        LOG_INFO("  Quality Mode: AUTO");
        LOG_INFO("  Model Count: 1x");
        LOG_INFO("  Agent Cycles: 1x");
        LOG_INFO("  Auto-Adjust Timeout: ENABLED");
        
        m_quantumOrchestratorInitialized = true;
        
        appendToOutput("\n═══════════════════════════════════════════════════════\n", 
                      "Quantum", OutputSeverity::Success);
        appendToOutput(" Quantum Agent Orchestrator Online\n", 
                      "Quantum", OutputSeverity::Success);
        appendToOutput("═══════════════════════════════════════════════════════\n", 
                      "Quantum", OutputSeverity::Success);
        appendToOutput(" Features:\n", "Quantum", OutputSeverity::Info);
        appendToOutput("  • Multi-Model Execution (1x-99x)\n", "Quantum", OutputSeverity::Info);
        appendToOutput("  • Multi-Agent Cycling (1x-99x)\n", "Quantum", OutputSeverity::Info);
        appendToOutput("  • Auto-Adjusting Terminal Timeouts (ML-based)\n", "Quantum", OutputSeverity::Info);
        appendToOutput("  • Balance/Max/Auto Quality Modes\n", "Quantum", OutputSeverity::Info);
        appendToOutput("  • Production Audit + Auto-Fix\n", "Quantum", OutputSeverity::Info);
        appendToOutput("  • Bypass Controls (Token/Complexity/Time)\n", "Quantum", OutputSeverity::Info);
        appendToOutput("\n", "Quantum", OutputSeverity::Info);
        appendToOutput(" CLI Commands: Type '!quantum help' for usage\n", 
                      "Quantum", OutputSeverity::Info);
        appendToOutput("═══════════════════════════════════════════════════════\n\n", 
                      "Quantum", OutputSeverity::Success);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize Quantum Orchestrator: " + std::string(e.what()));
        appendToOutput("❌ Quantum Orchestrator initialization failed: " + std::string(e.what()) + "\n",
                      "Quantum", OutputSeverity::Error);
    }
}

bool Win32IDE::handleQuantumCommand(int commandId) {
    if (!m_quantumOrchestratorInitialized) {
        initQuantumOrchestrator();
    }
    
    auto& orchestrator = RawrXD::Quantum::globalQuantumOrchestrator();
    
    switch (commandId) {
        case IDM_QUANTUM_MODE_AUTO:
            orchestrator.setQualityMode(RawrXD::Quantum::QualityMode::Auto);
            appendToOutput("✓ Quantum Mode: AUTO (adaptive)\n", "Quantum", OutputSeverity::Success);
            return true;
            
        case IDM_QUANTUM_MODE_BALANCE:
            orchestrator.setQualityMode(RawrXD::Quantum::QualityMode::Balance);
            appendToOutput("✓ Quantum Mode: BALANCE (speed + quality)\n", "Quantum", OutputSeverity::Success);
            return true;
            
        case IDM_QUANTUM_MODE_MAX:
            orchestrator.setQualityMode(RawrXD::Quantum::QualityMode::Max);
            appendToOutput("✓ Quantum Mode: MAX (maximum quality)\n", "Quantum", OutputSeverity::Success);
            appendToOutput("  ⚡ Token and complexity limits bypassed\n", "Quantum", OutputSeverity::Info);
            return true;
            
        case IDM_QUANTUM_MODELS_SET: {
            // Show input dialog to set model count
            wchar_t buffer[32] = {};
            if (DialogBoxWithInput(L"Set Model Count (1-99)", L"Enter number of parallel models:",
                                  buffer, _countof(buffer))) {
                int count = _wtoi(buffer);
                if (count >= 1 && count <= 99) {
                    orchestrator.setModelCount(count);
                    appendToOutput("✓ Parallel models set to " + std::to_string(count) + "x\n",
                                 "Quantum", OutputSeverity::Success);
                } else {
                    appendToOutput("❌ Model count must be between 1 and 99\n",
                                 "Quantum", OutputSeverity::Error);
                }
            }
            return true;
        }
            
        case IDM_QUANTUM_AGENTS_SET: {
            // Show input dialog to set agent cycle count
            wchar_t buffer[32] = {};
            if (DialogBoxWithInput(L"Set Agent Cycle Count (1-99)", L"Enter number of agent cycles:",
                                  buffer, _countof(buffer))) {
                int count = _wtoi(buffer);
                if (count >= 1 && count <= 99) {
                    orchestrator.setAgentCycleCount(count);
                    appendToOutput("✓ Agent cycles set to " + std::to_string(count) + "x\n",
                                 "Quantum", OutputSeverity::Success);
                } else {
                    appendToOutput("❌ Agent cycle count must be between 1 and 99\n",
                                 "Quantum", OutputSeverity::Error);
                }
            }
            return true;
        }
            
        case IDM_QUANTUM_STATUS: {
            auto strategy = orchestrator.getStrategy();
            auto stats = orchestrator.getStatistics();
            
            appendToOutput("\n╔══════════════════════════════════════════════════════════════╗\n",
                         "Quantum", OutputSeverity::Info);
            appendToOutput("║  QUANTUM ORCHESTRATOR STATUS                             ║\n",
                         "Quantum", OutputSeverity::Info);
            appendToOutput("╚══════════════════════════════════════════════════════════════╝\n\n",
                         "Quantum", OutputSeverity::Info);
            
            appendToOutput("CONFIGURATION:\n", "Quantum", OutputSeverity::Info);
            appendToOutput("  Quality Mode:      ", "Quantum", OutputSeverity::Info);
            switch (strategy.mode) {
                case RawrXD::Quantum::QualityMode::Auto:
                    appendToOutput("AUTO\n", "Quantum", OutputSeverity::Info);
                    break;
                case RawrXD::Quantum::QualityMode::Balance:
                    appendToOutput("BALANCE\n", "Quantum", OutputSeverity::Info);
                    break;
                case RawrXD::Quantum::QualityMode::Max:
                    appendToOutput("MAX\n", "Quantum", OutputSeverity::Info);
                    break;
            }
            appendToOutput("  Parallel Models:   " + std::to_string(strategy.modelCount) + "x\n",
                         "Quantum", OutputSeverity::Info);
            appendToOutput("  Agent Cycles:      " + std::to_string(strategy.agentCycleCount) + "x\n",
                         "Quantum", OutputSeverity::Info);
            appendToOutput("  Auto-Adjust Time:  " + std::string(strategy.autoAdjustTimeout ? "ENABLED" : "DISABLED") + "\n\n",
                         "Quantum", OutputSeverity::Info);
            
            appendToOutput("STATISTICS:\n", "Quantum", OutputSeverity::Info);
            appendToOutput("  Tasks Executed:    " + std::to_string(stats.totalTasksExecuted) + "\n",
                         "Quantum", OutputSeverity::Info);
            appendToOutput("  Total Iterations:  " + std::to_string(stats.totalIterations) + "\n",
                         "Quantum", OutputSeverity::Info);
            appendToOutput("  Success Rate:      " + std::to_string(stats.successRate * 100) + "%\n\n",
                         "Quantum", OutputSeverity::Info);
            
            return true;
        }
            
        case IDM_QUANTUM_STATS: {
            RawrXD::Quantum::CLI::showStatistics();
            return true;
        }
            
        case IDM_QUANTUM_AUDIT: {
            // Audit current workspace
            std::string rootPath = "D:\\rawrxd\\src";
            appendToOutput("🔍 Auditing codebase: " + rootPath + "\n",
                         "Quantum", OutputSeverity::Info);
            
            auto entries = orchestrator.auditProductionReadiness(rootPath);
            
            appendToOutput("\n✓ Audit complete: " + std::to_string(entries.size()) + " issues found\n",
                         "Quantum", OutputSeverity::Success);
            appendToOutput("  Use 'Quantum → Execute Top N Audit Items' to auto-fix\n\n",
                         "Quantum", OutputSeverity::Info);
            
            return true;
        }
            
        case IDM_QUANTUM_AUDIT_EXEC: {
            // Show dialog to get N value
            wchar_t buffer[32] = {};
            if (DialogBoxWithInput(L"Execute Top N Audit Items", L"How many top issues to fix?",
                                  buffer, _countof(buffer))) {
                int count = _wtoi(buffer);
                if (count > 0 && count <= 100) {
                    appendToOutput("🚀 Executing top " + std::to_string(count) + " audit items...\n",
                                 "Quantum", OutputSeverity::Info);
                    
                    // Get audit entries
                    auto entries = orchestrator.auditProductionReadiness("D:\\rawrxd\\src");
                    
                    // Execute with current strategy
                    auto strategy = orchestrator.getStrategy();
                    auto result = orchestrator.executeAuditItems(entries, count, strategy);
                    
                    if (result.success) {
                        appendToOutput("\n✅ Audit execution complete!\n", "Quantum", OutputSeverity::Success);
                        appendToOutput("  " + result.detail + "\n\n", "Quantum", OutputSeverity::Info);
                    } else {
                        appendToOutput("\n❌ Audit execution failed: " + result.detail + "\n\n",
                                     "Quantum", OutputSeverity::Error);
                    }
                } else {
                    appendToOutput("❌ Count must be between 1 and 100\n",
                                 "Quantum", OutputSeverity::Error);
                }
            }
            return true;
        }
            
        default:
            return false;
    }
}

// Helper for input dialog (declared in Win32IDE class) — Win32 modal dialog without .rc template
namespace {
struct InputDialogParams {
    const wchar_t* prompt;
    wchar_t* buffer;
    size_t bufferSize;
    bool ok = false;
};
enum { IDC_PROMPT = 1001, IDC_EDIT = 1002, IDC_OK = 1003, IDC_CANCEL = 1004 };
static const UINT WM_INPUTDLG_CLOSED = WM_APP + 2;
static LRESULT CALLBACK InputDialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InputDialogParams* p = reinterpret_cast<InputDialogParams*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        p = reinterpret_cast<InputDialogParams*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p);
        if (!p) break;
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
        CreateWindowW(L"Static", p->prompt ? p->prompt : L"",
            WS_CHILD | WS_VISIBLE, 12, 12, 316, 16, hwnd, (HMENU)(UINT_PTR)IDC_PROMPT, hInst, nullptr);
        CreateWindowW(L"Edit", p->buffer && p->bufferSize > 0 ? p->buffer : L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT, 12, 34, 316, 24,
            hwnd, (HMENU)(UINT_PTR)IDC_EDIT, hInst, nullptr);
        CreateWindowW(L"Button", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            132, 66, 72, 26, hwnd, (HMENU)(UINT_PTR)IDC_OK, hInst, nullptr);
        CreateWindowW(L"Button", L"Cancel", WS_CHILD | WS_VISIBLE,
            212, 66, 72, 26, hwnd, (HMENU)(UINT_PTR)IDC_CANCEL, hInst, nullptr);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_OK) {
            p = reinterpret_cast<InputDialogParams*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (p && p->buffer && p->bufferSize > 0)
                GetDlgItemTextW(hwnd, IDC_EDIT, p->buffer, (int)p->bufferSize);
            if (p) p->ok = true;
            PostMessageW(GetParent(hwnd), WM_INPUTDLG_CLOSED, 0, 0);
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_CANCEL) {
            PostMessageW(GetParent(hwnd), WM_INPUTDLG_CLOSED, 0, 0);
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        PostMessageW(GetParent(hwnd), WM_INPUTDLG_CLOSED, 0, 0);
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
static ATOM s_inputDialogClass = 0;
static HWND RunInputDialog(HWND parent, const wchar_t* title, InputDialogParams* params) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    if (!s_inputDialogClass) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = InputDialogWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"RawrXD_InputDialog";
        s_inputDialogClass = RegisterClassExW(&wc);
        if (!s_inputDialogClass) return nullptr;
    }
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
        L"RawrXD_InputDialog", title ? title : L"Input",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 348, 128, parent, nullptr, hInst, params);
    if (!dlg) return nullptr;
    ShowWindow(dlg, SW_SHOW);
    return dlg;
}
}

bool Win32IDE::DialogBoxWithInput(const wchar_t* title, const wchar_t* prompt,
                                  wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;
    buffer[0] = L'\0';
    InputDialogParams params = { prompt, buffer, bufferSize, false };
    HWND dlg = RunInputDialog(m_hwnd, title, &params);
    if (!dlg) return false;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_INPUTDLG_CLOSED && msg.hwnd == m_hwnd)
            break;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return params.ok;
}
