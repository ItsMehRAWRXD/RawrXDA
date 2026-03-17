/// =============================================================================
/// WebView2Container.h
/// Header file for WebView2 container implementation
/// =============================================================================

#ifndef WEBVIEW2CONTAINER_H
#define WEBVIEW2CONTAINER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/// Monaco Editor Options Structure
struct MonacoEditorOptions {
    int fontSize;
    int tabSize;
    bool wordWrap;
    bool minimapEnabled;
};

/// WebView2 Result Structure
struct WebView2Result {
    int status;         // 0 = success, negative = error
    char message[512];  // Status or error message
};

/// Callback function types
typedef void (*WebView2ReadyCallback)(void* userData);
typedef void (*WebView2ContentCallback)(const char* content, unsigned int length, void* userData);
typedef void (*WebView2CursorCallback)(int line, int column, void* userData);
typedef void (*WebView2ErrorCallback)(const char* error, void* userData);

/// Core functions
void WebView2Container_Constructor(void);
void WebView2Container_Destructor(void);
struct WebView2Result WebView2Container_Initialize(void* hwnd, const char* initialURL);
struct WebView2Result WebView2Container_Destroy(void);

/// Window management
void WebView2Container_Resize(int x, int y, int width, int height);
void WebView2Container_Show(void);
void WebView2Container_Hide(void);

/// Content management
struct WebView2Result WebView2Container_SetContent(const char* content, const char* baseURL);
struct WebView2Result WebView2Container_GetContent(void);

/// Editor configuration
struct WebView2Result WebView2Container_SetTheme(const char* themeName);
struct WebView2Result WebView2Container_SetLanguage(const char* language);
struct WebView2Result WebView2Container_SetOptions(const struct MonacoEditorOptions* options);

/// Editor operations
struct WebView2Result WebView2Container_ExecuteScript(const char* script);
struct WebView2Result WebView2Container_InsertText(const char* text);
struct WebView2Result WebView2Container_RevealLine(int lineNum);
struct WebView2Result WebView2Container_SetReadOnly(bool readOnly);
struct WebView2Result WebView2Container_Focus(void);

/// Callback registration
void WebView2Container_SetReadyCallback(WebView2ReadyCallback callback, void* userData);
void WebView2Container_SetContentCallback(WebView2ContentCallback callback, void* userData);
void WebView2Container_SetCursorCallback(WebView2CursorCallback callback, void* userData);
void WebView2Container_SetErrorCallback(WebView2ErrorCallback callback, void* userData);

#ifdef __cplusplus
}
#endif

#endif // WEBVIEW2CONTAINER_H

/// =============================================================================
/// LINKING REQUIREMENTS AND NOTES
/// =============================================================================
/// 
/// To build this WebView2 implementation, you need:
/// 
/// 1. WebView2 SDK:
///    - Download from: https://developer.microsoft.com/en-us/microsoft-edge/webview2/
///    - Or install via NuGet: Install-Package Microsoft.Web.WebView2
/// 
/// 2. Required libraries to link:
///    - WebView2Loader.dll.lib (or WebView2LoaderStatic.lib for static linking)
///    - ole32.lib
///    - oleaut32.lib
///    - user32.lib
///    - version.lib
/// 
/// 3. Preprocessor definitions:
///    - WIN32_LEAN_AND_MEAN (optional, for faster compilation)
/// 
/// 4. C++ Standard:
///    - Requires C++11 or later (/std:c++11 minimum)
/// 
/// 5. Example CMake configuration:
///    find_package(Microsoft.Web.WebView2 REQUIRED)
///    target_link_libraries(your_target PRIVATE Microsoft.Web.WebView2::WebView2)
/// 
/// 6. Example Visual Studio project settings:
///    - Additional Include Directories: $(WebView2)\include
///    - Additional Library Directories: $(WebView2)\lib
///    - Additional Dependencies: WebView2Loader.dll.lib
/// 
/// 7. Runtime requirements:
///    - WebView2Loader.dll must be accessible (in PATH or same directory)
///    - Microsoft Edge WebView2 Runtime must be installed on target machine
/// 
/// =============================================================================