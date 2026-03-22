#pragma once

// AgenticBrowserLayer — embedded Chromium (WebView2) for agent-driven browsing inside RawrXD.
// Separate from the Monaco editor WebView: use this for URLs, DOM automation, and host↔page JSON.

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "WebView2.h"

namespace RawrXD::Ide {

struct AgenticBrowserConfig {
    /// Subfolder under %LOCALAPPDATA%\RawrXD\ for WebView2 profile isolation.
    std::wstring userDataSubfolder = L"AgenticBrowser";
};

class AgenticBrowserLayer {
public:
    AgenticBrowserLayer();
    ~AgenticBrowserLayer();

    AgenticBrowserLayer(const AgenticBrowserLayer&) = delete;
    AgenticBrowserLayer& operator=(const AgenticBrowserLayer&) = delete;

    /// Start async WebView2 creation. `parentHwnd` is the host child window (client area for bounds).
    [[nodiscard]] bool create(HWND parentHwnd, const RECT& initialBounds, const AgenticBrowserConfig& cfg = {});

    void destroy();

    [[nodiscard]] bool isReady() const { return m_ready.load(std::memory_order_acquire); }

    void setBounds(const RECT& clientRect);
    void show();
    void hide();

    [[nodiscard]] bool navigate(const std::wstring& uri);
    [[nodiscard]] bool navigateToHtmlUtf8(const std::string& utf8Html);

    /// Run script in the current document (UTF-8). Returns false if webview not ready.
    [[nodiscard]] bool executeScriptUtf8(const std::string& utf8Js);

    /// Host → page via PostWebMessageAsJson (UTF-8 JSON text).
    [[nodiscard]] bool postWebMessageAsJsonUtf8(const std::string& utf8Json);

    using PageMessageHandler = void (*)(const char* utf8Json, void* user);
    void setPageMessageHandler(PageMessageHandler fn, void* user);

    using NavigationHandler = void (*)(bool success, void* user);
    void setNavigationHandler(NavigationHandler fn, void* user);

    /// Access underlying parent HWND (host).
    [[nodiscard]] HWND parentHwnd() const { return m_parentHwnd; }

    // --- WebView2 async entrypoints (invoked from COM handlers; not for app use) ---
    void onEnvironmentCreated(HRESULT hr, ::ICoreWebView2Environment* env);
    void onControllerCreated(HRESULT hr, ::ICoreWebView2Controller* ctrl);
    void onWebMessageReceived(const std::wstring& jsonOrString);
    void onNavigationCompleted(bool success);

private:
    void navigateShell();

    HWND m_parentHwnd = nullptr;
    RECT m_bounds{};
    HMODULE m_loader = nullptr;
    ::ICoreWebView2Environment* m_environment = nullptr;
    ::ICoreWebView2Controller* m_controller = nullptr;
    ::ICoreWebView2* m_webview = nullptr;

    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_destroying{false};

    PageMessageHandler m_pageMsg = nullptr;
    void* m_pageMsgUser = nullptr;
    NavigationHandler m_nav = nullptr;
    void* m_navUser = nullptr;

    std::wstring m_userDataFolder;
};

} // namespace RawrXD::Ide
