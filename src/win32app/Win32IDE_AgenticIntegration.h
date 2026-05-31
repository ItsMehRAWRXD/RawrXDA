// ============================================================================
// Win32IDE Agentic Integration - Wires ExecModeToolbar, GhostOverlay, ExecPipeline
// into the existing Win32IDE window without modifying Win32IDE.h/cpp directly
// ============================================================================

#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <chrono>

// Forward declarations
class Win32IDE;

namespace RawrXD {
namespace UI {
    class ExecModeToolbar;
    class GhostOverlay;
}
namespace Agentic {
    class ExecPipeline;
}
}

// ============================================================================
// Integration API
// ============================================================================

class Win32IDE_AgenticIntegration {
public:
    Win32IDE_AgenticIntegration(Win32IDE* ide);
    ~Win32IDE_AgenticIntegration();

    // Initialize all agentic components (call after Win32IDE::createWindow)
    bool Initialize();
    
    // Shutdown and cleanup
    void Shutdown();
    
    // Handle window messages (call from Win32IDE WndProc)
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Handle keyboard accelerators (call before TranslateMessage)
    bool HandleAccelerator(HWND hWnd, WPARAM wParam);
    
    // Execute an agentic command (/fix, /refactor, etc.)
    void ExecuteAgenticCommand(const std::wstring& command, const std::wstring& context);
    
    // Show ghost suggestion in editor
    void ShowGhostSuggestion(const std::wstring& text, bool isMultiFile = false);
    void ClearGhostSuggestion();
    
    // Get current execution mode
    std::wstring GetExecutionModeLabel() const;
    std::wstring GetExecutionModeDescription() const;
    
    // Status bar update
    void UpdateStatusBar();
    
    // Check if integration is initialized
    bool IsInitialized() const { return m_initialized; }

private:
    struct DeferredCommand {
        std::wstring command;
        std::wstring context;
        std::chrono::steady_clock::time_point enqueuedAt;
    };

    void DispatchAgenticCommand(const std::wstring& command, const std::wstring& context);
    bool IsBackendReady() const;
    void DrainDeferredQueue();

    Win32IDE* m_ide;
    std::unique_ptr<RawrXD::UI::ExecModeToolbar> m_toolbar;
    std::unique_ptr<RawrXD::UI::GhostOverlay> m_ghostOverlay;
    std::unique_ptr<RawrXD::Agentic::ExecPipeline> m_pipeline;

    mutable std::mutex m_deferredMutex;
    std::queue<DeferredCommand> m_deferredCommands;
    bool m_dispatchInProgress = false;
    static constexpr UINT_PTR kDeferredQueueTimerId = 0xA610;
    static constexpr UINT kDeferredQueueTickMs = 200;

    bool m_initialized = false;
};

// Global accessor
Win32IDE_AgenticIntegration* GetAgenticIntegration();
void SetAgenticIntegration(Win32IDE_AgenticIntegration* integration);
