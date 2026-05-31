#pragma once

/**
 * AgenticIDEIntegration.h
 * 
 * Central integration point for Agentic capabilities into RawrXD IDE.
 * This header provides the minimal API surface for:
 * - Initializing the agentic subsystem
 - Toggling autopilot mode
 * - Receiving suggestions and notifications
 * - Executing agentic actions
 * 
 * Usage in Win32IDE:
 *   #include "agentic/AgenticIDEIntegration.h"
 *   
 *   // In initialization:
 *   m_agenticIntegration = std::make_unique<AgenticIDEIntegration>();
 *   m_agenticIntegration->initialize(this, m_hInstance);
 *   
 *   // In status bar click:
 *   m_agenticIntegration->toggleAutopilotMode();
 */

#include "AgenticRouterBridge.h"
#include "AgenticUIBridge.h"
#include <memory>
#include <functional>

// Forward declaration for IDE callback
class Win32IDE;

namespace RawrXD {
namespace Agentic {

class AgenticIDEIntegration {
public:
    AgenticIDEIntegration();
    ~AgenticIDEIntegration();

    // Initialize with IDE instance and module handle
    bool initialize(Win32IDE* ide, HINSTANCE hInstance);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Mode control
    void toggleAutopilotMode();
    void setAutopilotMode(AgenticMode mode);
    AgenticMode getAutopilotMode() const;
    const char* getAutopilotModeString() const;

    // Manual triggers
    void analyzeCurrentFile(const std::string& filePath);
    void analyzeProject(const std::string& projectPath);
    void triggerErrorRecovery(const std::string& errorContext);

    // Action approval
    void approveSuggestion(const std::string& suggestionId);
    void rejectSuggestion(const std::string& suggestionId);
    void dismissAllSuggestions();

    // UI control
    void showSuggestionPanel();
    void hideSuggestionPanel();
    void toggleSuggestionPanel();

    // Status
    bool hasPendingSuggestions() const;
    int getPendingSuggestionCount() const;
    std::vector<AgenticSuggestion> getPendingSuggestions() const;

    // Callbacks for IDE
    using StatusUpdateCallback = std::function<void(const std::string& status)>;
    using SuggestionCallback = std::function<void(const AgenticSuggestion& suggestion)>;
    using NotificationCallback = std::function<void(const std::string& title, const std::string& message)>;

    void onStatusUpdate(StatusUpdateCallback cb) { m_statusCb = cb; }
    void onSuggestion(SuggestionCallback cb) { m_suggestionCb = cb; }
    void onNotification(NotificationCallback cb) { m_notificationCb = cb; }

    // Direct access (for advanced use)
    AgenticRouterBridge* getRouter() const { return m_router.get(); }
    AgenticUIBridge* getUI() const { return m_ui.get(); }

private:
    void setupCallbacks();
    void onRouterSuggestion(const AgenticSuggestion& suggestion);
    void onRouterAction(const AgenticAction& action, bool success);
    void onRouterModeChange(AgenticMode oldMode, AgenticMode newMode);
    void onRouterLog(const std::string& message);
    void onUINotification(const AgenticNotification& notification);
    void onUIModeToggle(AgenticMode mode);

    bool m_initialized = false;
    Win32IDE* m_ide = nullptr;
    HINSTANCE m_hInstance = nullptr;

    std::unique_ptr<AgenticRouterBridge> m_router;
    std::unique_ptr<AgenticUIBridge> m_ui;

    StatusUpdateCallback m_statusCb;
    SuggestionCallback m_suggestionCb;
    NotificationCallback m_notificationCb;
};

} // namespace Agentic
} // namespace RawrXD
