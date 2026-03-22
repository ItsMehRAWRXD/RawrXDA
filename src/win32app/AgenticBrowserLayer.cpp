// AgenticBrowserLayer.cpp — WebView2 host for agentic browsing (separate from Monaco WebView2).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>

#include "rawrxd/ide/AgenticBrowserLayer.hpp"

#include "IDELogger.h"
#include "WebView2.h"

#include <atomic>
#include <cstring>
#include <string>

#pragma comment(lib, "Shlwapi.lib")

namespace RawrXD::Ide {
namespace {

std::wstring widenUtf8(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w;
    w.resize(static_cast<size_t>(n));
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrowWide(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string out;
    out.resize(static_cast<size_t>(n));
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring userDataPathFor(const std::wstring& sub) {
    wchar_t localAppData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData))) {
        return L".\\RawrXDWebView2Data\\" + sub;
    }
    std::wstring base = std::wstring(localAppData) + L"\\RawrXD\\" + sub;
    CreateDirectoryW((std::wstring(localAppData) + L"\\RawrXD").c_str(), nullptr);
    CreateDirectoryW(base.c_str(), nullptr);
    return base;
}

HMODULE loadWebView2Loader() {
    const char* paths[] = {"WebView2Loader.dll", ".\\WebView2Loader.dll", "bin\\WebView2Loader.dll", nullptr};
    for (int i = 0; paths[i]; ++i) {
        if (HMODULE h = LoadLibraryA(paths[i])) {
            return h;
        }
    }
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecA(exePath);
    std::string loaderPath = std::string(exePath) + "\\WebView2Loader.dll";
    return LoadLibraryA(loaderPath.c_str());
}

template <typename T>
class AbCallbackBase : public T {
public:
    AbCallbackBase() : m_ref(1) {}
    virtual ~AbCallbackBase() = default;

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = InterlockedDecrement(&m_ref);
        if (c == 0) {
            delete this;
        }
        return c;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(T)) {
            *ppv = static_cast<T*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

private:
    ULONG m_ref;
};

class AbEnvHandler : public AbCallbackBase<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    explicit AbEnvHandler(AgenticBrowserLayer* layer) : m_layer(layer) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
        if (m_layer) {
            m_layer->onEnvironmentCreated(errorCode, env);
        }
        return S_OK;
    }

private:
    AgenticBrowserLayer* m_layer;
};

class AbControllerHandler : public AbCallbackBase<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    explicit AbControllerHandler(AgenticBrowserLayer* layer) : m_layer(layer) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* ctrl) override {
        if (m_layer) {
            m_layer->onControllerCreated(errorCode, ctrl);
        }
        return S_OK;
    }

private:
    AgenticBrowserLayer* m_layer;
};

class AbWebMessageHandler : public AbCallbackBase<ICoreWebView2WebMessageReceivedEventHandler> {
public:
    explicit AbWebMessageHandler(AgenticBrowserLayer* layer) : m_layer(layer) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (!m_layer || !args) {
            return S_OK;
        }
        LPWSTR raw = nullptr;
        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
            m_layer->onWebMessageReceived(std::wstring(raw));
            CoTaskMemFree(raw);
            return S_OK;
        }
        if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
            m_layer->onWebMessageReceived(std::wstring(raw));
            CoTaskMemFree(raw);
        }
        return S_OK;
    }

private:
    AgenticBrowserLayer* m_layer;
};

class AbNavHandler : public AbCallbackBase<ICoreWebView2NavigationCompletedEventHandler> {
public:
    explicit AbNavHandler(AgenticBrowserLayer* layer) : m_layer(layer) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) override {
        BOOL ok = FALSE;
        if (args) {
            args->get_IsSuccess(&ok);
        }
        if (m_layer) {
            m_layer->onNavigationCompleted(ok != FALSE);
        }
        return S_OK;
    }

private:
    AgenticBrowserLayer* m_layer;
};

} // namespace

AgenticBrowserLayer::AgenticBrowserLayer() = default;

AgenticBrowserLayer::~AgenticBrowserLayer() {
    destroy();
}

bool AgenticBrowserLayer::create(HWND parentHwnd, const RECT& initialBounds, const AgenticBrowserConfig& cfg) {
    if (!parentHwnd || m_loader || m_environment || m_webview) {
        return false;
    }
    m_parentHwnd = parentHwnd;
    m_bounds = initialBounds;

    m_loader = loadWebView2Loader();
    if (!m_loader) {
        RAWRXD_LOG_ERROR("AgenticBrowser") << "WebView2Loader.dll not found";
        return false;
    }

    auto createEnv = reinterpret_cast<CreateCoreWebView2EnvironmentWithOptionsFunc>(
        GetProcAddress(m_loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!createEnv) {
        RAWRXD_LOG_ERROR("AgenticBrowser") << "CreateCoreWebView2EnvironmentWithOptions missing";
        FreeLibrary(m_loader);
        m_loader = nullptr;
        return false;
    }

    m_userDataFolder = userDataPathFor(cfg.userDataSubfolder);

    auto* handler = new AbEnvHandler(this);
    const HRESULT hr =
        createEnv(nullptr, m_userDataFolder.c_str(), nullptr, handler);
    if (FAILED(hr)) {
        handler->Release();
        FreeLibrary(m_loader);
        m_loader = nullptr;
        RAWRXD_LOG_ERROR("AgenticBrowser") << "CreateCoreWebView2EnvironmentWithOptions failed hr=" << (int)hr;
        return false;
    }
    return true;
}

void AgenticBrowserLayer::destroy() {
    m_destroying.store(true, std::memory_order_release);
    m_ready.store(false, std::memory_order_release);

    if (m_controller) {
        m_controller->Close();
        m_controller->Release();
        m_controller = nullptr;
    }
    if (m_webview) {
        m_webview->Release();
        m_webview = nullptr;
    }
    if (m_environment) {
        m_environment->Release();
        m_environment = nullptr;
    }
    if (m_loader) {
        FreeLibrary(m_loader);
        m_loader = nullptr;
    }
    m_parentHwnd = nullptr;
    m_destroying.store(false, std::memory_order_release);
}

void AgenticBrowserLayer::setBounds(const RECT& clientRect) {
    m_bounds = clientRect;
    if (m_controller) {
        RECT b = {0, 0, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top};
        if (b.right <= 0) {
            b.right = 400;
        }
        if (b.bottom <= 0) {
            b.bottom = 300;
        }
        m_controller->put_Bounds(b);
    }
}

void AgenticBrowserLayer::show() {
    if (m_controller) {
        m_controller->put_IsVisible(TRUE);
    }
}

void AgenticBrowserLayer::hide() {
    if (m_controller) {
        m_controller->put_IsVisible(FALSE);
    }
}

bool AgenticBrowserLayer::navigate(const std::wstring& uri) {
    if (!m_webview || uri.empty()) {
        return false;
    }
    return SUCCEEDED(m_webview->Navigate(uri.c_str()));
}

bool AgenticBrowserLayer::navigateToHtmlUtf8(const std::string& utf8Html) {
    if (!m_webview) {
        return false;
    }
    const std::wstring w = widenUtf8(utf8Html);
    return SUCCEEDED(m_webview->NavigateToString(w.c_str()));
}

bool AgenticBrowserLayer::executeScriptUtf8(const std::string& utf8Js) {
    if (!m_webview || !m_ready.load(std::memory_order_acquire)) {
        return false;
    }
    const std::wstring w = widenUtf8(utf8Js);
    return SUCCEEDED(m_webview->ExecuteScript(w.c_str(), nullptr));
}

bool AgenticBrowserLayer::postWebMessageAsJsonUtf8(const std::string& utf8Json) {
    if (!m_webview || !m_ready.load(std::memory_order_acquire)) {
        return false;
    }
    const std::wstring w = widenUtf8(utf8Json);
    return SUCCEEDED(m_webview->PostWebMessageAsJson(w.c_str()));
}

void AgenticBrowserLayer::setPageMessageHandler(PageMessageHandler fn, void* user) {
    m_pageMsg = fn;
    m_pageMsgUser = user;
}

void AgenticBrowserLayer::setNavigationHandler(NavigationHandler fn, void* user) {
    m_nav = fn;
    m_navUser = user;
}

void AgenticBrowserLayer::onEnvironmentCreated(HRESULT hr, ICoreWebView2Environment* env) {
    if (m_destroying.load(std::memory_order_acquire)) {
        return;
    }
    if (FAILED(hr) || !env || !m_parentHwnd) {
        RAWRXD_LOG_ERROR("AgenticBrowser") << "Environment create failed";
        return;
    }
    m_environment = env;
    m_environment->AddRef();

    auto* h = new AbControllerHandler(this);
    const HRESULT hr2 = m_environment->CreateCoreWebView2Controller(m_parentHwnd, h);
    if (FAILED(hr2)) {
        h->Release();
        RAWRXD_LOG_ERROR("AgenticBrowser") << "CreateCoreWebView2Controller failed";
    }
}

void AgenticBrowserLayer::onControllerCreated(HRESULT hr, ICoreWebView2Controller* ctrl) {
    if (m_destroying.load(std::memory_order_acquire)) {
        return;
    }
    if (FAILED(hr) || !ctrl) {
        RAWRXD_LOG_ERROR("AgenticBrowser") << "Controller create failed";
        return;
    }
    m_controller = ctrl;
    m_controller->AddRef();

    ICoreWebView2* webview = nullptr;
    if (FAILED(m_controller->get_CoreWebView2(&webview)) || !webview) {
        RAWRXD_LOG_ERROR("AgenticBrowser") << "get_CoreWebView2 failed";
        return;
    }
    m_webview = webview;

    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(m_webview->get_Settings(&settings)) && settings) {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);
        settings->Release();
    }

    setBounds(m_bounds);

    EventRegistrationToken t1{};
    m_webview->add_WebMessageReceived(new AbWebMessageHandler(this), &t1);
    EventRegistrationToken t2{};
    m_webview->add_NavigationCompleted(new AbNavHandler(this), &t2);

    navigateShell();
}

void AgenticBrowserLayer::onWebMessageReceived(const std::wstring& jsonOrString) {
    if (m_pageMsg) {
        const std::string u8 = narrowWide(jsonOrString);
        m_pageMsg(u8.c_str(), m_pageMsgUser);
    }
}

void AgenticBrowserLayer::onNavigationCompleted(bool success) {
    if (m_nav) {
        m_nav(success, m_navUser);
    }
    if (success) {
        m_ready.store(true, std::memory_order_release);
    }
}

void AgenticBrowserLayer::navigateShell() {
    // Minimal shell: host bridge for agents (postMessage JSON).
    const char* html = R"(<!DOCTYPE html><html><head><meta charset="utf-8"/><title>RawrXD Agentic Browser</title>
<style>body{font-family:system-ui,Segoe UI,sans-serif;background:#1a1625;color:#e0def4;margin:16px;}
code{color:#9ccfd8}</style></head><body>
<h1>Agentic Browser</h1><p>Layer ready. Use <code>chrome.webview.postMessage</code> from script or IDE <code>postWebMessageAsJsonUtf8</code>.</p>
<script>
try {
  chrome.webview.postMessage(JSON.stringify({type:"agenticBrowserShellReady",v:1}));
} catch(e) {}
</script></body></html>)";
    (void)navigateToHtmlUtf8(html);
}

} // namespace RawrXD::Ide
