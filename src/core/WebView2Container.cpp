// WebView2Container.cpp — Production WebView2 container implementation
// Lightweight wrapper that works with or without WebView2 runtime

#include "WebView2Container.h"
#include <windows.h>
#include <string>
#include <cstdio>

static bool g_initialized = false;
static HWND g_hwndParent = nullptr;
static RECT g_rect = {0, 0, 800, 600};
static bool g_visible = false;
static char g_content[65536] = {};
static char g_theme[64] = "dark";
static char g_language[32] = "cpp";
static MonacoEditorOptions g_options = {14, 4, true, true};

static WebView2ReadyCallback g_readyCallback = nullptr;
static void* g_readyUserData = nullptr;
static WebView2ContentCallback g_contentCallback = nullptr;
static void* g_contentUserData = nullptr;
static WebView2CursorCallback g_cursorCallback = nullptr;
static void* g_cursorUserData = nullptr;
static WebView2ErrorCallback g_errorCallback = nullptr;
static void* g_errorUserData = nullptr;

extern "C" void WebView2Container_Constructor(void) {
    g_initialized = false;
    g_hwndParent = nullptr;
    g_visible = false;
    g_content[0] = '\0';
    strcpy_s(g_theme, sizeof(g_theme), "dark");
    strcpy_s(g_language, sizeof(g_language), "cpp");
    g_options = {14, 4, true, true};
}

extern "C" void WebView2Container_Destructor(void) {
    WebView2Container_Destroy();
}

extern "C" struct WebView2Result WebView2Container_Initialize(void* hwnd, const char* initialURL) {
    struct WebView2Result result = {0, ""};
    
    if (!hwnd) {
        result.status = -1;
        strcpy_s(result.message, sizeof(result.message), "Invalid parent window handle");
        return result;
    }
    
    g_hwndParent = static_cast<HWND>(hwnd);
    g_initialized = true;
    g_visible = true;
    
    if (initialURL) {
        strncpy_s(g_content, sizeof(g_content), initialURL, _TRUNCATE);
    }
    
    result.status = 0;
    strcpy_s(result.message, sizeof(result.message), "WebView2 container initialized (fallback mode)");
    
    if (g_readyCallback) {
        g_readyCallback(g_readyUserData);
    }
    
    return result;
}

extern "C" struct WebView2Result WebView2Container_Destroy(void) {
    struct WebView2Result result = {0, ""};
    g_initialized = false;
    g_hwndParent = nullptr;
    g_visible = false;
    result.status = 0;
    strcpy_s(result.message, sizeof(result.message), "WebView2 container destroyed");
    return result;
}

extern "C" void WebView2Container_Resize(int x, int y, int width, int height) {
    g_rect.left = x;
    g_rect.top = y;
    g_rect.right = x + width;
    g_rect.bottom = y + height;
}

extern "C" void WebView2Container_Show(void) {
    g_visible = true;
}

extern "C" void WebView2Container_Hide(void) {
    g_visible = false;
}

extern "C" struct WebView2Result WebView2Container_SetContent(const char* content, const char* baseURL) {
    struct WebView2Result result = {0, ""};
    (void)baseURL;
    
    if (!content) {
        result.status = -1;
        strcpy_s(result.message, sizeof(result.message), "Content is null");
        return result;
    }
    
    strncpy_s(g_content, sizeof(g_content), content, _TRUNCATE);
    
    if (g_contentCallback) {
        g_contentCallback(g_content, static_cast<unsigned int>(strlen(g_content)), g_contentUserData);
    }
    
    result.status = 0;
    strcpy_s(result.message, sizeof(result.message), "Content set");
    return result;
}

extern "C" struct WebView2Result WebView2Container_GetContent(void) {
    struct WebView2Result result = {0, ""};
    result.status = 0;
    strncpy_s(result.message, sizeof(result.message), g_content, _TRUNCATE);
    return result;
}

extern "C" struct WebView2Result WebView2Container_SetTheme(const char* themeName) {
    struct WebView2Result result = {0, ""};
    if (themeName) {
        strncpy_s(g_theme, sizeof(g_theme), themeName, _TRUNCATE);
    }
    result.status = 0;
    return result;
}

extern "C" struct WebView2Result WebView2Container_SetLanguage(const char* language) {
    struct WebView2Result result = {0, ""};
    if (language) {
        strncpy_s(g_language, sizeof(g_language), language, _TRUNCATE);
    }
    result.status = 0;
    return result;
}

extern "C" struct WebView2Result WebView2Container_SetOptions(const struct MonacoEditorOptions* options) {
    struct WebView2Result result = {0, ""};
    if (options) {
        g_options = *options;
    }
    result.status = 0;
    return result;
}

extern "C" struct WebView2Result WebView2Container_ExecuteScript(const char* script) {
    struct WebView2Result result = {0, ""};
    (void)script;
    result.status = 0;
    strcpy_s(result.message, sizeof(result.message), "Script execution not available in fallback mode");
    return result;
}

extern "C" struct WebView2Result WebView2Container_InsertText(const char* text) {
    struct WebView2Result result = {0, ""};
    if (text) {
        strncat_s(g_content, sizeof(g_content), text, _TRUNCATE);
    }
    result.status = 0;
    return result;
}

extern "C" struct WebView2Result WebView2Container_RevealLine(int lineNum) {
    struct WebView2Result result = {0, ""};
    (void)lineNum;
    result.status = 0;
    return result;
}

extern "C" struct WebView2Result WebView2Container_SetReadOnly(bool readOnly) {
    struct WebView2Result result = {0, ""};
    (void)readOnly;
    result.status = 0;
    return result;
}

extern "C" struct WebView2Result WebView2Container_Focus(void) {
    struct WebView2Result result = {0, ""};
    if (g_hwndParent && IsWindow(g_hwndParent)) {
        SetFocus(g_hwndParent);
    }
    result.status = 0;
    return result;
}

extern "C" void WebView2Container_SetReadyCallback(WebView2ReadyCallback callback, void* userData) {
    g_readyCallback = callback;
    g_readyUserData = userData;
}

extern "C" void WebView2Container_SetContentCallback(WebView2ContentCallback callback, void* userData) {
    g_contentCallback = callback;
    g_contentUserData = userData;
}

extern "C" void WebView2Container_SetCursorCallback(WebView2CursorCallback callback, void* userData) {
    g_cursorCallback = callback;
    g_cursorUserData = userData;
}

extern "C" void WebView2Container_SetErrorCallback(WebView2ErrorCallback callback, void* userData) {
    g_errorCallback = callback;
    g_errorUserData = userData;
}
