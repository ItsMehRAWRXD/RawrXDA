// ============================================================================
// inhouse_browser.cpp — RawrXD In-House Browser (WebView2)
// ============================================================================
// Purpose:
//   Provide a native, embedded browser that loads RawrXD's Qt-free web UI from
//   the local Standalone Web Bridge (http://127.0.0.1:8080/...) so the UI can
//   use HTTP + WebSocket without file:// origin restrictions.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>
#include <vector>

#include "WebView2.h"

// ----------------------------------------------------------------------------
// Minimal COM callback helper
// ----------------------------------------------------------------------------
template <typename T>
class CallbackBase : public T {
public:
    explicit CallbackBase() : m_ref(1) {}
    virtual ~CallbackBase() = default;

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = InterlockedDecrement(&m_ref);
        if (c == 0) delete this;
        return c;
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

private:
    ULONG m_ref;
};

static std::wstring widen_utf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w;
    w.resize((size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

static std::wstring get_localappdata_subdir(const wchar_t* leaf) {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return L".";
    }
    std::wstring out = path;
    out += L"\\RawrXD\\";
    out += leaf;
    CreateDirectoryW((std::wstring(path) + L"\\RawrXD").c_str(), nullptr);
    CreateDirectoryW(out.c_str(), nullptr);
    return out;
}

static HMODULE load_webview2_loader() {
    // Prefer local alongside exe, then ship folder.
    std::vector<std::wstring> candidates;

    candidates.push_back(L"WebView2Loader.dll");
    candidates.push_back(L".\\WebView2Loader.dll");

    candidates.push_back(L"D:\\rawrxd\\Ship\\webview2\\build\\native\\x64\\WebView2Loader.dll");
    candidates.push_back(L"D:\\rawrxd\\Ship\\webview2\\runtimes\\win-x64\\native\\WebView2Loader.dll");

    for (const auto& p : candidates) {
        HMODULE h = LoadLibraryW(p.c_str());
        if (h) return h;
    }
    return nullptr;
}

struct AppState {
    HWND hwnd = nullptr;
    ICoreWebView2Environment* env = nullptr;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webview = nullptr;
    HMODULE loader = nullptr;
    std::wstring initialUrl;
};

static void resize_webview(AppState* st) {
    if (!st || !st->controller || !st->hwnd) return;
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    st->controller->put_Bounds(rc);
}

class EnvCreatedHandler : public CallbackBase<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    explicit EnvCreatedHandler(AppState* st) : m_st(st) {}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) override {
        if (FAILED(errorCode) || !createdEnvironment || !m_st) return S_OK;
        m_st->env = createdEnvironment;
        m_st->env->AddRef();
        m_st->env->CreateCoreWebView2Controller(m_st->hwnd, new ControllerCreatedHandler(m_st));
        return S_OK;
    }
private:
    AppState* m_st;

    class ControllerCreatedHandler : public CallbackBase<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
    public:
        explicit ControllerCreatedHandler(AppState* st) : m_st(st) {}
        HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) override {
            if (FAILED(errorCode) || !createdController || !m_st) return S_OK;
            m_st->controller = createdController;
            m_st->controller->AddRef();

            ICoreWebView2* wv = nullptr;
            if (SUCCEEDED(createdController->get_CoreWebView2(&wv)) && wv) {
                m_st->webview = wv;
                m_st->webview->AddRef();

                ICoreWebView2Settings* settings = nullptr;
                if (SUCCEEDED(m_st->webview->get_Settings(&settings)) && settings) {
                    settings->put_AreDevToolsEnabled(TRUE);
                    settings->put_IsWebMessageEnabled(TRUE);
                    settings->put_IsScriptEnabled(TRUE);
                    settings->Release();
                }

                resize_webview(m_st);
                if (!m_st->initialUrl.empty()) {
                    m_st->webview->Navigate(m_st->initialUrl.c_str());
                }
            }
            if (wv) wv->Release();
            return S_OK;
        }
    private:
        AppState* m_st;
    };
};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        case WM_SIZE:
            if (st) resize_webview(st);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    AppState st{};
    st.initialUrl = L"http://127.0.0.1:8080/gui/ide_chatbot.html";

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        st.initialUrl = argv[1];
    }
    if (argv) LocalFree(argv);

    const wchar_t* kClass = L"RawrXD_InHouseBrowser";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    st.hwnd = CreateWindowExW(
        0, kClass, L"RawrXD In-House Browser",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, hInstance, &st);
    if (!st.hwnd) {
        CoUninitialize();
        return 1;
    }

    ShowWindow(st.hwnd, nCmdShow);
    UpdateWindow(st.hwnd);

    st.loader = load_webview2_loader();
    if (!st.loader) {
        MessageBoxW(st.hwnd, L"WebView2Loader.dll not found. Ensure WebView2 runtime + loader is available.", L"RawrXD", MB_ICONERROR);
        DestroyWindow(st.hwnd);
        CoUninitialize();
        return 1;
    }

    auto createEnv = reinterpret_cast<CreateCoreWebView2EnvironmentWithOptionsFunc>(
        GetProcAddress(st.loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!createEnv) {
        MessageBoxW(st.hwnd, L"CreateCoreWebView2EnvironmentWithOptions missing from loader.", L"RawrXD", MB_ICONERROR);
        DestroyWindow(st.hwnd);
        CoUninitialize();
        return 1;
    }

    auto userData = get_localappdata_subdir(L"WebView2");
    createEnv(nullptr, userData.c_str(), nullptr, new EnvCreatedHandler(&st));

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (st.webview) st.webview->Release();
    if (st.controller) st.controller->Release();
    if (st.env) st.env->Release();
    if (st.loader) FreeLibrary(st.loader);
    CoUninitialize();
    return 0;
}
