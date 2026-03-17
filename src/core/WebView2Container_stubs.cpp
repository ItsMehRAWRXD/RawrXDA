/// =============================================================================
/// WebView2Container.cpp
/// Full WebView2 implementation for Monaco Editor integration
/// =============================================================================

#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <windows.h>
#include <wrl.h>
#include <comdef.h>
#include <memory>
#include <map>
#include <functional>

// WebView2 headers
#include "WebView2.h"

using namespace Microsoft::WRL;

struct MonacoEditorOptions {
    int fontSize;
    int tabSize;
    bool wordWrap;
    bool minimapEnabled;
};

struct WebView2Result {
    int status;
    char message[512];
};

// Global state management
struct WebView2State {
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    HWND parentHwnd;
    bool isInitialized;
    bool isNavigating;
    std::string lastContent;
    
    // Callbacks
    std::function<void(void*)> readyCallback;
    void* readyUserData;
    std::function<void(const char*, unsigned int, void*)> contentCallback;
    void* contentUserData;
    std::function<void(int, int, void*)> cursorCallback;
    void* cursorUserData;
    std::function<void(const char*, void*)> errorCallback;
    void* errorUserData;
    
    WebView2State() : parentHwnd(nullptr), isInitialized(false), isNavigating(false),
                      readyUserData(nullptr), contentUserData(nullptr), 
                      cursorUserData(nullptr), errorUserData(nullptr) {}
};

static WebView2State g_state;

// Helper function to convert string to wstring
std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
    return result;
}

// Helper function to convert wstring to string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, NULL, NULL);
    return result;
}

// Monaco Editor HTML template
const char* GetMonacoHTML() {
    return R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body, html { margin: 0; padding: 0; height: 100%; overflow: hidden; font-family: 'Consolas', 'Monaco', monospace; }
        #container { height: 100vh; }
    </style>
</head>
<body>
    <div id="container"></div>
    <script src="https://cdn.jsdelivr.net/npm/monaco-editor@latest/min/vs/loader.js"></script>
    <script>
        require.config({ paths: { 'vs': 'https://cdn.jsdelivr.net/npm/monaco-editor@latest/min/vs' }});
        
        let editor;
        let currentTheme = 'vs';
        
        require(['vs/editor/editor.main'], function() {
            editor = monaco.editor.create(document.getElementById('container'), {
                value: '// Monaco Editor Ready\n',
                language: 'javascript',
                theme: currentTheme,
                fontSize: 14,
                tabSize: 4,
                wordWrap: 'off',
                minimap: { enabled: true },
                automaticLayout: true,
                scrollBeyondLastLine: false
            });
            
            // Notify that editor is ready
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.postMessage({type: 'ready'});
            }
            
            // Content change events
            editor.onDidChangeModelContent(() => {
                const content = editor.getValue();
                if (window.chrome && window.chrome.webview) {
                    window.chrome.webview.postMessage({
                        type: 'contentChanged', 
                        content: content,
                        length: content.length
                    });
                }
            });
            
            // Cursor position events
            editor.onDidChangeCursorPosition((e) => {
                if (window.chrome && window.chrome.webview) {
                    window.chrome.webview.postMessage({
                        type: 'cursorChanged',
                        line: e.position.lineNumber,
                        column: e.position.column
                    });
                }
            });
        });
        
        // External API functions
        window.setContent = function(content, baseURL) {
            if (editor) {
                editor.setValue(content || '');
                return true;
            }
            return false;
        };
        
        window.getContent = function() {
            return editor ? editor.getValue() : '';
        };
        
        window.setTheme = function(theme) {
            currentTheme = theme === 'dark' ? 'vs-dark' : 'vs';
            if (editor) {
                monaco.editor.setTheme(currentTheme);
            }
        };
        
        window.setLanguage = function(language) {
            if (editor) {
                monaco.editor.setModelLanguage(editor.getModel(), language);
            }
        };
        
        window.setOptions = function(options) {
            if (editor && options) {
                editor.updateOptions({
                    fontSize: options.fontSize || 14,
                    tabSize: options.tabSize || 4,
                    wordWrap: options.wordWrap ? 'on' : 'off',
                    minimap: { enabled: options.minimapEnabled !== false }
                });
            }
        };
        
        window.insertText = function(text) {
            if (editor) {
                const position = editor.getPosition();
                editor.executeEdits('', [{
                    range: new monaco.Range(position.lineNumber, position.column, position.lineNumber, position.column),
                    text: text
                }]);
            }
        };
        
        window.revealLine = function(lineNumber) {
            if (editor) {
                editor.revealLineInCenter(lineNumber);
            }
        };
        
        window.setReadOnly = function(readOnly) {
            if (editor) {
                editor.updateOptions({ readOnly: readOnly });
            }
        };
        
        window.focusEditor = function() {
            if (editor) {
                editor.focus();
            }
        };
    </script>
</body>
</html>)HTML";
}

extern "C" {
    
    /// Constructor
    void WebView2Container_Constructor(void) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        g_state = WebView2State();
    }
    
    /// Destructor
    void WebView2Container_Destructor(void) {
        WebView2Container_Destroy();
        CoUninitialize();
    }
    
    /// Initialize
    WebView2Result WebView2Container_Initialize(void* hwnd, const char* initialURL) {
        WebView2Result res = {0};
        
        if (!hwnd) {
            res.status = -1;
            strcpy_s(res.message, "Invalid window handle");
            return res;
        }
        
        g_state.parentHwnd = static_cast<HWND>(hwnd);
        
        // Create WebView2 environment
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (SUCCEEDED(result)) {
                        g_state.environment = env;
                        
                        // Create WebView2 controller
                        return env->CreateCoreWebView2Controller(g_state.parentHwnd,
                            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                    if (SUCCEEDED(result)) {
                                        g_state.controller = controller;
                                        controller->get_CoreWebView2(&g_state.webview);
                                        
                                        // Set up message handling
                                        g_state.webview->add_WebMessageReceived(
                                            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                                [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                                    LPWSTR messageRaw;
                                                    args->TryGetWebMessageAsString(&messageRaw);
                                                    std::string message = WStringToString(messageRaw);
                                                    CoTaskMemFree(messageRaw);
                                                    
                                                    // Parse JSON message (simple parsing)
                                                    if (message.find("\"type\":\"ready\"") != std::string::npos) {
                                                        g_state.isInitialized = true;
                                                        if (g_state.readyCallback) {
                                                            g_state.readyCallback(g_state.readyUserData);
                                                        }
                                                    } else if (message.find("\"type\":\"contentChanged\"") != std::string::npos) {
                                                        // Extract content from JSON (simplified)
                                                        size_t contentStart = message.find("\"content\":\"") + 11;
                                                        size_t contentEnd = message.find("\",", contentStart);
                                                        if (contentStart != std::string::npos && contentEnd != std::string::npos) {
                                                            std::string content = message.substr(contentStart, contentEnd - contentStart);
                                                            g_state.lastContent = content;
                                                            if (g_state.contentCallback) {
                                                                g_state.contentCallback(content.c_str(), content.length(), g_state.contentUserData);
                                                            }
                                                        }
                                                    } else if (message.find("\"type\":\"cursorChanged\"") != std::string::npos) {
                                                        // Extract cursor position (simplified)
                                                        size_t lineStart = message.find("\"line\":") + 7;
                                                        size_t colStart = message.find("\"column\":") + 9;
                                                        if (lineStart != std::string::npos && colStart != std::string::npos) {
                                                            int line = std::stoi(message.substr(lineStart));
                                                            int col = std::stoi(message.substr(colStart));
                                                            if (g_state.cursorCallback) {
                                                                g_state.cursorCallback(line, col, g_state.cursorUserData);
                                                            }
                                                        }
                                                    }
                                                    return S_OK;
                                                }).Get(), nullptr);
                                        
                                        // Navigate to Monaco HTML
                                        g_state.webview->NavigateToString(StringToWString(GetMonacoHTML()).c_str());
                                        
                                        // Resize to parent
                                        RECT bounds;
                                        GetClientRect(g_state.parentHwnd, &bounds);
                                        controller->put_Bounds(bounds);
                                    }
                                    return S_OK;
                                }).Get());
                    }
                    return S_OK;
                }).Get());
        
        if (SUCCEEDED(hr)) {
            res.status = 0;
            strcpy_s(res.message, "WebView2 initialization started");
        } else {
            res.status = -1;
            sprintf_s(res.message, "WebView2 initialization failed: 0x%08lX", hr);
        }
        
        return res;
    }
    
    /// Destroy
    WebView2Result WebView2Container_Destroy(void) {
        WebView2Result res = {0};
        
        if (g_state.controller) {
            g_state.controller->Close();
        }
        
        g_state.webview = nullptr;
        g_state.controller = nullptr;
        g_state.environment = nullptr;
        g_state.isInitialized = false;
        g_state.parentHwnd = nullptr;
        
        res.status = 0;
        strcpy_s(res.message, "WebView2 destroyed successfully");
        return res;
    }
    
    /// Resize
    void WebView2Container_Resize(int x, int y, int width, int height) {
        if (g_state.controller) {
            RECT bounds = {x, y, x + width, y + height};
            g_state.controller->put_Bounds(bounds);
        }
    }
    
    /// Show
    void WebView2Container_Show(void) {
        if (g_state.controller) {
            g_state.controller->put_IsVisible(TRUE);
        }
    }
    
    /// Hide
    void WebView2Container_Hide(void) {
        if (g_state.controller) {
            g_state.controller->put_IsVisible(FALSE);
        }
    }
    
    /// SetContent
    WebView2Result WebView2Container_SetContent(const char* content, const char* baseURL) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        std::string script = "window.setContent(";
        script += content ? ("\"" + std::string(content) + "\"") : "\"\"";
        script += ", ";
        script += baseURL ? ("\"" + std::string(baseURL) + "\"") : "null";
        script += ");";
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Content set successfully");
        return res;
    }
    
    /// GetContent
    WebView2Result WebView2Container_GetContent(void) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        // Content is obtained asynchronously through the content callback
        strncpy_s(res.message, g_state.lastContent.c_str(), sizeof(res.message) - 1);
        res.status = 0;
        return res;
    }
    
    /// SetTheme
    WebView2Result WebView2Container_SetTheme(const char* themeName) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        std::string script = "window.setTheme(\"";
        script += themeName ? themeName : "light";
        script += "\");";
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Theme set successfully");
        return res;
    }
    
    /// SetLanguage
    WebView2Result WebView2Container_SetLanguage(const char* language) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        std::string script = "window.setLanguage(\"";
        script += language ? language : "javascript";
        script += "\");";
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Language set successfully");
        return res;
    }
    
    /// SetOptions
    WebView2Result WebView2Container_SetOptions(const MonacoEditorOptions* options) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        if (!options) {
            res.status = -1;
            strcpy_s(res.message, "Invalid options");
            return res;
        }
        
        char script[512];
        sprintf_s(script, "window.setOptions({fontSize: %d, tabSize: %d, wordWrap: %s, minimapEnabled: %s});",
                  options->fontSize, options->tabSize,
                  options->wordWrap ? "true" : "false",
                  options->minimapEnabled ? "true" : "false");
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Options set successfully");
        return res;
    }
    
    /// ExecuteScript
    WebView2Result WebView2Container_ExecuteScript(const char* script) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        if (!script) {
            res.status = -1;
            strcpy_s(res.message, "Invalid script");
            return res;
        }
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    if (FAILED(errorCode) && g_state.errorCallback) {
                        char errorMsg[256];
                        sprintf_s(errorMsg, "Script execution failed: 0x%08lX", errorCode);
                        g_state.errorCallback(errorMsg, g_state.errorUserData);
                    }
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Script executed");
        return res;
    }
    
    /// InsertText
    WebView2Result WebView2Container_InsertText(const char* text) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        std::string script = "window.insertText(\"";
        script += text ? text : "";
        script += "\");";
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Text inserted successfully");
        return res;
    }
    
    /// RevealLine
    WebView2Result WebView2Container_RevealLine(int lineNum) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        char script[128];
        sprintf_s(script, "window.revealLine(%d);", lineNum);
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Line revealed successfully");
        return res;
    }
    
    /// SetReadOnly
    WebView2Result WebView2Container_SetReadOnly(bool readOnly) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        char script[64];
        sprintf_s(script, "window.setReadOnly(%s);", readOnly ? "true" : "false");
        
        g_state.webview->ExecuteScript(StringToWString(script).c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Read-only mode set successfully");
        return res;
    }
    
    /// Focus
    WebView2Result WebView2Container_Focus(void) {
        WebView2Result res = {0};
        
        if (!g_state.webview || !g_state.isInitialized) {
            res.status = -1;
            strcpy_s(res.message, "WebView2 not initialized");
            return res;
        }
        
        g_state.webview->ExecuteScript(L"window.focusEditor();",
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                    return S_OK;
                }).Get());
        
        res.status = 0;
        strcpy_s(res.message, "Editor focused successfully");
        return res;
    }
    
    /// SetReadyCallback
    void WebView2Container_SetReadyCallback(void (*callback)(void*), void* userData) {
        g_state.readyCallback = callback;
        g_state.readyUserData = userData;
    }
    
    /// SetContentCallback
    void WebView2Container_SetContentCallback(void (*callback)(const char*, unsigned int, void*), void* userData) {
        g_state.contentCallback = callback;
        g_state.contentUserData = userData;
    }
    
    /// SetCursorCallback
    void WebView2Container_SetCursorCallback(void (*callback)(int, int, void*), void* userData) {
        g_state.cursorCallback = callback;
        g_state.cursorUserData = userData;
    }
    
    /// SetErrorCallback
    void WebView2Container_SetErrorCallback(void (*callback)(const char*, void*), void* userData) {
        g_state.errorCallback = callback;
        g_state.errorUserData = userData;
    }
}

/// =============================================================================
/// END OF WEBVIEW2CONTAINER IMPLEMENTATION
/// =============================================================================

/// =============================================================================
/// END OF WEBVIEW2CONTAINER STUBS
/// =============================================================================

