// UEC-X VS Code Adapter
// Bridges VS Code extension host to UEC-X microkernel

#pragma once

#include "../protocol/jsonrpc_protocol.h"
#include <node.h>
#include <v8.h>

namespace uec {
namespace adapters {
namespace vscode {

// =============================================================================
// VS Code Extension Types
// =============================================================================

struct VSCodeCommand {
    std::string id;
    std::string title;
    std::string category;
    std::vector<std::string> keybindings;
    bool enablement;
};

struct VSCodeConfiguration {
    std::string id;
    std::string title;
    std::vector<std::pair<std::string, nlohmann::json>> properties;
};

// =============================================================================
// VS Code Bridge
// =============================================================================

class VSCodeBridge {
public:
    VSCodeBridge();
    ~VSCodeBridge();

    // Node.js/V8 integration
    static void Initialize(v8::Local<v8::Object> exports);
    static void Shutdown();
    
    // Command registration
    static void RegisterCommand(const std::string& id, 
                                v8::Local<v8::Function> callback);
    static void UnregisterCommand(const std::string& id);
    
    // Event handling
    static void SubscribeToEvent(const std::string& eventType,
                                   v8::Local<v8::Function> handler);
    static void UnsubscribeFromEvent(const std::string& eventType);
    
    // Configuration
    static nlohmann::json GetConfiguration(const std::string& section);
    static void UpdateConfiguration(const std::string& section,
                                    const nlohmann::json& value);
    
    // Status bar
    static void SetStatusBarMessage(const std::string& message, 
                                    int32_t timeoutMs = 0);
    static void HideStatusBarMessage();
    
    // Progress
    static void ShowProgress(const std::string& title,
                             std::function<void()> work);
    
    // Quick pick / Input
    static void ShowQuickPick(const std::vector<std::string>& items,
                              std::function<void(const std::string&)> callback);
    static void ShowInputBox(const std::string& prompt,
                             std::function<void(const std::string&)> callback);
    
    // File operations
    static void OpenFile(const std::string& path);
    static void SaveFile(const std::string& path);
    static std::string GetActiveEditorPath();
    static std::string GetActiveEditorContent();
    static void SetActiveEditorContent(const std::string& content);
    
    // Terminal
    static void CreateTerminal(const std::string& name);
    static void SendTerminalCommand(const std::string& command);
    
    // Webview
    static void CreateWebview(const std::string& id,
                               const std::string& title,
                               const std::string& content);
    static void PostWebviewMessage(const std::string& id,
                                   const nlohmann::json& message);

private:
    static std::unique_ptr<protocol::ProtocolHandler> s_protocolHandler;
    static std::unique_ptr<protocol::Transport> s_transport;
    static std::unordered_map<std::string, v8::Persistent<v8::Function>> s_commandHandlers;
    static std::unordered_map<std::string, v8::Persistent<v8::Function>> s_eventHandlers;
    static std::atomic<bool> s_initialized;
};

// =============================================================================
// Node.js Module Exports
// =============================================================================

void InitializeModule(v8::Local<v8::Object> exports);

} // namespace vscode
} // namespace adapters
} // namespace uec
