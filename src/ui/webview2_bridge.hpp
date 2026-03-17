#pragma once
#include <windows.h>
#include <unknwn.h>
#include <objbase.h>
#include <string>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include "rawrxd_com_min.h"
#include <WebView2.h>
#include "rawrxd_ipc_protocol.h"

namespace rawrxd::ui {

class WebView2Bridge {
public:
    static WebView2Bridge& getInstance() {
        static WebView2Bridge instance;
        return instance;
    }

    // Initialize WebView2 with the given HWND
    bool initialize(HWND hwnd);

    // Shutdown WebView2
    void shutdown();

    // High-performance binary IPC push
    void sendBinaryMessage(ipc::MessageType type, const void* data, size_t len);

    // Send a message to the UI (JavaScript string)
    void postMessage(const std::string& message);

    // Register a callback for messages from the UI
    void onMessageFromUI(std::function<void(const std::string&)> callback);

    // Initial listener for binary messages
    void onBinaryMessageFromUI(const uint8_t* buffer, size_t length);

    // Snapshot current modules via PEB and stream to UI
    void snapshotModules();

private:
    WebView2Bridge() = default;
    ~WebView2Bridge() = default;
    WebView2Bridge(const WebView2Bridge&) = delete;
    WebView2Bridge& operator=(const WebView2Bridge&) = delete;

    HWND m_hwnd = nullptr;
    ICoreWebView2Controller* m_controller = nullptr;
    ICoreWebView2* m_webview = nullptr;
    HMODULE m_webview2Loader = nullptr;
    std::function<void(const std::string&)> m_messageHandler;
    uint32_t m_sequence = 0;
    bool m_webview2Ready = false;

    // GDI fallback: render a minimal status window when WebView2 is unavailable
    void initGdiFallback(HWND hwnd);
};

} // namespace rawrxd::ui
