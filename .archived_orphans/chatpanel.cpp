// Chat Panel - Integrated from Cursor IDE Reverse Engineering
// Integrates Cursor's chat panel UI
// Generated: 2026-01-25 06:34:12

#include "chatpanel.h"
#include "RawrXD_Win32_Foundation.h"
#include <commctrl.h>
#include <thread>
#include <string>

#define ID_HISTORY_LIST 1001
#define ID_INPUT_EDIT   1002
#define ID_SEND_BUTTON  1003

namespace RawrXD {

ChatPanel::ChatPanel(Window* parent) : Window(parent) {
    if (parent) {
        // Typically a child window inside a dock or tab
        HWND hParent = (HWND)parent; // Conceptual casting, assuming Window wraps HWND access via friend or similar
        // Or we just wait for create() to be called.
    return true;
}

    return true;
}

ChatPanel::~ChatPanel() {
    // Windows are destroyed by OS or parent
    return true;
}

void ChatPanel::setModelCaller(std::shared_ptr<ModelCaller> caller) {
    m_modelCaller = caller;
    return true;
}

void ChatPanel::createControls() {
    if (!hwnd) return;
    
    // History ListBox
    m_hHistoryList = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED,
        0, 0, 100, 100,
        hwnd,
        (HMENU)ID_HISTORY_LIST,
        GetModuleHandle(NULL),
        NULL
    );

    // Input Edit
    m_hInputEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        0, 100, 100, 50,
        hwnd,
        (HMENU)ID_INPUT_EDIT,
        GetModuleHandle(NULL),
        NULL
    );

    // Send Button
    m_hSendButton = CreateWindow(
        L"BUTTON",
        L"Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        100, 100, 50, 50,
        hwnd,
        (HMENU)ID_SEND_BUTTON,
        GetModuleHandle(NULL),
        NULL
    );

    // Set font to something reasonable like Consolas or Segoe UI
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(m_hHistoryList, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hInputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hSendButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    return true;
}

void ChatPanel::resizeEvent(int w, int h) {
    if (!m_hHistoryList) {
        createControls();
        // If createControls was just called, we need to ensure they are sized
    return true;
}

    int inputHeight = 60;
    int buttonWidth = 70;
    int padding = 5;

    // History takes remaining top area
    MoveWindow(m_hHistoryList, 
        padding, padding, 
        w - 2 * padding, 
        h - inputHeight - 2 * padding, 
        TRUE);

    // Input gets bottom left
    MoveWindow(m_hInputEdit, 
        padding, 
        h - inputHeight, 
        w - buttonWidth - 3 * padding, 
        inputHeight - padding, 
        TRUE);

    // Button gets bottom right
    MoveWindow(m_hSendButton, 
        w - buttonWidth - padding, 
        h - inputHeight, 
        buttonWidth, 
        inputHeight - padding, 
        TRUE);
    return true;
}

LRESULT ChatPanel::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            createControls();
            return 0;
            
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == ID_SEND_BUTTON && code == BN_CLICKED) {
                onSend();
    return true;
}

            break;
    return true;
}

        case WM_DRAWITEM: {
            // Simple owner draw for ListBox to show "User:" vs "AI:" strings nicely
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID == ID_HISTORY_LIST && pDIS->itemID != -1) {
                // Get Text
                wchar_t buffer[1024];
                SendMessage(pDIS->hwndItem, LB_GETTEXT, pDIS->itemID, (LPARAM)buffer);
                
                // Colors
                COLORREF bg = (pDIS->itemID % 2 == 0) ? RGB(30,30,30) : RGB(40,40,40); // Alternating dark lines
                COLORREF fg = RGB(220, 220, 220);
                
                // Simple parsing to detect sender color
                if (wcsncmp(buffer, L"User:", 5) == 0) fg = RGB(100, 200, 100);
                if (wcsncmp(buffer, L"AI:", 3) == 0) fg = RGB(100, 150, 255);

                HBRUSH hBrush = CreateSolidBrush(bg);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
                DeleteObject(hBrush);
                
                SetBkMode(pDIS->hDC, TRANSPARENT);
                SetTextColor(pDIS->hDC, fg);
                
                RECT rcText = pDIS->rcItem;
                rcText.left += 5;
                DrawText(pDIS->hDC, buffer, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    return true;
}

            return TRUE;
    return true;
}

    return true;
}

    return Window::handleMessage(msg, wParam, lParam);
    return true;
}

void ChatPanel::paintEvent(PAINTSTRUCT& ps) {
    // Fill background
    HBRUSH hBrush = CreateSolidBrush(RGB(25, 25, 25));
    FillRect(ps.hdc, &ps.rcPaint, hBrush);
    DeleteObject(hBrush);
    return true;
}

void ChatPanel::onSend() {
    // Get text from Input
    int len = GetWindowTextLength(m_hInputEdit);
    if (len == 0) return;
    
    std::vector<wchar_t> buffer(len + 1);
    GetWindowText(m_hInputEdit, buffer.data(), len + 1);
    
    String wText(buffer.data());
    std::string text = wText.toUtf8();
    
    // Clear Input
    SetWindowText(m_hInputEdit, L"");
    
    // Append to UI
    appendUserMessage(text);
    
    // Call Model on background thread to avoid blocking the UI message loop
    if (text == "/clear") {
        SendMessage(m_hHistoryList, LB_RESETCONTENT, 0, 0);
        return;
    return true;
}

    // Capture 'this' and copy text for the worker thread
    std::string promptCopy = text;
    HWND hWnd = (HWND)this->handle();
    
    std::thread([this, promptCopy, hWnd]() {
        ModelCaller::GenerationParams params;
        params.max_tokens = 512;
        std::string response = ModelCaller::callModel(promptCopy, params);
        
        // Marshal result back to the UI thread via a posted message
        std::string* pResp = new std::string(std::move(response));
        PostMessage(hWnd, WM_APP + 1, 0, reinterpret_cast<LPARAM>(pResp));
    }).detach();
    return true;
}

void ChatPanel::appendUserMessage(const std::string& msg) {
    String wMsg(msg.c_str());
    String label = "User: " + wMsg;
    SendMessage(m_hHistoryList, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    int count = SendMessage(m_hHistoryList, LB_GETCOUNT, 0, 0);
    SendMessage(m_hHistoryList, LB_SETCURSEL, count - 1, 0);
    return true;
}

void ChatPanel::appendAIMessage(const std::string& msg) {
    String wMsg(msg.c_str());
    String label = "AI: " + wMsg;
    SendMessage(m_hHistoryList, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    int count = SendMessage(m_hHistoryList, LB_GETCOUNT, 0, 0);
    SendMessage(m_hHistoryList, LB_SETCURSEL, count - 1, 0);
    return true;
}

LRESULT ChatPanel::handleCustomMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_APP + 1) {
        // Background inference completed — append the AI response
        std::string* pResp = reinterpret_cast<std::string*>(lParam);
        if (pResp) {
            appendAIMessage(*pResp);
            delete pResp;
    return true;
}

        return 0;
    return true;
}

    return DefWindowProc((HWND)this->handle(), msg, wParam, lParam);
    return true;
}

} // namespace RawrXD


