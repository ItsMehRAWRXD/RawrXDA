#include "AgenticIDEIntegration.h"
#include "../win32app/Win32IDE.h"  // For Win32IDE access
#include <windows.h>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

AgenticIDEIntegration::AgenticIDEIntegration() = default;

AgenticIDEIntegration::~AgenticIDEIntegration() {
    shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool AgenticIDEIntegration::initialize(Win32IDE* ide, HINSTANCE hInstance) {
    if (m_initialized) return true;
    if (!ide) return false;

    m_ide = ide;
    m_hInstance = hInstance;

    // Create router
    m_router = std::make_unique<AgenticRouterBridge>();
    
    // Create UI bridge
    m_ui = std::make_unique<AgenticUIBridge>();
    m_ui->initialize(reinterpret_cast<HWND>(ide->getMainWindowHandle()), hInstance);

    // Wire callbacks
    setupCallbacks();

    // Initialize router (will need UnifiedInferenceRouter and AgenticExecutor from IDE)
    // This is deferred until IDE provides them
    m_initialized = true;

    return true;
}

void AgenticIDEIntegration::shutdown() {
    if (!m_initialized) return;

    if (m_ui) {
        m_ui->shutdown();
        m_ui.reset();
    }

    if (m_router) {
        m_router->shutdown();
        m_router.reset();
    }

    m_ide = nullptr;
    m_hInstance = nullptr;
    m_initialized = false;
}

// ============================================================================
// MODE CONTROL
// ============================================================================

void AgenticIDEIntegration::toggleAutopilotMode() {
    if (!m_ui) return;
    m_ui->toggleMode();
}

void AgenticIDEIntegration::setAutopilotMode(AgenticMode mode) {
    if (!m_ui) return;
    m_ui->setMode(mode);
}

AgenticMode AgenticIDEIntegration::getAutopilotMode() const {
    if (!m_ui) return AgenticMode::Passive;
    return m_ui->getCurrentMode();
}

const char* AgenticIDEIntegration::getAutopilotModeString() const {
    if (!m_router) return "Unknown";
    return m_router->getModeString();
}

// ============================================================================
// MANUAL TRIGGERS
// ============================================================================

void AgenticIDEIntegration::analyzeCurrentFile(const std::string& filePath) {
    if (!m_router) return;
    m_router->triggerFileAnalysis(filePath);
}

void AgenticIDEIntegration::analyzeProject(const std::string& projectPath) {
    if (!m_router) return;
    m_router->triggerProjectAnalysis(projectPath);
}

void AgenticIDEIntegration::triggerErrorRecovery(const std::string& errorContext) {
    if (!m_router) return;
    m_router->triggerErrorRecovery(errorContext);
}

// ============================================================================
// ACTION APPROVAL
// ============================================================================

void AgenticIDEIntegration::approveSuggestion(const std::string& suggestionId) {
    if (!m_router) return;
    m_router->approveAction(suggestionId);
}

void AgenticIDEIntegration::rejectSuggestion(const std::string& suggestionId) {
    if (!m_router) return;
    m_router->rejectAction(suggestionId);
}

void AgenticIDEIntegration::dismissAllSuggestions() {
    if (!m_router) return;
    m_router->clearSuggestions();
}

// ============================================================================
// UI CONTROL
// ============================================================================

void AgenticIDEIntegration::showSuggestionPanel() {
    if (!m_ui) return;
    m_ui->showSuggestionPanel();
}

void AgenticIDEIntegration::hideSuggestionPanel() {
    if (!m_ui) return;
    m_ui->hideSuggestionPanel();
}

void AgenticIDEIntegration::toggleSuggestionPanel() {
    if (!m_ui) return;
    m_ui->toggleSuggestionPanel();
}

// ============================================================================
// STATUS
// ============================================================================

bool AgenticIDEIntegration::hasPendingSuggestions() const {
    if (!m_router) return false;
    return m_router->hasPendingSuggestions();
}

int AgenticIDEIntegration::getPendingSuggestionCount() const {
    if (!m_router) return 0;
    return (int)m_router->getPendingSuggestions().size();
}

std::vector<AgenticSuggestion> AgenticIDEIntegration::getPendingSuggestions() const {
    if (!m_router) return {};
    return m_router->getPendingSuggestions();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void AgenticIDEIntegration::setupCallbacks() {
    if (!m_router || !m_ui) return;

    // Router -> UI
    m_router->onSuggestion([this](const AgenticSuggestion& suggestion) {
        onRouterSuggestion(suggestion);
    });

    m_router->onAction([this](const AgenticAction& action, bool success) {
        onRouterAction(action, success);
    });

    m_router->onModeChange([this](AgenticMode oldMode, AgenticMode newMode) {
        onRouterModeChange(oldMode, newMode);
    });

    m_router->onLog([this](const std::string& message) {
        onRouterLog(message);
    });

    // UI -> Router
    m_ui->onNotification([this](const AgenticNotification& notification) {
        onUINotification(notification);
    });

    m_ui->onModeToggle([this](AgenticMode mode) {
        onUIModeToggle(mode);
    });
}

void AgenticIDEIntegration::onRouterSuggestion(const AgenticSuggestion& suggestion) {
    // Forward to UI
    if (m_ui) {
        m_ui->showSuggestion(suggestion);
    }

    // Forward to IDE callback
    if (m_suggestionCb) {
        m_suggestionCb(suggestion);
    }

    // Update status bar
    if (m_statusCb) {
        m_statusCb("New agentic suggestion: " + suggestion.title);
    }
}

void AgenticIDEIntegration::onRouterAction(const AgenticAction& action, bool success) {
    // Forward to UI
    if (m_ui) {
        m_ui->showActionResult(action, success);
    }

    // Update status bar
    if (m_statusCb) {
        m_statusCb("Agentic action " + std::string(success ? "completed" : "failed") + ": " + action.description);
    }
}

void AgenticIDEIntegration::onRouterModeChange(AgenticMode oldMode, AgenticMode newMode) {
    // Update UI to reflect new mode
    if (m_ui) {
        m_ui->updateStatusBar();
    }

    // Notify IDE
    if (m_statusCb) {
        m_statusCb(std::string("Agentic mode: ") + m_router->getModeString());
    }
}

void AgenticIDEIntegration::onRouterLog(const std::string& message) {
    // Log to IDE output
    if (m_statusCb) {
        m_statusCb(message);
    }
}

void AgenticIDEIntegration::onUINotification(const AgenticNotification& notification) {
    // Forward to IDE notification system
    if (m_notificationCb) {
        m_notificationCb(notification.title, notification.message);
    }
}

void AgenticIDEIntegration::onUIModeToggle(AgenticMode mode) {
    // Sync router with UI mode change
    if (m_router) {
        m_router->setMode(mode);
    }
}

} // namespace Agentic
} // namespace RawrXD
