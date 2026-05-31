#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <functional>

// Forward declarations for WebView2
struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;

namespace RawrXD::UI {

using WebMessageCallback = std::function<void(const std::string& message)>;

class WebView2Bridge {
public:
    explicit WebView2Bridge(HWND parentHwnd);
    ~WebView2Bridge();
    
    bool initialize();
    void shutdown();
    
    // Navigation
    void navigate(const std::string& url);
    void navigateToString(const std::string& html);
    void reload();
    void goBack();
    void goForward();
    
    // Script execution
    void executeScript(const std::string& script);
    void executeScriptWithCallback(const std::string& script, std::function<void(const std::string& result)> callback);
    
    // Message handling
    void postMessage(const std::string& message);
    void setWebMessageHandler(WebMessageCallback handler);
    
    // Settings
    void setDevToolsEnabled(bool enabled);
    void setZoomFactor(double factor);
    
    // Size/position
    void resize(const RECT& bounds);
    void focus();
    
    bool isInitialized() const { return m_initialized; }

private:
    HWND m_parentHwnd = nullptr;
    ICoreWebView2* m_webview = nullptr;
    ICoreWebView2Controller* m_controller = nullptr;
    ICoreWebView2Environment* m_env = nullptr;
    bool m_initialized = false;
    WebMessageCallback m_messageHandler;
    
    void cleanup();
};

} // namespace RawrXD::UI
