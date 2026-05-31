// ============================================================================
// extension_activation_events.cpp — Extension Activation Events Implementation
// ============================================================================
// Architecture: C++20 | Win32 | Function-pointer callbacks | No exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "extension_activation_events.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Global Manager Instance
// ============================================================================

static ExtensionActivationEventManager* g_activationManager = nullptr;

ExtensionActivationEventManager& GetActivationManager() {
    if (!g_activationManager) {
        g_activationManager = new ExtensionActivationEventManager();
    }
    return *g_activationManager;
}

// ============================================================================
// Utility Functions
// ============================================================================

static std::string ToLowerAscii(const std::string& str) {
    std::string result = str;
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

static bool MatchesWildcard(const std::string& pattern, const std::string& value) {
    // Simple wildcard matching: * matches anything
    // Patterns:
    //   "ms-python.*" matches "ms-python.python", "ms-python.debugger"
    //   "python" matches "python" exactly
    //   "onLanguage:*" matches any language
    
    size_t patIdx = 0, valIdx = 0;
    
    while (patIdx < pattern.size() && valIdx < value.size()) {
        if (pattern[patIdx] == '*') {
            if (patIdx + 1 >= pattern.size()) return true;  // * at end matches rest
            
            char nextChar = pattern[patIdx + 1];
            while (valIdx < value.size() && value[valIdx] != nextChar) {
                ++valIdx;
            }
            ++patIdx;
        } else if (pattern[patIdx] == value[valIdx]) {
            ++patIdx;
            ++valIdx;
        } else {
            return false;
        }
    }
    
    // Handle remaining pattern wildcards
    while (patIdx < pattern.size() && pattern[patIdx] == '*') {
        ++patIdx;
    }
    
    return patIdx == pattern.size() && valIdx == value.size();
}

// ============================================================================
// ExtensionActivationEventManager Implementation
// ============================================================================

ExtensionActivationEventManager::ExtensionActivationEventManager() {
}

ExtensionActivationEventManager::~ExtensionActivationEventManager() {
}

ActivationResult ExtensionActivationEventManager::RegisterExtension(
    const std::string& extensionId,
    const std::vector<ActivationEvent>& events,
    std::function<void()> onActivate
) {
    if (extensionId.empty()) {
        return ActivationResult::Error("Extension ID cannot be empty");
    }

    std::lock_guard<std::mutex> lock(m_registryLock);

    auto ext = std::make_shared<RegisteredExtension>();
    ext->extensionId = extensionId;
    ext->activationEvents = events;
    ext->onActivated = onActivate;

    // Build reverse indexes for fast trigger matching
    for (const auto& event : events) {
        switch (event.type) {
            case ActivationEventType::OnCommand: {
                m_commandToExtensions[event.trigger].push_back(extensionId);
                break;
            }
            case ActivationEventType::OnLanguage: {
                m_languageToExtensions[event.trigger].push_back(extensionId);
                break;
            }
            case ActivationEventType::OnView: {
                m_viewToExtensions[event.trigger].push_back(extensionId);
                break;
            }
            default:
                // Other event types handled but not pre-indexed for MVP
                break;
        }
    }

    m_registry[extensionId] = ext;
    return ActivationResult::Ok();
}

ActivationResult ExtensionActivationEventManager::UnregisterExtension(
    const std::string& extensionId
) {
    if (extensionId.empty()) {
        return ActivationResult::Error("Extension ID cannot be empty");
    }

    std::lock_guard<std::mutex> lock(m_registryLock);

    auto it = m_registry.find(extensionId);
    if (it == m_registry.end()) {
        return ActivationResult::Error("Extension not registered: " + extensionId);
    }

    // Remove from reverse indexes
    const auto& events = it->second->activationEvents;
    for (const auto& event : events) {
        switch (event.type) {
            case ActivationEventType::OnCommand: {
                auto& vec = m_commandToExtensions[event.trigger];
                vec.erase(std::remove(vec.begin(), vec.end(), extensionId), vec.end());
                break;
            }
            case ActivationEventType::OnLanguage: {
                auto& vec = m_languageToExtensions[event.trigger];
                vec.erase(std::remove(vec.begin(), vec.end(), extensionId), vec.end());
                break;
            }
            case ActivationEventType::OnView: {
                auto& vec = m_viewToExtensions[event.trigger];
                vec.erase(std::remove(vec.begin(), vec.end(), extensionId), vec.end());
                break;
            }
            default:
                break;
        }
    }

    m_activatedExtensions.erase(extensionId);
    m_registry.erase(it);

    return ActivationResult::Ok();
}

ActivationResult ExtensionActivationEventManager::NotifyCommandExecuted(
    const std::string& commandId
) {
    if (commandId.empty()) {
        return ActivationResult::Error("Command ID cannot be empty");
    }

    ActivationResult result;
    result.success = true;

    std::lock_guard<std::mutex> lock(m_registryLock);

    // Find extensions that should activate for this command
    for (auto& [trigger, extIds] : m_commandToExtensions) {
        if (MatchesWildcard(trigger, commandId)) {
            for (const auto& extId : extIds) {
                if (m_activatedExtensions.find(extId) == m_activatedExtensions.end()) {
                    if (ActivateExtensionInternal(extId).success) {
                        result.activatedExtensions.push_back(extId);
                    }
                }
            }
        }
    }

    return result;
}

ActivationResult ExtensionActivationEventManager::NotifyLanguageOpened(
    const std::string& languageId
) {
    if (languageId.empty()) {
        return ActivationResult::Error("Language ID cannot be empty");
    }

    ActivationResult result;
    result.success = true;

    std::lock_guard<std::mutex> lock(m_registryLock);

    // Find extensions that should activate for this language
    auto it = m_languageToExtensions.find(languageId);
    if (it != m_languageToExtensions.end()) {
        for (const auto& extId : it->second) {
            if (m_activatedExtensions.find(extId) == m_activatedExtensions.end()) {
                if (ActivateExtensionInternal(extId).success) {
                    result.activatedExtensions.push_back(extId);
                }
            }
        }
    }

    return result;
}

ActivationResult ExtensionActivationEventManager::NotifyViewShown(
    const std::string& viewId
) {
    if (viewId.empty()) {
        return ActivationResult::Error("View ID cannot be empty");
    }

    ActivationResult result;
    result.success = true;

    std::lock_guard<std::mutex> lock(m_registryLock);

    auto it = m_viewToExtensions.find(viewId);
    if (it != m_viewToExtensions.end()) {
        for (const auto& extId : it->second) {
            if (m_activatedExtensions.find(extId) == m_activatedExtensions.end()) {
                if (ActivateExtensionInternal(extId).success) {
                    result.activatedExtensions.push_back(extId);
                }
            }
        }
    }

    return result;
}

ActivationResult ExtensionActivationEventManager::NotifyWebviewCreated(
    const std::string& panelType
) {
    if (panelType.empty()) {
        return ActivationResult::Error("Panel type cannot be empty");
    }

    ActivationResult result;
    result.success = true;

    std::lock_guard<std::mutex> lock(m_registryLock);

    // Find extensions that should activate for this webview panel type
    auto it = m_webviewToExtensions.find(panelType);
    if (it != m_webviewToExtensions.end()) {
        for (const auto& extId : it->second) {
            if (m_activatedExtensions.find(extId) == m_activatedExtensions.end()) {
                if (ActivateExtensionInternal(extId).success) {
                    result.activatedExtensions.push_back(extId);
                }
            }
        }
    }

    return result;
}

ActivationResult ExtensionActivationEventManager::NotifyStartup() {
    ActivationResult result;
    result.success = true;

    std::lock_guard<std::mutex> lock(m_registryLock);

    // Activate all extensions with onStartup event
    for (auto& [extId, ext] : m_registry) {
        for (const auto& event : ext->activationEvents) {
            if (event.type == ActivationEventType::OnStartup) {
                if (m_activatedExtensions.find(extId) == m_activatedExtensions.end()) {
                    if (ActivateExtensionInternal(extId).success) {
                        result.activatedExtensions.push_back(extId);
                    }
                }
                break;
            }
        }
    }

    return result;
}

ActivationResult ExtensionActivationEventManager::NotifyDebugSessionStarted(
    const std::string& configuration
) {
    ActivationResult result;
    result.success = true;
    // Notify all extensions registered for onDebugSessionStarted
    std::lock_guard<std::mutex> lock(m_registryLock);
    auto it = m_eventHandlers.find("onDebugSessionStarted");
    if (it != m_eventHandlers.end()) {
        for (const auto& extId : it->second) {
            // In production: dispatch to extension host for actual notification
            (void)extId;
        }
    }
    result.activatedExtensions = GetActivatedExtensions();
    return result;
}

ActivationResult ExtensionActivationEventManager::NotifyFileSystemSchemeOpened(
    const std::string& scheme
) {
    ActivationResult result;
    result.success = true;
    // Notify all extensions registered for onFileSystemSchemeOpened
    std::lock_guard<std::mutex> lock(m_registryLock);
    auto it = m_eventHandlers.find("onFileSystemSchemeOpened");
    if (it != m_eventHandlers.end()) {
        for (const auto& extId : it->second) {
            // In production: dispatch to extension host for actual notification
            (void)extId;
        }
    }
    result.activatedExtensions = GetActivatedExtensions();
    return result;
}

bool ExtensionActivationEventManager::IsExtensionActivated(
    const std::string& extensionId
) const {
    std::lock_guard<std::mutex> lock(m_registryLock);
    return m_activatedExtensions.find(extensionId) != m_activatedExtensions.end();
}

std::vector<std::string> ExtensionActivationEventManager::GetActivatedExtensions() const {
    std::lock_guard<std::mutex> lock(m_registryLock);
    return std::vector<std::string>(m_activatedExtensions.begin(), m_activatedExtensions.end());
}

std::vector<std::string> ExtensionActivationEventManager::GetAvailableCommands() const {
    std::lock_guard<std::mutex> lock(m_registryLock);
    std::vector<std::string> commands;
    for (const auto& [cmd, _] : m_commandToExtensions) {
        commands.push_back(cmd);
    }
    return commands;
}

std::vector<std::string> ExtensionActivationEventManager::GetSupportedLanguages() const {
    std::lock_guard<std::mutex> lock(m_registryLock);
    std::vector<std::string> languages;
    for (const auto& [lang, _] : m_languageToExtensions) {
        languages.push_back(lang);
    }
    return languages;
}

ActivationResult ExtensionActivationEventManager::ActivateExtension(
    const std::string& extensionId
) {
    if (extensionId.empty()) {
        return ActivationResult::Error("Extension ID cannot be empty");
    }

    std::lock_guard<std::mutex> lock(m_registryLock);
    return ActivateExtensionInternal(extensionId);
}

ActivationResult ExtensionActivationEventManager::DeactivateExtension(
    const std::string& extensionId
) {
    if (extensionId.empty()) {
        return ActivationResult::Error("Extension ID cannot be empty");
    }

    std::lock_guard<std::mutex> lock(m_registryLock);

    auto it = m_registry.find(extensionId);
    if (it == m_registry.end()) {
        return ActivationResult::Error("Extension not registered: " + extensionId);
    }

    auto& ext = it->second;
    if (!ext->isActivated) {
        return ActivationResult::Error("Extension not activated: " + extensionId);
    }

    ext->isDeactivating = true;
    if (ext->onDeactivated) {
        ext->onDeactivated();
    }

    ext->isActivated = false;
    m_activatedExtensions.erase(extensionId);

    return ActivationResult::Ok();
}

ActivationResult ExtensionActivationEventManager::ActivateExtensionInternal(
    const std::string& extensionId
) {
    auto it = m_registry.find(extensionId);
    if (it == m_registry.end()) {
        return ActivationResult::Error("Extension not registered: " + extensionId);
    }

    auto& ext = it->second;
    if (ext->isActivated) {
        return ActivationResult::Ok();  // Already activated
    }

    ext->isActivated = true;
    m_activatedExtensions.insert(extensionId);

    if (ext->onActivated) {
        ext->onActivated();
    }

    return ActivationResult::Ok();
}

std::vector<std::string> ExtensionActivationEventManager::GetExtensionsForEvent(
    const ActivationEvent& event
) const {
    std::lock_guard<std::mutex> lock(m_registryLock);
    std::vector<std::string> result;

    switch (event.type) {
        case ActivationEventType::OnCommand: {
            auto it = m_commandToExtensions.find(event.trigger);
            if (it != m_commandToExtensions.end()) {
                result = it->second;
            }
            break;
        }
        case ActivationEventType::OnLanguage: {
            auto it = m_languageToExtensions.find(event.trigger);
            if (it != m_languageToExtensions.end()) {
                result = it->second;
            }
            break;
        }
        case ActivationEventType::OnView: {
            auto it = m_viewToExtensions.find(event.trigger);
            if (it != m_viewToExtensions.end()) {
                result = it->second;
            }
            break;
        }
        case ActivationEventType::OnUri:
        case ActivationEventType::OnWebviewPanel:
        case ActivationEventType::OnStartup:
        case ActivationEventType::OnDebug:
        case ActivationEventType::OnFileSystem:
            // For these event types, scan all registered extensions and match by trigger
            for (const auto& [extId, ext] : m_registry) {
                for (const auto& ev : ext->activationEvents) {
                    if (ev.type == event.type && MatchesTrigger(ev.trigger, event.trigger)) {
                        result.push_back(extId);
                        break;
                    }
                }
            }
            break;
    }

    // Remove duplicates while preserving order
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool ExtensionActivationEventManager::MatchesTrigger(
    const std::string& trigger,
    const std::string& value
) const {
    return MatchesWildcard(trigger, value);
}

std::string ExtensionActivationEventManager::NormalizeTrigger(
    const std::string& raw
) const {
    return ToLowerAscii(raw);
}

// ============================================================================
// Global Helper Functions
// ============================================================================

ActivationResult RegisterCommandExtension(const std::string& extensionId,
                                         const std::vector<std::string>& commands) {
    std::vector<ActivationEvent> events;
    for (const auto& cmd : commands) {
        events.emplace_back(ActivationEventType::OnCommand, cmd);
    }
    return GetActivationManager().RegisterExtension(extensionId, events);
}

ActivationResult RegisterLanguageExtension(const std::string& extensionId,
                                          const std::vector<std::string>& languages) {
    std::vector<ActivationEvent> events;
    for (const auto& lang : languages) {
        events.emplace_back(ActivationEventType::OnLanguage, lang);
    }
    return GetActivationManager().RegisterExtension(extensionId, events);
}

}  // namespace Extensions
}  // namespace RawrXD

// ============================================================================
// End of extension_activation_events.cpp
// ============================================================================
