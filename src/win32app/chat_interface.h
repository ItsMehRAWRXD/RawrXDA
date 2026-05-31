// ============================================================================
// chat_interface.h — Chat Panel Interface for Win32IDE
// ============================================================================
// Defines IChatPanel, the contract between ChatPanelModelCaller and the UI.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>

namespace RawrXD {

// Forward declarations
struct ContextFrame;
struct ContextEvent;

// ---------------------------------------------------------------------------
// IChatPanel — Abstract chat panel surface
// ---------------------------------------------------------------------------
class IChatPanel {
public:
    virtual ~IChatPanel() = default;

    // Message display
    virtual void AppendMessage(const std::string& role, const std::string& text) = 0;
    virtual void AppendStreamingToken(const std::string& token) = 0;
    virtual void AppendAIMessage(const std::string& text) = 0;
    virtual void AppendSystemMessage(const std::string& text) = 0;
    virtual void FlushStreaming() = 0;

    // Status / progress
    virtual void SetStatus(const std::string& status) = 0;
    virtual void SetProgress(int percent) = 0;
    virtual void ShowTypingIndicator(bool show) = 0;

    // Context
    virtual void SetContextFile(const std::string& path) = 0;
    virtual void SetContextLanguage(const std::string& lang) = 0;

    // Model caller integration
    virtual void SetModelCaller(void* caller) = 0;

    // Input
    virtual std::string GetUserInput() = 0;
    virtual void ClearInput() = 0;

    // Lifecycle
    virtual bool IsVisible() const = 0;
    virtual void Focus() = 0;
};

} // namespace RawrXD
