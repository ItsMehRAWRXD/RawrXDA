// ============================================================================
// Win32IDE_AgenticIntegration.cpp - Wires agentic system into existing Win32IDE
// ============================================================================

#include "Win32IDE_AgenticIntegration.h"
#include "Win32IDE.h"
#include "../win32ide/ExecModeToolbar.h"
#include "../win32ide/GhostOverlay.h"
#include "../agentic/ExecPipeline.h"
#include "../agentic/PatchEngine.h"
#include <string>
#include <vector>
#include <commctrl.h>

static Win32IDE_AgenticIntegration* g_agenticIntegration = nullptr;

static std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return std::string();
    }
    std::vector<char> buffer(static_cast<size_t>(bytes), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, buffer.data(), bytes, nullptr, nullptr);
    if (written <= 1) {
        return std::string();
    }
    return std::string(buffer.data(), static_cast<size_t>(written - 1));
}

Win32IDE_AgenticIntegration* GetAgenticIntegration() { return g_agenticIntegration; }
void SetAgenticIntegration(Win32IDE_AgenticIntegration* integration) { g_agenticIntegration = integration; }

Win32IDE_AgenticIntegration::Win32IDE_AgenticIntegration(Win32IDE* ide)
    : m_ide(ide)
{
    SetAgenticIntegration(this);
}

Win32IDE_AgenticIntegration::~Win32IDE_AgenticIntegration() {
    Shutdown();
    if (g_agenticIntegration == this) g_agenticIntegration = nullptr;
}

bool Win32IDE_AgenticIntegration::Initialize() {
    if (m_initialized) {
        OutputDebugStringW(L"[Agentic] Initialize skipped: already initialized\n");
        return false;
    }
    if (!m_ide) {
        OutputDebugStringW(L"[Agentic] Initialize failed: m_ide null\n");
        return false;
    }
    
    HWND hwndMain = m_ide->getMainWindow();
    HWND hwndEditor = m_ide->getEditor();
    HWND hwndToolbar = m_ide->getToolbar();
    
    if (!hwndMain || !IsWindow(hwndMain)) {
        OutputDebugStringW(L"[Agentic] Initialize failed: invalid hwndMain\n");
        return false;
    }
    if (!hwndEditor || !IsWindow(hwndEditor)) {
        OutputDebugStringW(L"[Agentic] Initialize failed: invalid hwndEditor\n");
        return false;
    }
    if (hwndToolbar && !IsWindow(hwndToolbar)) {
        OutputDebugStringW(L"[Agentic] Initialize warning: hwndToolbar invalid, falling back to main window\n");
        hwndToolbar = nullptr;
    }
    
    // 1. Create execution mode toolbar (fallback to main window if no toolbar)
    HWND toolbarParent = hwndToolbar ? hwndToolbar : hwndMain;
    m_toolbar = std::make_unique<RawrXD::UI::ExecModeToolbar>();
    if (!m_toolbar->Create(toolbarParent, 400, 2)) {
        OutputDebugStringW(L"[Agentic] Failed to create ExecModeToolbar\n");
    } else {
        OutputDebugStringW(L"[Agentic] ExecModeToolbar created\n");
    }
    
    // 2. Attach ghost overlay to editor
    m_ghostOverlay = std::make_unique<RawrXD::UI::GhostOverlay>();
    if (!m_ghostOverlay->Attach(hwndEditor)) {
        OutputDebugStringW(L"[Agentic] Failed to attach GhostOverlay\n");
    } else {
        OutputDebugStringW(L"[Agentic] GhostOverlay attached to editor\n");
    }
    
    // 3. Initialize execution pipeline
    m_pipeline = std::make_unique<RawrXD::Agentic::ExecPipeline>();
    RawrXD::Agentic::VerificationConfig config;
    config.ctestCommand = L"ctest -C Debug --output-on-failure";
    config.buildDir = L"build-ninja";
    config.timeoutSeconds = 300;
    config.selectiveTesting = true;
    m_pipeline->Initialize(config);
    
    // Set status callback to update IDE status bar with actual status text
    m_pipeline->SetStatusCallback([this](const std::wstring& status) {
        if (m_ide) {
            HWND hwndStatus = m_ide->getStatusBar();
            if (hwndStatus) {
                SendMessageW(hwndStatus, SB_SETTEXT, 2, (LPARAM)status.c_str());
            }
        }
    });
    
    OutputDebugStringW(L"[Agentic] Pipeline initialized\n");
    
    // 4. Sync with existing execution mode
    auto mode = m_ide->getExecutionMode();
    switch (mode) {
    case Win32IDE::ExecutionMode::Safe:
        m_toolbar->SetMode(RawrXD::UI::ExecMode::Shadow);
        break;
    case Win32IDE::ExecutionMode::Normal:
        m_toolbar->SetMode(RawrXD::UI::ExecMode::Normal);
        break;
    case Win32IDE::ExecutionMode::Unsafe:
        m_toolbar->SetMode(RawrXD::UI::ExecMode::Unsafe);
        break;
    case Win32IDE::ExecutionMode::Kernel:
        m_toolbar->SetMode(RawrXD::UI::ExecMode::Kernel);
        break;
    }
    
    m_initialized = true;
    SetTimer(hwndMain, kDeferredQueueTimerId, kDeferredQueueTickMs, nullptr);
    OutputDebugStringW(L"[Agentic] Integration initialized successfully\n");
    return true;
}

void Win32IDE_AgenticIntegration::Shutdown() {
    if (m_ide) {
        HWND hwndMain = m_ide->getMainWindow();
        if (hwndMain && IsWindow(hwndMain)) {
            KillTimer(hwndMain, kDeferredQueueTimerId);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_deferredMutex);
        std::queue<DeferredCommand> empty;
        m_deferredCommands.swap(empty);
        m_dispatchInProgress = false;
    }

    if (m_ghostOverlay) {
        m_ghostOverlay->Detach();
        m_ghostOverlay.reset();
    }
    if (m_toolbar) {
        m_toolbar->Destroy();
        m_toolbar.reset();
    }
    m_pipeline.reset();
    m_initialized = false;
}

LRESULT Win32IDE_AgenticIntegration::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_initialized) return 0;
    
    // Let toolbar handle its messages
    if (m_toolbar) {
        LRESULT result = m_toolbar->HandleMessage(hWnd, msg, wParam, lParam);
        if (result != 0) return result;
    }
    
    // Handle custom messages
    switch (msg) {
    case WM_EXEC_MODE_CHANGED: {
        // Sync toolbar mode back to IDE execution mode
        if (m_toolbar) {
            auto mode = m_toolbar->GetMode();
            switch (mode) {
            case RawrXD::UI::ExecMode::Shadow:
                m_ide->setExecutionMode(Win32IDE::ExecutionMode::Safe);
                break;
            case RawrXD::UI::ExecMode::Normal:
                m_ide->setExecutionMode(Win32IDE::ExecutionMode::Normal);
                break;
            case RawrXD::UI::ExecMode::Unsafe:
                m_ide->setExecutionMode(Win32IDE::ExecutionMode::Unsafe);
                break;
            case RawrXD::UI::ExecMode::Kernel:
                m_ide->setExecutionMode(Win32IDE::ExecutionMode::Kernel);
                break;
            }
        }
        return 0;
    }
    
    case WM_AGENT_APPLY_PATCH: {
        // Handle patch application from async test thread
        bool passed = (wParam == 1);
        auto* task = reinterpret_cast<RawrXD::Agentic::ExecTask*>(lParam);
        if (task) {
            if (passed && task->onApply) {
                task->onApply();
            } else if (!passed && task->onFail) {
                task->onFail(L"Tests failed");
            }
        }
        return 0;
    }

    case WM_TIMER: {
        if (wParam == kDeferredQueueTimerId) {
            DrainDeferredQueue();
            return 0;
        }
        break;
    }
    }
    
    return 0;
}

bool Win32IDE_AgenticIntegration::HandleAccelerator(HWND hWnd, WPARAM wParam) {
    if (!m_initialized || !m_toolbar) return false;
    return m_toolbar->HandleAccelerator(hWnd, wParam);
}

bool Win32IDE_AgenticIntegration::IsBackendReady() const {
    if (!m_ide) {
        return false;
    }

    return m_ide->isModelLoaded();
}

void Win32IDE_AgenticIntegration::DrainDeferredQueue() {
    if (!m_initialized || !m_pipeline) {
        return;
    }

    if (!IsBackendReady()) {
        return;
    }

    std::vector<DeferredCommand> ready;
    {
        std::lock_guard<std::mutex> lock(m_deferredMutex);
        if (m_dispatchInProgress) {
            return;
        }
        m_dispatchInProgress = true;

        while (!m_deferredCommands.empty()) {
            ready.push_back(m_deferredCommands.front());
            m_deferredCommands.pop();
        }
    }

    for (const auto& cmd : ready) {
        DispatchAgenticCommand(cmd.command, cmd.context);
    }

    {
        std::lock_guard<std::mutex> lock(m_deferredMutex);
        m_dispatchInProgress = false;
    }
}

void Win32IDE_AgenticIntegration::ExecuteAgenticCommand(const std::wstring& command, const std::wstring& context) {
    if (!m_initialized || !m_pipeline) return;

    if (!IsBackendReady()) {
        {
            std::lock_guard<std::mutex> lock(m_deferredMutex);
            m_deferredCommands.push(DeferredCommand{command, context, std::chrono::steady_clock::now()});
        }

        if (m_ide) {
            m_ide->appendToOutput(
                "[Agentic] Backend is still booting/deferred. Command queued and will run when model is ready.\n",
                "Agentic", Win32IDE::OutputSeverity::Warning);
        }

        OutputDebugStringW(L"[Agentic] Command deferred because backend is not ready\n");
        return;
    }

    DispatchAgenticCommand(command, context);
}

void Win32IDE_AgenticIntegration::DispatchAgenticCommand(const std::wstring& command, const std::wstring& context) {
    if (!m_initialized || !m_pipeline) {
        return;
    }

    RawrXD::Agentic::ExecTask task{};
    task.command = command;
    task.context = context;
    task.description = command + L" suggestion";

    task.onPreview = [this, context]() {
        OutputDebugStringW(L"[Agentic] Showing preview\n");
        ShowGhostSuggestion(context, false);
        if (m_ide) {
            m_ide->appendToOutput("[Agentic] Preview ready — review suggestion in editor.\n", "Agentic", Win32IDE::OutputSeverity::Info);
        }
    };

    task.onApply = [this]() {
        OutputDebugStringW(L"[Agentic] Patch applied\n");
        ClearGhostSuggestion();
        if (m_ide) {
            m_ide->appendToOutput("[Agentic] Patch applied successfully.\n", "Agentic", Win32IDE::OutputSeverity::Info);
        }
    };

    task.onFail = [this](const std::wstring& error) {
        OutputDebugStringW((L"[Agentic] Failed: " + error + L"\n").c_str());
        ClearGhostSuggestion();
        if (m_ide) {
            m_ide->appendToOutput("[Agentic] Failed: " + WideToUtf8(error) + "\n", "Agentic", Win32IDE::OutputSeverity::Error);
        }
    };

    task.onComplete = [this](RawrXD::Agentic::VerificationResult result) {
        OutputDebugStringW(L"[Agentic] Command complete\n");
        if (m_ide) {
            bool passed = (result == RawrXD::Agentic::VerificationResult::Passed);
            std::string msg = "[Agentic] Command complete. Result: " + std::string(passed ? "PASS" : "FAIL") + "\n";
            m_ide->appendToOutput(msg, "Agentic", passed ? Win32IDE::OutputSeverity::Info : Win32IDE::OutputSeverity::Warning);
        }
    };

    m_pipeline->Execute(task);
}

void Win32IDE_AgenticIntegration::ShowGhostSuggestion(const std::wstring& text, bool isMultiFile) {
    if (!m_ghostOverlay) return;
    
    RawrXD::UI::GhostSuggestion suggestion{};
    suggestion.type = RawrXD::UI::GhostType::Insert;
    suggestion.text = text;
    suggestion.isMultiFile = isMultiFile;
    suggestion.active = true;
    
    m_ghostOverlay->SetSuggestion(suggestion);
}

void Win32IDE_AgenticIntegration::ClearGhostSuggestion() {
    if (m_ghostOverlay) {
        m_ghostOverlay->ClearSuggestion();
    }
}

std::wstring Win32IDE_AgenticIntegration::GetExecutionModeLabel() const {
    if (!m_toolbar) return L"Unknown";
    return m_toolbar->GetModeLabel();
}

std::wstring Win32IDE_AgenticIntegration::GetExecutionModeDescription() const {
    if (!m_toolbar) return L"";
    return m_toolbar->GetModeDescription();
}

void Win32IDE_AgenticIntegration::UpdateStatusBar() {
    if (!m_ide) return;
    
    // Update IDE's existing status bar with agentic info
    std::wstring status = L"Agentic: " + GetExecutionModeLabel();
    
    HWND hwndStatus = m_ide->getStatusBar();
    if (hwndStatus) {
        SendMessageW(hwndStatus, SB_SETTEXT, 2, (LPARAM)status.c_str());
    }
}
