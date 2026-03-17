/// =============================================================================
/// example_usage.cpp
/// Example usage of WebView2Container for Monaco Editor integration
/// =============================================================================

#include <windows.h>
#include <iostream>
#include <string>
#include "WebView2Container.h"

// Global variables
HWND g_hwndMain = nullptr;
HWND g_hwndWebView = nullptr;
bool g_editorReady = false;

// Callback functions
void OnEditorReady(void* userData) {
    std::cout << "Monaco Editor is ready!" << std::endl;
    g_editorReady = true;
    
    // Set initial content
    const char* initialCode = R"(// Welcome to Monaco Editor with WebView2!
function helloWorld() {
    console.log("Hello from Monaco Editor!");
    return "WebView2 + Monaco = Amazing!";
}

// Call the function
const result = helloWorld();
console.log(result);)";
    
    WebView2Container_SetContent(initialCode, nullptr);
    WebView2Container_SetLanguage("javascript");
    WebView2Container_SetTheme("dark");
    
    MonacoEditorOptions options = {0};
    options.fontSize = 16;
    options.tabSize = 2;
    options.wordWrap = true;
    options.minimapEnabled = true;
    WebView2Container_SetOptions(&options);
}

void OnContentChanged(const char* content, unsigned int length, void* userData) {
    std::cout << "Content changed! Length: " << length << std::endl;
    // Optionally save content or perform other actions
}

void OnCursorChanged(int line, int column, void* userData) {
    std::cout << "Cursor at line " << line << ", column " << column << std::endl;
}

void OnError(const char* error, void* userData) {
    std::cout << "WebView2 Error: " << error << std::endl;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create child window for WebView2
            g_hwndWebView = CreateWindow(
                L"STATIC", L"",
                WS_CHILD | WS_VISIBLE,
                0, 0, 800, 600,
                hwnd, nullptr, GetModuleHandle(nullptr), nullptr
            );
            
            // Initialize WebView2Container
            WebView2Container_Constructor();
            
            // Set callbacks
            WebView2Container_SetReadyCallback(OnEditorReady, nullptr);
            WebView2Container_SetContentCallback(OnContentChanged, nullptr);
            WebView2Container_SetCursorCallback(OnCursorChanged, nullptr);
            WebView2Container_SetErrorCallback(OnError, nullptr);
            
            // Initialize WebView2
            WebView2Result result = WebView2Container_Initialize(g_hwndWebView, nullptr);
            if (result.status != 0) {
                MessageBoxA(hwnd, result.message, "WebView2 Error", MB_OK | MB_ICONERROR);
            }
            
            break;
        }
        
        case WM_SIZE: {
            if (g_hwndWebView) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                SetWindowPos(g_hwndWebView, nullptr, 0, 0, 
                           rect.right - rect.left, rect.bottom - rect.top,
                           SWP_NOZORDER);
                WebView2Container_Resize(0, 0, 
                                       rect.right - rect.left, 
                                       rect.bottom - rect.top);
            }
            break;
        }
        
        case WM_KEYDOWN: {
            if (g_editorReady) {
                switch (wParam) {
                    case VK_F1: {
                        // Execute custom script
                        WebView2Container_ExecuteScript(
                            "alert('Hello from F1 key! Current content length: ' + window.getContent().length);"
                        );
                        break;
                    }
                    case VK_F5: {
                        // Toggle theme
                        static bool darkMode = true;
                        WebView2Container_SetTheme(darkMode ? "light" : "dark");
                        darkMode = !darkMode;
                        break;
                    }
                    case VK_F10: {
                        // Insert text at cursor
                        WebView2Container_InsertText("\n// Inserted by F10 key\n");
                        break;
                    }
                }
            }
            break;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1001: { // Menu item example
                    if (g_editorReady) {
                        WebView2Container_SetReadOnly(true);
                        MessageBoxA(hwnd, "Editor is now read-only", "Info", MB_OK);
                    }
                    break;
                }
                case 1002: { // Menu item example
                    if (g_editorReady) {
                        WebView2Container_SetReadOnly(false);
                        MessageBoxA(hwnd, "Editor is now editable", "Info", MB_OK);
                    }
                    break;
                }
            }
            break;
        }
        
        case WM_DESTROY: {
            WebView2Container_Destroy();
            WebView2Container_Destructor();
            PostQuitMessage(0);
            break;
        }
        
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                     LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WebView2MonacoExample";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    RegisterClassW(&wc);
    
    // Create main window
    g_hwndMain = CreateWindowW(
        L"WebView2MonacoExample",
        L"Monaco Editor with WebView2 - F1=Script, F5=Theme, F10=Insert",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (!g_hwndMain) {
        return -1;
    }
    
    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

/// =============================================================================
/// Build Instructions
/// =============================================================================
/// 
/// To compile this example:
/// 
/// 1. Using Visual Studio:
///    - Create a new C++ Windows Desktop Application project
///    - Add this file and WebView2Container.cpp to the project
///    - Install WebView2 NuGet package: Microsoft.Web.WebView2
///    - Add these libraries to Additional Dependencies:
///      ole32.lib, oleaut32.lib, user32.lib, version.lib
/// 
/// 2. Using CMake:
///    cmake -B build -DWEBVIEW2_BUILD_EXAMPLE=ON
///    cmake --build build
/// 
/// 3. Using g++/clang++ (with WebView2 SDK):
///    g++ -std=c++17 example_usage.cpp WebView2Container_stubs.cpp \
///        -I"path/to/webview2/include" \
///        -L"path/to/webview2/lib" \
///        -lWebView2Loader.dll -lole32 -loleaut32 -luser32 -lversion \
///        -o monaco_example.exe
/// 
/// =============================================================================