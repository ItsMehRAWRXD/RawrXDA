// BrowserSurface.cpp — Agentic browser-in-the-loop surface
// Delegates to WebView2Bridge for actual rendering; adds agent-facing
// navigate/execute/extract API. No Qt. No exceptions.

#include "BrowserSurface.h"
#include "../ui/webview2_bridge.hpp"
#include <sstream>

// Simple JSON string escaping — avoids pulling in nlohmann for one call
static std::string JsonEscapeString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    out += esc;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

namespace RawrXD {

BrowserSurface& BrowserSurface::instance() {
    static BrowserSurface s_instance;
    return s_instance;
}

bool BrowserSurface::attach(HWND hwndParent, int x, int y, int w, int h) {
    if (m_attached) return true;
    if (!hwndParent) return false;

    m_hwndParent = hwndParent;

    // Initialize WebView2Bridge on this HWND.
    auto& bridge = rawrxd::ui::WebView2Bridge::getInstance();
    if (!bridge.initialize(hwndParent)) {
        return false;
    }

    // Register incoming UI messages (e.g., user clicks links, form submits).
    bridge.onMessageFromUI([](const std::string& msg) {
        // Forward UI-initiated messages to the output dispatcher if needed.
        OutputDebugStringA(("[BrowserSurface] UI message: " + msg + "\n").c_str());
    });

    m_attached = true;
    resize(x, y, w, h);
    return true;
}

void BrowserSurface::detach() {
    if (!m_attached) return;
    rawrxd::ui::WebView2Bridge::getInstance().shutdown();
    m_attached = false;
    m_hwndParent = nullptr;
    m_pendingNavCallback = nullptr;
    m_pendingScriptCallback = nullptr;
}

void BrowserSurface::navigate(const std::string& url, BrowserResultFn resultCallback) {
    if (!m_attached) {
        if (resultCallback) {
            resultCallback("{\"error\":\"BrowserSurface not attached\"}");
        }
        return;
    }
    m_pendingNavCallback = resultCallback;
    // Instruct WebView2Bridge to navigate; use postMessage as the trigger.
    std::string navCmd = "{\"cmd\":\"navigate\",\"url\":\"" + url + "\"}";
    rawrxd::ui::WebView2Bridge::getInstance().postMessage(navCmd);
    // Note: The completion fires asynchronously via onMessageFromUI when
    // the page raises 'NavigationCompleted'. For now we fire the callback
    // synchronously to maintain the agent loop cadence until async wiring
    // is plumbed through the bridge's DOMContentLoaded event.
    if (resultCallback) {
        std::string r = "{\"event\":\"navigated\",\"url\":\"" + url + "\"}";
        resultCallback(r);
    }
}

void BrowserSurface::executeScript(const std::string& js, BrowserResultFn resultCallback) {
    if (!m_attached) {
        if (resultCallback) {
            resultCallback("{\"error\":\"BrowserSurface not attached\"}");
        }
        return;
    }
    m_pendingScriptCallback = resultCallback;
    std::string scriptCmd = "{\"cmd\":\"executeScript\",\"js\":" +
                            JsonEscapeString(js) + "}";
    rawrxd::ui::WebView2Bridge::getInstance().postMessage(scriptCmd);
    // Async result arrives via onMessageFromUI; forward it through callback.
    // Until full async wiring is complete, acknowledge dispatch only.
    if (resultCallback) {
        resultCallback("{\"result\":\"dispatched\"}");
    }
}

void BrowserSurface::getPageContent(BrowserResultFn resultCallback) {
    // Extract text via JavaScript injection.
    const std::string extractJs =
        "JSON.stringify({content: document.body ? document.body.innerText : ''})";
    executeScript(extractJs, resultCallback);
}

void BrowserSurface::resize(int x, int y, int w, int h) {
    if (!m_attached || !m_hwndParent) return;
    // WebView2Bridge controller bounds are set on its internal ICoreWebView2Controller.
    // Route through postMessage resize command so the bridge can call
    // put_Bounds() on the controller.
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"cmd\":\"resize\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
             x, y, w, h);
    rawrxd::ui::WebView2Bridge::getInstance().postMessage(buf);
}

} // namespace RawrXD
