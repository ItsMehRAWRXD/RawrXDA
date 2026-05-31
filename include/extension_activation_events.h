// ============================================================================
// extension_activation_events.h — Extension Activation Events System
// ============================================================================
// PURPOSE:
//   Manages extension activation based on VS Code activation events:
//   - onCommand: Activate when user runs a command
//   - onLanguage: Activate when a file with specific language is opened
//   - onView: Activate when a view container is shown
//   - onUri: Activate when URI is opened
//   - onWebviewPanel: Activate on first webview panel creation
//
// Architecture: C++20 | Win32 | No exceptions | Qt-free
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Activation Event Types
// ============================================================================

enum class ActivationEventType {
    OnCommand,          // activationEvents: ["onCommand:command.id"]
    OnLanguage,         // activationEvents: ["onLanguage:languageId"]
    OnView,             // activationEvents: ["onView:viewId"]
    OnUri,              // activationEvents: ["onUri"]
    OnWebviewPanel,     // activationEvents: ["onWebviewPanel:type"]
    OnStartup,          // activationEvents: ["onStartup"]
    OnDebug,            // activationEvents: ["onDebug"]
  OnFileSystem,       // activationEvents: ["onFileSystem:scheme"]
};

// ============================================================================
// Activation Event Descriptor
// ============================================================================

struct ActivationEvent {
    ActivationEventType type;
    std::string         trigger;      // e.g., "ms-python.python.*" for onCommand
    std::string         detail;       // Additional context (e.g., language name)
    
    explicit ActivationEvent(ActivationEventType t, const std::string& trig, const std::string& det = "")
        : type(t), trigger(trig), detail(det) {}
};

// ============================================================================
// Extension Registration Record
// ============================================================================

struct RegisteredExtension {
    std::string                      extensionId;        // "publisher.extensionName"
    std::vector<ActivationEvent>     activationEvents;
    std::vector<std::string>         exposedCommands;    // From contributes.commands
    std::vector<std::string>         exposedLanguages;   // From contributes.languages
    
    std::atomic<bool>                isActivated{false};
    std::atomic<bool>                isDeactivating{false};
    
    // Callbacks
    std::function<void()>            onActivated;
    std::function<void()>            onDeactivated;
};

// ============================================================================
// Result Types
// ============================================================================

struct ActivationResult {
    bool                             success = false;
    std::string                      errorMessage;
    std::vector<std::string>         activatedExtensions;
    
    static ActivationResult Ok() {
        ActivationResult r; r.success = true; return r;
    }
    
    static ActivationResult Error(const std::string& msg) {
        ActivationResult r; r.success = false; r.errorMessage = msg; return r;
    }
};

// ============================================================================
// Extension Activation Events Manager
// ============================================================================

class ExtensionActivationEventManager {
public:
    explicit ExtensionActivationEventManager();
    ~ExtensionActivationEventManager();

    // Registration
    ActivationResult RegisterExtension(const std::string& extensionId,
                                       const std::vector<ActivationEvent>& events,
                                       std::function<void()> onActivate = nullptr);
    
    ActivationResult UnregisterExtension(const std::string& extensionId);

    // Event firing (host calls these when events occur)
    ActivationResult NotifyCommandExecuted(const std::string& commandId);
    ActivationResult NotifyLanguageOpened(const std::string& languageId);
    ActivationResult NotifyViewShown(const std::string& viewId);
    ActivationResult NotifyWebviewCreated(const std::string& panelType);
    ActivationResult NotifyStartup();
    ActivationResult NotifyDebugSessionStarted(const std::string& configuration);
    ActivationResult NotifyFileSystemSchemeOpened(const std::string& scheme);

    // Query
    bool IsExtensionActivated(const std::string& extensionId) const;
    std::vector<std::string> GetActivatedExtensions() const;
    std::vector<std::string> GetAvailableCommands() const;
    std::vector<std::string> GetSupportedLanguages() const;

    // Activation control
    ActivationResult ActivateExtension(const std::string& extensionId);
    ActivationResult DeactivateExtension(const std::string& extensionId);

private:
    mutable std::mutex m_registryLock;
    std::unordered_map<std::string, std::shared_ptr<RegisteredExtension>> m_registry;
    
    // Reverse indexes for fast lookup
    std::unordered_map<std::string, std::vector<std::string>> m_commandToExtensions;  // commandId -> extensionIds
    std::unordered_map<std::string, std::vector<std::string>> m_languageToExtensions; // languageId -> extensionIds
    std::unordered_map<std::string, std::vector<std::string>> m_viewToExtensions;     // viewId -> extensionIds
    
    // Track activated set
    std::unordered_set<std::string> m_activatedExtensions;

    // Internal activation
    ActivationResult ActivateExtensionInternal(const std::string& extensionId);
    
    // Helpers
    std::vector<std::string> GetExtensionsForEvent(const ActivationEvent& event) const;
    bool MatchesTrigger(const std::string& trigger, const std::string& value) const;
    std::string NormalizeTrigger(const std::string& raw) const;
};

// ============================================================================
// Global Helper Functions
// ============================================================================

// Get global manager instance
ExtensionActivationEventManager& GetActivationManager();

// Convenience functions
ActivationResult RegisterCommandExtension(const std::string& extensionId,
                                         const std::vector<std::string>& commands);
ActivationResult RegisterLanguageExtension(const std::string& extensionId,
                                          const std::vector<std::string>& languages);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // EXTENSION_ACTIVATION_EVENTS_H
