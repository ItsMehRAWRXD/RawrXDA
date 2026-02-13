// RawrXD_WebView.hpp - Native WebView2 Wrapper
// Pure Win32 + WebView2 COM - No Qt Dependencies
// Integrated into RawrXD IDE for AI Dashboards and documentation

#pragma once

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include "webview2/build/native/include/WebView2.h"
#include <string>

using namespace Microsoft::WRL;

namespace RawrXD {

class WebView {
public:
    WebView() : m_hWnd(NULL) {}

    bool Initialize(HWND hWndParent, const std::wstring& url) {
        m_hWnd = hWndParent;
        m_initialUrl = url;

        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result)) return result;
                    
                    env->CreateCoreWebView2Controller(m_hWnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (controller != nullptr) {
                                    m_controller = controller;
                                    m_controller->get_CoreWebView2(&m_webView);
                                }

                                // Resize webview to parent bounds
                                RECT bounds;
                                GetClientRect(m_hWnd, &bounds);
                                m_controller->put_Bounds(bounds);

                                // Navigate to initial URL
                                m_webView->Navigate(m_initialUrl.c_str());
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());

        return SUCCEEDED(hr);
    }

    void Resize(const RECT& bounds) {
        if (m_controller) {
            m_controller->put_Bounds(bounds);
        }
    }

    void Navigate(const std::wstring& url) {
        if (m_webView) {
            m_webView->Navigate(url.c_str());
        }
    }

private:
    HWND m_hWnd;
    std::wstring m_initialUrl;
    ComPtr<ICoreWebView2Controller> m_controller;
    ComPtr<ICoreWebView2> m_webView;
};

} // namespace RawrXD
