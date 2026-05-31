#pragma once
// BrowserSurface.h — Agentic browser-in-the-loop surface
// Wraps WebView2Bridge with agent-facing navigation, scripting, and content
// extraction. No Qt. No exceptions.

#include <windows.h>
#include <string>
#include <functional>

namespace RawrXD {

// Callback fired when browser content or script result is ready.
using BrowserResultFn = std::function<void(const std::string& resultJson)>;

class BrowserSurface {
public:
    static BrowserSurface& instance();

    // Attach to a Win32 HWND panel (called once during IDE init).
    bool attach(HWND hwndParent, int x, int y, int w, int h);

    // Detach and release WebView2 resources.
    void detach();

    // Navigate to a URL. Fires resultCallback with {"event":"navigated","url":...}.
    void navigate(const std::string& url, BrowserResultFn resultCallback = nullptr);

    // Execute JavaScript in current page and return result as JSON string.
    // resultCallback receives {"result": <value>} or {"error": <msg>}.
    void executeScript(const std::string& js, BrowserResultFn resultCallback = nullptr);

    // Extract full page text content as plain text.
    // resultCallback receives {"content": <text>}.
    void getPageContent(BrowserResultFn resultCallback = nullptr);

    // Resize the embedded browser pane.
    void resize(int x, int y, int w, int h);

    bool isAttached() const { return m_attached; }

private:
    BrowserSurface() = default;
    ~BrowserSurface() = default;
    BrowserSurface(const BrowserSurface&) = delete;
    BrowserSurface& operator=(const BrowserSurface&) = delete;

    bool m_attached = false;
    HWND m_hwndParent = nullptr;
    BrowserResultFn m_pendingNavCallback;
    BrowserResultFn m_pendingScriptCallback;
};

} // namespace RawrXD
