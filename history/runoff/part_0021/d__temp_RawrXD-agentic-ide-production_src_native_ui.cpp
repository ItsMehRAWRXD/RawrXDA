#include "native_ui.h"
#include "native_widgets.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <windows.h>

#ifdef _WIN32
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    NativeWindow* window = reinterpret_cast<NativeWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (uMsg) {
        case WM_CLOSE:
            if (window) window->invokeClose();
            return 0;
        case WM_SIZE:
            if (window) window->invokeResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
#endif

NativeWindow::NativeWindow() : m_onClose(nullptr), m_onResize(nullptr) {
#ifdef _WIN32
    m_hwnd = nullptr;
#endif
}

NativeWindow::~NativeWindow() {
    close();
}

bool NativeWindow::create(const std::string& title, int width, int height) {
#ifdef _WIN32
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "NativeWindow";
    
    RegisterClass(&wc);
    
    m_hwnd = CreateWindowEx(
        0,
        "NativeWindow",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (m_hwnd) {
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }
#endif
    return false;
}

void NativeWindow::show() {
#ifdef _WIN32
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
#endif
}

void NativeWindow::hide() {
#ifdef _WIN32
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
#endif
}

void NativeWindow::close() {
#ifdef _WIN32
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
#endif
}

void NativeWindow::setOnClose(std::function<void()> callback) {
    m_onClose = std::move(callback);
}

void NativeWindow::setOnResize(std::function<void(int, int)> callback) {
    m_onResize = std::move(callback);
}

void NativeWindow::invokeClose() {
    if (m_onClose) m_onClose();
}

void NativeWindow::invokeResize(int width, int height) {
    if (m_onResize) m_onResize(width, height);
}

// NativeButton and NativeComboBox implementations live in native_widgets.cpp to avoid
// duplicate Win32 control definitions. This file focuses on the simple helper wrappers.

// ---------------------------------------------------------------------------
// Global UI state for the simple native helper API
// ---------------------------------------------------------------------------
extern NativeWindow g_mainWindow; // defined in native_ui.cpp
static std::vector<NativeComboBox*> g_combos;
static std::vector<NativeButton*> g_buttons;

static std::unordered_map<std::string, std::string> g_comboValueToLabel;

// Global handles for chat view and input field (Win32 edit controls)
static HWND g_chatView = nullptr;
static HWND g_inputField = nullptr;

// ---------------------------------------------------------------------------
// Helper functions – these are the C‑style API declared in native_ui.h.
// ---------------------------------------------------------------------------

void* Native_CreateCombo() {
    auto* combo = new NativeComboBox();
    g_combos.push_back(combo);
    return static_cast<void*>(combo);
}

void Native_SetComboMinWidth(void* /*combo*/, int /*width*/) {
    // No‑op in this minimal implementation.
}

void Native_SetComboCallback(void* combo, std::function<void(int, const std::string&)> cb) {
    if (!combo) return;
    auto* c = static_cast<NativeComboBox*>(combo);
    // Wrap the callback to provide an empty string for the value (value can be
    // retrieved via Native_GetComboValueAt).
    c->setOnChange([cb](int index) {
        cb(index, "");
    });
}

void Native_AddComboItem(void* combo, const std::string& label, int /*value*/) {
    if (!combo) return;
    auto* c = static_cast<NativeComboBox*>(combo);
    c->addItem(label);
    // Store mapping for later retrieval.
    g_comboValueToLabel[label] = label;
}

void Native_AddComboItemByValue(const std::string& value, const std::string& label) {
    g_comboValueToLabel[value] = label;
}

void Native_RemoveComboItemByValue(const std::string& value) {
    g_comboValueToLabel.erase(value);
}

std::string Native_GetComboValueAt(int index) {
    if (g_combos.empty()) return {};
    // Retrieve the nth entry from the map (order is unspecified but sufficient).
    int i = 0;
    for (const auto& kv : g_comboValueToLabel) {
        if (i == index) return kv.second;
        ++i;
    }
    return {};
}

void Native_SelectComboItemByValue(const std::string& /*value*/) {
    // No visual state tracking in this stub implementation.
}

// ---------------------------------------------------------------------------
// Button helpers
// ---------------------------------------------------------------------------
void* Native_CreateButton(const std::string& text) {
    auto* btn = new NativeButton();
    g_buttons.push_back(btn);
    btn->setText(text);
    return static_cast<void*>(btn);
}

void Native_SetButtonCallback(void* btn, std::function<void()> cb) {
    if (!btn) return;
    static_cast<NativeButton*>(btn)->setOnClick(std::move(cb));
}

void Native_SetButtonToggle(void* /*btn*/, bool /*toggle*/) {
    // No visual toggle in console mode.
}

void Native_SetButtonChecked(const std::string&, bool) {}
void Native_SetButtonText(const std::string&, const std::string&) {}

// ---------------------------------------------------------------------------
// Agent list helpers – simple console output for now.
// ---------------------------------------------------------------------------
void Native_AddAgentListItem(const std::string& text) {
    std::cout << "[Agent] " << text << std::endl;
}

void Native_RemoveAgentListItemByValue(const std::string& value) {
    std::cout << "[Agent] removed: " << value << std::endl;
}

void Native_SelectAgentListRowByValue(const std::string& value) {
    std::cout << "[Agent] selected: " << value << std::endl;
}

void Native_SelectAgentListRowByIndex(int index) {
    std::cout << "[Agent] selected index: " << index << std::endl;
}

// ---------------------------------------------------------------------------
// Global window instance definition (single definition for the extern declared above)
NativeWindow g_mainWindow;

// ---------------------------------------------------------------------------
// Input helpers – simple static buffer.
// ---------------------------------------------------------------------------
static std::string g_inputBuffer;

// ---------------------------------------------------------------------------
// Input helpers – use Win32 edit control when available, otherwise fallback to
// an internal buffer.
// ---------------------------------------------------------------------------
std::string Native_GetInputText()
{
    if (g_inputField) {
        wchar_t buf[1024];
        GetWindowTextW(g_inputField, buf, 1024);
        std::wstring ws(buf);
        return std::string(ws.begin(), ws.end());
    }
    return g_inputBuffer;
}

void Native_ClearInputText()
{
    if (g_inputField) {
        SetWindowTextW(g_inputField, L"");
    }
    g_inputBuffer.clear();
}

void Native_SetInputText(const std::string& text)
{
    if (g_inputField) {
        std::wstring ws(text.begin(), text.end());
        SetWindowTextW(g_inputField, ws.c_str());
    }
    g_inputBuffer = text;
}


// ---------------------------------------------------------------------------
// Additional UI element creation for the Win32 version.
// ---------------------------------------------------------------------------

void* Native_CreateChatView()
{
    if (!g_mainWindow.getHandle()) return nullptr;
    HWND parent = g_mainWindow.getHandle();
    HWND hEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        10, 10, 560, 400,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr);
    g_chatView = hEdit;
    return static_cast<void*>(hEdit);
}

void* Native_CreateInputField()
{
    if (!g_mainWindow.getHandle()) return nullptr;
    HWND parent = g_mainWindow.getHandle();
    HWND hEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
        10, 420, 560, 24,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr);
    if (hEdit) {
        g_inputField = hEdit;
        SetFocus(hEdit); // ensure the text box receives keyboard input immediately
    }
    return static_cast<void*>(hEdit);
}

// Update chat helpers to use the edit control when available.

void Native_ClearChat()
{
    if (g_chatView) {
        SetWindowTextW(g_chatView, L"");
    } else {
        std::cout << "--- Chat cleared ---" << std::endl;
    }
}

void Native_AppendChatMessage(const std::string& sender, const std::string& message,
                              const std::string& timestamp, bool isAgent)
{
    if (g_chatView) {
        std::string line = "[" + timestamp + "] " + (isAgent ? "*" : "") + sender + ": " + message + "\r\n";
        int len = GetWindowTextLength(g_chatView);
        SendMessage(g_chatView, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessage(g_chatView, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    } else {
        std::cout << "[" << timestamp << "] " << (isAgent ? "*" : "") << sender << ": " << message << std::endl;
    }
}

