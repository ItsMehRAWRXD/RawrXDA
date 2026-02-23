// ============================================================================
// Win32IDE_WebView2.cpp — WebView2 + Monaco Editor Integration Implementation
// ============================================================================
//
// Phase 26: WebView2 Integration — Feature #206
//
// This file implements:
//   1. WebView2Container — COM lifecycle, dynamic loader, message bridge
//   2. Monaco HTML generation — Self-contained HTML with embedded Monaco CDN
//   3. Two-way C++↔JS message protocol via PostWebMessageAsJson
//   4. Fallback behavior when WebView2 Runtime is not available
//
// COM Threading: All WebView2 calls must happen on the STA (UI) thread.
// The Win32IDE message pump (GetMessage/DispatchMessage) satisfies this.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE_WebView2.h"
#include "Win32IDE.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include "WebView2.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cwchar>

// SCAFFOLD_041: WebView2 and model selector


// ============================================================================
// COM Helpers — Reference-counted callback implementations
// ============================================================================

// Generic ref-counted COM base for our callback handlers
// NOTE: We avoid __uuidof(T) here because MinGW doesn't have
// __mingw_uuidof specializations for our custom WebView2 interfaces.
// Instead, we always succeed QueryInterface — WebView2 only calls us
// through the typed pointer it already has, so this is safe.
template <typename T>
class WebView2CallbackBase : public T {
public:
    WebView2CallbackBase(WebView2Container* container) : m_refCount(1), m_container(container) {}
    virtual ~WebView2CallbackBase() = default;

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown) {
            *ppv = static_cast<T*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

protected:
    ULONG m_refCount;
    WebView2Container* m_container;
};

// ============================================================================
// Environment Created Handler
// ============================================================================
class EnvironmentCreatedHandler
    : public WebView2CallbackBase<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    using WebView2CallbackBase::WebView2CallbackBase;

    HRESULT STDMETHODCALLTYPE Invoke(
        HRESULT errorCode,
        ICoreWebView2Environment* createdEnvironment) override
    {
        m_container->onEnvironmentCreated(errorCode, createdEnvironment);
        return S_OK;
    }
};

// ============================================================================
// Controller Created Handler
// ============================================================================
class ControllerCreatedHandler
    : public WebView2CallbackBase<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    using WebView2CallbackBase::WebView2CallbackBase;

    HRESULT STDMETHODCALLTYPE Invoke(
        HRESULT errorCode,
        ICoreWebView2Controller* createdController) override
    {
        m_container->onControllerCreated(errorCode, createdController);
        return S_OK;
    }
};

// ============================================================================
// Web Message Received Handler
// ============================================================================
class WebMessageReceivedHandler
    : public WebView2CallbackBase<ICoreWebView2WebMessageReceivedEventHandler> {
public:
    using WebView2CallbackBase::WebView2CallbackBase;

    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2* sender,
        ICoreWebView2WebMessageReceivedEventArgs* args) override
    {
        LPWSTR messageRaw = nullptr;
        HRESULT hr = args->TryGetWebMessageAsString(&messageRaw);
        if (SUCCEEDED(hr) && messageRaw) {
            m_container->onWebMessageReceived(std::wstring(messageRaw));
            CoTaskMemFree(messageRaw);
        } else {
            // Try JSON variant
            hr = args->get_WebMessageAsJson(&messageRaw);
            if (SUCCEEDED(hr) && messageRaw) {
                m_container->onWebMessageReceived(std::wstring(messageRaw));
                CoTaskMemFree(messageRaw);
            }
        }
        return S_OK;
    }
};

// ============================================================================
// Navigation Completed Handler
// ============================================================================
class NavigationCompletedHandler
    : public WebView2CallbackBase<ICoreWebView2NavigationCompletedEventHandler> {
public:
    using WebView2CallbackBase::WebView2CallbackBase;

    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2* sender,
        ICoreWebView2NavigationCompletedEventArgs* args) override
    {
        BOOL isSuccess = FALSE;
        args->get_IsSuccess(&isSuccess);
        m_container->onNavigationCompleted(isSuccess != FALSE);
        return S_OK;
    }
};

// ============================================================================
// WebView2Container — Constructor / Destructor
// ============================================================================

WebView2Container::WebView2Container() = default;

WebView2Container::~WebView2Container() {
    if (m_state.load() != WebView2State::Destroyed &&
        m_state.load() != WebView2State::NotInitialized) {
        destroy();
    }
}

// ============================================================================
// State String
// ============================================================================
const char* WebView2Container::getStateString() const {
    switch (m_state.load()) {
        case WebView2State::NotInitialized:      return "NotInitialized";
        case WebView2State::LoaderFound:         return "LoaderFound";
        case WebView2State::EnvironmentCreating: return "EnvironmentCreating";
        case WebView2State::EnvironmentReady:    return "EnvironmentReady";
        case WebView2State::ControllerCreating:  return "ControllerCreating";
        case WebView2State::ControllerReady:     return "ControllerReady";
        case WebView2State::MonacoLoading:       return "MonacoLoading";
        case WebView2State::MonacoReady:         return "MonacoReady";
        case WebView2State::Error:               return "Error";
        case WebView2State::Destroyed:           return "Destroyed";
    }
    return "Unknown";
}

// ============================================================================
// Initialize — Load WebView2Loader.dll and begin async creation
// ============================================================================
WebView2Result WebView2Container::initialize(HWND parentWindow, const std::string& userDataFolder) {
    if (m_state.load() != WebView2State::NotInitialized) {
        return WebView2Result::error("Already initialized or in error state", -1);
    }

    m_parentWindow = parentWindow;

    // Attempt to load WebView2Loader.dll from multiple paths
    const char* loaderPaths[] = {
        "WebView2Loader.dll",
        ".\\WebView2Loader.dll",
        "bin\\WebView2Loader.dll",
        nullptr
    };

    for (int i = 0; loaderPaths[i]; i++) {
        m_hLoaderDll = LoadLibraryA(loaderPaths[i]);
        if (m_hLoaderDll) break;
    }

    // If not found locally, try the system path (Edge WebView2 SDK distributable)
    if (!m_hLoaderDll) {
        // Try alongside the executable
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        PathRemoveFileSpecA(exePath);
        std::string loaderPath = std::string(exePath) + "\\WebView2Loader.dll";
        m_hLoaderDll = LoadLibraryA(loaderPath.c_str());
    }

    if (!m_hLoaderDll) {
        m_state = WebView2State::Error;
        return WebView2Result::error("WebView2Loader.dll not found. Install the WebView2 SDK redistributable.", -2);
    }

    m_state = WebView2State::LoaderFound;

    // Get the creation function
    auto CreateEnvironmentFunc = reinterpret_cast<CreateCoreWebView2EnvironmentWithOptionsFunc>(
        GetProcAddress(m_hLoaderDll, "CreateCoreWebView2EnvironmentWithOptions")
    );

    if (!CreateEnvironmentFunc) {
        FreeLibrary(m_hLoaderDll);
        m_hLoaderDll = nullptr;
        m_state = WebView2State::Error;
        return WebView2Result::error("CreateCoreWebView2EnvironmentWithOptions not found in loader DLL", -3);
    }

    // Determine user data folder
    std::wstring wUserDataFolder;
    if (!userDataFolder.empty()) {
        wUserDataFolder = utf8ToWide(userDataFolder);
    } else {
        // Default: %LOCALAPPDATA%\RawrXD\WebView2Data
        wchar_t localAppData[MAX_PATH] = {};
        SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData);
        wUserDataFolder = std::wstring(localAppData) + L"\\RawrXD\\WebView2Data";
    }

    m_state = WebView2State::EnvironmentCreating;

    // Create the environment (async — calls back to onEnvironmentCreated)
    auto* envHandler = new EnvironmentCreatedHandler(this);
    HRESULT hr = CreateEnvironmentFunc(
        nullptr,                    // browserExecutableFolder (null = auto-detect)
        wUserDataFolder.c_str(),    // userDataFolder
        nullptr,                    // environmentOptions
        envHandler
    );

    if (FAILED(hr)) {
        envHandler->Release();
        m_state = WebView2State::Error;
        return WebView2Result::error("CreateCoreWebView2EnvironmentWithOptions failed", (int)hr);
    }

    return WebView2Result::ok("WebView2 environment creation started (async)");
}

// ============================================================================
// Destroy — Release all COM objects
// ============================================================================
WebView2Result WebView2Container::destroy() {
    std::lock_guard<std::mutex> lock(m_mutex);

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

    if (m_hLoaderDll) {
        FreeLibrary(m_hLoaderDll);
        m_hLoaderDll = nullptr;
    }

    m_state = WebView2State::Destroyed;
    return WebView2Result::ok("WebView2 destroyed");
}

// ============================================================================
// Geometry
// ============================================================================
void WebView2Container::resize(int x, int y, int width, int height) {
    m_bounds = { x, y, x + width, y + height };
    if (m_controller) {
        RECT bounds = { 0, 0, width, height };
        m_controller->put_Bounds(bounds);
    }
}

void WebView2Container::show() {
    m_visible = true;
    if (m_controller) {
        m_controller->put_IsVisible(TRUE);
    }
}

void WebView2Container::hide() {
    m_visible = false;
    if (m_controller) {
        m_controller->put_IsVisible(FALSE);
    }
}

// ============================================================================
// Monaco API — Content
// ============================================================================
WebView2Result WebView2Container::setContent(const std::string& content, const std::string& language) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);

    // Escape the content for JSON embedding
    std::string escaped;
    escaped.reserve(content.size() + content.size() / 4);
    for (char c : content) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\u%04x", (unsigned char)c);
                    escaped += hex;
                } else {
                    escaped += c;
                }
                break;
        }
    }

    std::wstring msg = utf8ToWide(
        "{\"type\":\"setContent\",\"payload\":{\"content\":\"" + escaped +
        "\",\"language\":\"" + language + "\"}}"
    );

    m_stats.contentSets.fetch_add(1, std::memory_order_relaxed);
    return postMessage(msg);
}

WebView2Result WebView2Container::getContent() {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    m_stats.contentGets.fetch_add(1, std::memory_order_relaxed);
    return postMessage(utf8ToWide("{\"type\":\"getContent\"}"));
}

// ============================================================================
// Monaco API — Theme
// ============================================================================
WebView2Result WebView2Container::setTheme(const std::string& themeName) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    m_stats.themeChanges.fetch_add(1, std::memory_order_relaxed);
    return postMessage(utf8ToWide(
        "{\"type\":\"setTheme\",\"payload\":{\"theme\":\"" + themeName + "\"}}"
    ));
}

WebView2Result WebView2Container::defineTheme(const MonacoThemeDef& theme) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);

    // Build the defineTheme JSON
    std::ostringstream oss;
    oss << "{\"type\":\"defineTheme\",\"payload\":{";
    oss << "\"name\":\"" << theme.name << "\",";
    oss << "\"base\":\"" << theme.base << "\",";

    // Token rules
    oss << "\"rules\":[";
    for (size_t i = 0; i < theme.rules.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"token\":\"" << theme.rules[i].token << "\"";
        oss << ",\"foreground\":\"" << theme.rules[i].foreground << "\"";
        if (!theme.rules[i].fontStyle.empty()) {
            oss << ",\"fontStyle\":\"" << theme.rules[i].fontStyle << "\"";
        }
        oss << "}";
    }
    oss << "],";

    // Colors
    oss << "\"colors\":{";
    bool first = true;
    for (const auto& [key, value] : theme.colors) {
        if (!first) oss << ",";
        oss << "\"" << key << "\":\"" << value << "\"";
        first = false;
    }
    oss << "}";

    oss << "}}";

    m_registeredThemes.push_back(theme.name);
    return postMessage(utf8ToWide(oss.str()));
}

// ============================================================================
// Monaco API — Language, Options, Actions
// ============================================================================
WebView2Result WebView2Container::setLanguage(const std::string& language) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    return postMessage(utf8ToWide(
        "{\"type\":\"setLanguage\",\"payload\":{\"language\":\"" + language + "\"}}"
    ));
}

WebView2Result WebView2Container::setOptions(const MonacoEditorOptions& opts) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);

    std::ostringstream oss;
    oss << "{\"type\":\"setOptions\",\"payload\":{";
    oss << "\"fontSize\":" << opts.fontSize << ",";
    oss << "\"fontFamily\":\"" << opts.fontFamily << "\",";
    oss << "\"wordWrap\":\"" << (opts.wordWrap ? "on" : "off") << "\",";
    oss << "\"minimap\":{\"enabled\":" << (opts.minimap ? "true" : "false") << "},";
    oss << "\"lineNumbers\":\"" << (opts.lineNumbers ? "on" : "off") << "\",";
    oss << "\"renderWhitespace\":\"" << (opts.renderWhitespace ? "all" : "none") << "\",";
    oss << "\"tabSize\":" << opts.tabSize << ",";
    oss << "\"insertSpaces\":" << (opts.insertSpaces ? "true" : "false") << ",";
    oss << "\"bracketPairColorization\":{\"enabled\":" << (opts.bracketPairColorization ? "true" : "false") << "},";
    oss << "\"smoothScrolling\":" << (opts.smoothScrolling ? "true" : "false") << ",";
    oss << "\"cursorBlinking\":\"" << (opts.cursorBlinking ? "smooth" : "blink") << "\",";
    oss << "\"cursorSmoothCaretAnimation\":\"" << (opts.cursorSmoothCaretAnimation ? "on" : "off") << "\",";
    oss << "\"stickyScroll\":{\"enabled\":" << (opts.stickyScroll ? "true" : "false") << "},";
    oss << "\"readOnly\":" << (opts.readOnly ? "true" : "false") << ",";
    oss << "\"cursorStyle\":\"" << opts.cursorStyle << "\"";
    oss << "}}";

    return postMessage(utf8ToWide(oss.str()));
}

WebView2Result WebView2Container::executeAction(const std::string& actionId) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    m_stats.monacoActions.fetch_add(1, std::memory_order_relaxed);
    return postMessage(utf8ToWide(
        "{\"type\":\"action\",\"payload\":{\"id\":\"" + actionId + "\"}}"
    ));
}

WebView2Result WebView2Container::insertText(const std::string& text) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    // Escape text for JSON
    std::string escaped;
    for (char c : text) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return postMessage(utf8ToWide(
        "{\"type\":\"insertText\",\"payload\":{\"text\":\"" + escaped + "\"}}"
    ));
}

WebView2Result WebView2Container::revealLine(int lineNumber) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    return postMessage(utf8ToWide(
        "{\"type\":\"revealLine\",\"payload\":{\"line\":" + std::to_string(lineNumber) + "}}"
    ));
}

WebView2Result WebView2Container::setReadOnly(bool readOnly) {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    return postMessage(utf8ToWide(
        "{\"type\":\"setReadOnly\",\"payload\":{\"readOnly\":" +
        std::string(readOnly ? "true" : "false") + "}}"
    ));
}

WebView2Result WebView2Container::focus() {
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);
    return postMessage(utf8ToWide("{\"type\":\"focus\"}"));
}

// ============================================================================
// Execute arbitrary JavaScript in the WebView2 context
// ============================================================================
WebView2Result WebView2Container::executeScript(const std::string& javascript) {
    if (!m_webview) return WebView2Result::error("WebView not initialized", -1);
    if (!isReady()) return WebView2Result::error("Monaco not ready", -1);

    std::wstring wScript = utf8ToWide(javascript);
    HRESULT hr = m_webview->ExecuteScript(wScript.c_str(), nullptr);
    if (FAILED(hr)) {
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        return WebView2Result::error("ExecuteScript failed", (int)hr);
    }

    m_stats.messagesPosted.fetch_add(1, std::memory_order_relaxed);
    return WebView2Result::ok("Script executed");
}

// ============================================================================
// Callbacks
// ============================================================================
void WebView2Container::setReadyCallback(WebView2ReadyCallback fn, void* userData) {
    m_readyFn = fn; m_readyData = userData;
}
void WebView2Container::setContentCallback(MonacoContentCallback fn, void* userData) {
    m_contentFn = fn; m_contentData = userData;
}
void WebView2Container::setCursorCallback(MonacoCursorCallback fn, void* userData) {
    m_cursorFn = fn; m_cursorData = userData;
}
void WebView2Container::setErrorCallback(MonacoErrorCallback fn, void* userData) {
    m_errorFn = fn; m_errorData = userData;
}

// ============================================================================
// COM Callbacks — Called asynchronously by WebView2
// ============================================================================

void WebView2Container::onEnvironmentCreated(HRESULT hr, ICoreWebView2Environment* env) {
    if (FAILED(hr) || !env) {
        m_state = WebView2State::Error;
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        if (m_errorFn) m_errorFn("Failed to create WebView2 environment", m_errorData);
        return;
    }

    m_environment = env;
    m_environment->AddRef();
    m_state = WebView2State::EnvironmentReady;

    // Now create the controller (async)
    m_state = WebView2State::ControllerCreating;
    auto* handler = new ControllerCreatedHandler(this);
    hr = m_environment->CreateCoreWebView2Controller(m_parentWindow, handler);
    if (FAILED(hr)) {
        handler->Release();
        m_state = WebView2State::Error;
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        if (m_errorFn) m_errorFn("CreateCoreWebView2Controller failed", m_errorData);
    }
}

void WebView2Container::onControllerCreated(HRESULT hr, ICoreWebView2Controller* ctrl) {
    if (FAILED(hr) || !ctrl) {
        m_state = WebView2State::Error;
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        if (m_errorFn) m_errorFn("Failed to create WebView2 controller", m_errorData);
        return;
    }

    m_controller = ctrl;
    m_controller->AddRef();
    m_state = WebView2State::ControllerReady;

    // Get the core webview
    ICoreWebView2* webview = nullptr;
    hr = m_controller->get_CoreWebView2(&webview);
    if (FAILED(hr) || !webview) {
        m_state = WebView2State::Error;
        return;
    }
    m_webview = webview;

    // Configure settings
    ICoreWebView2Settings* settings = nullptr;
    hr = m_webview->get_Settings(&settings);
    if (SUCCEEDED(hr) && settings) {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);     // Allow for debugging
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
        settings->Release();
    }

    // Set initial bounds
    RECT bounds = {
        0, 0,
        m_bounds.right - m_bounds.left,
        m_bounds.bottom - m_bounds.top
    };
    if (bounds.right <= 0) bounds.right = 800;
    if (bounds.bottom <= 0) bounds.bottom = 600;
    m_controller->put_Bounds(bounds);

    // Register message handler
    auto* msgHandler = new WebMessageReceivedHandler(this);
    EventRegistrationToken token;
    m_webview->add_WebMessageReceived(msgHandler, &token);

    // Register navigation handler
    auto* navHandler = new NavigationCompletedHandler(this);
    EventRegistrationToken navToken;
    m_webview->add_NavigationCompleted(navHandler, &navToken);

    // Navigate to the Monaco editor HTML
    navigateToMonacoHtml();
}

void WebView2Container::onWebMessageReceived(const std::wstring& messageJson) {
    m_stats.messagesReceived.fetch_add(1, std::memory_order_relaxed);

    std::string msg = wideToUtf8(messageJson);

    // Quick JSON field extraction (no external JSON library needed)
    // Format: {"type":"xxx", ...}
    auto extractField = [&msg](const std::string& field) -> std::string {
        std::string needle = "\"" + field + "\":\"";
        size_t pos = msg.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        size_t end = msg.find('"', pos);
        if (end == std::string::npos) return "";
        return msg.substr(pos, end - pos);
    };

    auto extractInt = [&msg](const std::string& field) -> int {
        std::string needle = "\"" + field + "\":";
        size_t pos = msg.find(needle);
        if (pos == std::string::npos) return -1;
        pos += needle.size();
        return std::atoi(msg.c_str() + pos);
    };

    std::string type = extractField("type");

    if (type == "ready") {
        m_state = WebView2State::MonacoReady;
        if (m_readyFn) m_readyFn(m_readyData);
    }
    else if (type == "contentChanged") {
        // Content changed — editor is dirty
        // No payload needed for "dirty" notification; use getContent to retrieve
    }
    else if (type == "cursorChanged") {
        int line = extractInt("line");
        int col = extractInt("column");
        if (m_cursorFn) m_cursorFn(line, col, m_cursorData);
    }
    else if (type == "content") {
        // Response to getContent request
        // Extract the content field (may be large)
        std::string needle = "\"content\":\"";
        size_t pos = msg.find(needle);
        if (pos != std::string::npos) {
            pos += needle.size();
            // Find the closing quote (handling escapes)
            std::string content;
            bool escape = false;
            for (size_t i = pos; i < msg.size(); i++) {
                if (escape) {
                    switch (msg[i]) {
                        case 'n': content += '\n'; break;
                        case 'r': content += '\r'; break;
                        case 't': content += '\t'; break;
                        case '"': content += '"'; break;
                        case '\\': content += '\\'; break;
                        default: content += msg[i]; break;
                    }
                    escape = false;
                } else if (msg[i] == '\\') {
                    escape = true;
                } else if (msg[i] == '"') {
                    break;
                } else {
                    content += msg[i];
                }
            }
            if (m_contentFn) {
                m_contentFn(content.c_str(), (uint32_t)content.size(), m_contentData);
            }
        }
    }
    else if (type == "error") {
        std::string error = extractField("message");
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        if (m_errorFn) m_errorFn(error.c_str(), m_errorData);
    }
    else if (type == "themeApplied") {
        // Theme switch confirmed
    }
    // ---- WebSocket State Bridge Messages ----
    else if (type == "wsConnected") {
        OutputDebugStringA("[WebView2] WebSocket connected to API server\n");
    }
    else if (type == "wsDisconnected") {
        OutputDebugStringA("[WebView2] WebSocket disconnected from API server\n");
    }
    else if (type == "memoryStats") {
        // Real-time memory stats from server push
        // Forward to Win32IDE memory panel update
        OutputDebugStringA("[WebView2] Memory stats push received\n");
        // The stats are in the JSON payload — Win32IDE can extract and update panels
    }
    else if (type == "modelState") {
        // Real-time model state from server push
        OutputDebugStringA("[WebView2] Model state push received\n");
    }
    else if (type == "patchEvent") {
        // A hotpatch was applied/reverted by another process
        OutputDebugStringA("[WebView2] Patch event from cross-process MMF\n");
    }
    else if (type == "serverEvent") {
        // Generic event from another IDE instance
        OutputDebugStringA("[WebView2] Server event from cross-process MMF\n");
    }
    else if (type == "stateReconciled") {
        // Full state reconciliation completed (after WS reconnect)
        OutputDebugStringA("[WebView2] State reconciliation complete — local state overwritten with server truth\n");
    }
}

void WebView2Container::onNavigationCompleted(bool success) {
    m_stats.navigations.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        m_state = WebView2State::Error;
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        if (m_errorFn) m_errorFn("Monaco HTML navigation failed", m_errorData);
    } else if (m_state.load() == WebView2State::MonacoLoading) {
        // Monaco will send "ready" via PostMessage once initialized
        // State transition happens in onWebMessageReceived
    }
}

// ============================================================================
// Post Message — Send JSON to Monaco JS
// ============================================================================
WebView2Result WebView2Container::postMessage(const std::wstring& json) {
    if (!m_webview) return WebView2Result::error("WebView not initialized", -1);

    HRESULT hr = m_webview->PostWebMessageAsJson(json.c_str());
    if (FAILED(hr)) {
        m_stats.errors.fetch_add(1, std::memory_order_relaxed);
        return WebView2Result::error("PostWebMessageAsJson failed", (int)hr);
    }

    m_stats.messagesPosted.fetch_add(1, std::memory_order_relaxed);
    return WebView2Result::ok();
}

// ============================================================================
// Navigate to Monaco HTML
// ============================================================================
void WebView2Container::navigateToMonacoHtml() {
    m_state = WebView2State::MonacoLoading;
    std::string html = generateMonacoHtml();
    std::wstring wHtml = utf8ToWide(html);
    m_webview->NavigateToString(wHtml.c_str());
}

// ============================================================================
// Generate Monaco HTML — Self-contained editor page
// ============================================================================
// This generates a complete HTML document that:
//   1. Loads Monaco from the official CDN (unpkg.com/monaco-editor)
//   2. Creates a full-viewport editor instance
//   3. Registers all 16 RawrXD themes via defineTheme
//   4. Sets up bidirectional PostMessage bridge
//   5. Applies Cyberpunk Neon as the default theme
// ============================================================================
std::string WebView2Container::generateMonacoHtml() {
    // Get all theme definitions as JavaScript
    std::string allThemeJs = MonacoThemeExporter::toAllDefineThemeJs();

    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RawrXD Monaco Editor</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  html, body { width: 100%; height: 100%; overflow: hidden; background: #0A0A12; }
  #editor-container { width: 100%; height: 100%; }
</style>
</head>
<body>
<div id="editor-container"></div>

<script src="https://unpkg.com/monaco-editor@0.50.0/min/vs/loader.js"></script>
<script>
// ============================================================================
// RawrXD Monaco Editor — WebView2 Bridge
// ============================================================================

require.config({
    paths: { vs: 'https://unpkg.com/monaco-editor@0.50.0/min/vs' }
});

require(['vs/editor/editor.main'], function() {
    // ---- Register all 16 RawrXD themes ----
)" << allThemeJs << R"(

    // ---- Create the editor ----
    const editor = monaco.editor.create(document.getElementById('editor-container'), {
        value: '// Welcome to RawrXD IDE — Monaco Editor (WebView2)\n// Phase 26: Feature #206 — WebView2 Integration\n//\n// The hostile takeover is complete.\n// All 16 themes from Win32 are now available in Monaco.\n// Type away — Cyberpunk Neon is watching.\n\n#include <iostream>\n\nint main() {\n    std::cout << \"RawrXD Engine v7.4.0\" << std::endl;\n    return 0;\n}\n',
        language: 'cpp',
        theme: 'rawrxd-cyberpunk-neon',
        fontSize: 14,
        fontFamily: "Consolas, 'Courier New', monospace",
        automaticLayout: true,
        minimap: { enabled: true },
        scrollBeyondLastLine: false,
        renderWhitespace: 'none',
        bracketPairColorization: { enabled: true },
        cursorBlinking: 'smooth',
        cursorSmoothCaretAnimation: 'on',
        smoothScrolling: true,
        padding: { top: 8, bottom: 8 },
        suggest: { showMethods: true, showFunctions: true, showConstructors: true },
        quickSuggestions: true,
        wordBasedSuggestions: 'currentDocument',
        tabSize: 4,
        insertSpaces: true,
        folding: true,
        foldingStrategy: 'auto',
        links: true,
        colorDecorators: true,
        'semanticHighlighting.enabled': true,
    });

    // ---- Message Bridge: C++ → JS ----
    window.chrome.webview.addEventListener('message', function(event) {
        const msg = (typeof event.data === 'string') ? JSON.parse(event.data) : event.data;
        const payload = msg.payload || {};

        switch (msg.type) {
            case 'setContent':
                editor.setValue(payload.content || '');
                if (payload.language) {
                    monaco.editor.setModelLanguage(editor.getModel(), payload.language);
                }
                break;

            case 'getContent':
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'content',
                    content: editor.getValue()
                }));
                break;

            case 'setTheme':
                monaco.editor.setTheme(payload.theme);
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'themeApplied',
                    theme: payload.theme
                }));
                break;

            case 'defineTheme':
                try {
                    monaco.editor.defineTheme(payload.name, {
                        base: payload.base,
                        inherit: true,
                        rules: payload.rules,
                        colors: payload.colors
                    });
                } catch (e) {
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'error',
                        message: 'defineTheme failed: ' + e.message
                    }));
                }
                break;

            case 'setLanguage':
                monaco.editor.setModelLanguage(editor.getModel(), payload.language);
                break;

            case 'setOptions':
                editor.updateOptions(payload);
                break;

            case 'action':
                editor.trigger('rawrxd', payload.id, null);
                break;

            case 'insertText':
                editor.trigger('rawrxd', 'type', { text: payload.text });
                break;

            case 'revealLine':
                editor.revealLineInCenter(payload.line);
                break;

            case 'setReadOnly':
                editor.updateOptions({ readOnly: payload.readOnly });
                break;

            case 'focus':
                editor.focus();
                break;
        }
    });

    // ---- Message Bridge: JS → C++ ----
    editor.onDidChangeModelContent(function() {
        window.chrome.webview.postMessage(JSON.stringify({
            type: 'contentChanged'
        }));
    });

    editor.onDidChangeCursorPosition(function(e) {
        window.chrome.webview.postMessage(JSON.stringify({
            type: 'cursorChanged',
            line: e.position.lineNumber,
            column: e.position.column
        }));
    });

    // Signal ready
    window.chrome.webview.postMessage(JSON.stringify({ type: 'ready' }));

    // ================================================================
    // RawrXD WebSocket State Bridge — Real-time Server Push + Reconnect Reconciliation
    // ================================================================
    // Connects to the local API server's WebSocket endpoint to receive
    // server-push updates for memory stats, model state, and patch events.
    // On reconnect, fetches /api/full-state to reconcile local state.
    // ================================================================

    (function() {
        var WS_URL = 'ws://127.0.0.1:11434/ws';
        var ws = null;
        var reconnectDelay = 1000;
        var maxReconnectDelay = 30000;
        var reconnectAttempts = 0;
        var stateCache = {
            memory: null,
            model: null,
            patches: [],
            events: [],
            fullState: null,
            lastReconcileTime: 0
        };

        function connectWebSocket() {
            try {
                ws = new WebSocket(WS_URL);
            } catch(e) {
                scheduleReconnect();
                return;
            }

            ws.onopen = function() {
                reconnectDelay = 1000;
                reconnectAttempts = 0;

                // Subscribe to all channels
                ws.send(JSON.stringify({
                    type: 'subscribe',
                    channels: ['memory', 'model', 'patches', 'events']
                }));

                // If this is a reconnection, request full state reconciliation
                if (stateCache.lastReconcileTime > 0) {
                    reconcileState();
                }

                // Notify C++ side that WS is connected
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'wsConnected',
                    reconnected: stateCache.lastReconcileTime > 0
                }));
            };

            ws.onmessage = function(event) {
                try {
                    var msg = JSON.parse(event.data);
                    handleServerPush(msg);
                } catch(e) { /* ignore parse errors */ }
            };

            ws.onclose = function(event) {
                ws = null;
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'wsDisconnected',
                    code: event.code,
                    reason: event.reason
                }));
                scheduleReconnect();
            };

            ws.onerror = function() {
                // onerror is always followed by onclose
            };
        }

        function scheduleReconnect() {
            reconnectAttempts++;
            var delay = Math.min(reconnectDelay * Math.pow(1.5, reconnectAttempts), maxReconnectDelay);
            setTimeout(connectWebSocket, delay);
        }

        function handleServerPush(msg) {
            switch(msg.type) {
                case 'memory':
                    stateCache.memory = msg.data;
                    // Forward to C++ for UI panel update
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'memoryStats',
                        data: msg.data
                    }));
                    break;

                case 'model':
                    stateCache.model = msg.data;
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'modelState',
                        data: msg.data
                    }));
                    break;

                case 'patch':
                    stateCache.patches.push(msg.data);
                    if (stateCache.patches.length > 256) stateCache.patches.shift();
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'patchEvent',
                        data: msg.data
                    }));
                    break;

                case 'event':
                    stateCache.events.push(msg.data);
                    if (stateCache.events.length > 64) stateCache.events.shift();
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'serverEvent',
                        data: msg.data
                    }));
                    break;

                case 'full-state':
                    // Full state reconciliation received
                    stateCache.fullState = msg.data;
                    stateCache.memory = msg.data.memory || stateCache.memory;
                    stateCache.model = msg.data.model || stateCache.model;
                    stateCache.lastReconcileTime = Date.now();
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'stateReconciled',
                        data: msg.data
                    }));
                    break;

                case 'welcome':
                    stateCache.lastReconcileTime = Date.now();
                    break;

                case 'pong':
                    break;
            }
        }

        // Full state reconciliation on reconnect
        // This is the critical path: when WebSocket drops during a patch apply
        // and reconnects, we fetch the complete server state and overwrite local.
        function reconcileState() {
            // Strategy 1: Request via WebSocket
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'get-full-state' }));
            }

            // Strategy 2: HTTP fallback — always works even if WS is flaky
            fetch('http://127.0.0.1:11434/api/full-state')
                .then(function(resp) { return resp.json(); })
                .then(function(serverState) {
                    stateCache.fullState = serverState;
                    stateCache.memory = serverState.memory || stateCache.memory;
                    stateCache.model = serverState.model || stateCache.model;
                    stateCache.lastReconcileTime = Date.now();
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'stateReconciled',
                        data: serverState,
                        source: 'http-fallback'
                    }));
                })
                .catch(function() { /* HTTP reconcile failed, WS response should cover */ });
        }

        // Keepalive ping every 20 seconds
        setInterval(function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'ping' }));
            }
        }, 20000);

        // Expose to window for C++ bridge to query state
        window.rawrxdState = stateCache;
        window.rawrxdReconcile = reconcileState;

        // Initial connection
        connectWebSocket();
    })();
});
</script>
</body>
</html>)";

    return html.str();
}

// ============================================================================
// UTF-8 ↔ Wide String Conversion
// ============================================================================
std::wstring WebView2Container::utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

std::string WebView2Container::wideToUtf8(const std::wstring& str) {
    if (str.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size, nullptr, nullptr);
    return result;
}
