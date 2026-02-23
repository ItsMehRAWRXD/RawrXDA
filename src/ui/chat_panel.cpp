#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ui/chat_panel.h"
#include "ui/tool_action_status.h"
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace UI {

bool ChatPanel::create(HWND parent, int idBase) {
    m_container = CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE, 0,0,10,10, parent, (HMENU)(intptr_t)(idBase), GetModuleHandle(NULL), NULL);
    if (!m_container) return false;
    m_transcript = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
                                   0,0,10,10, m_container, (HMENU)(intptr_t)(idBase+1), GetModuleHandle(NULL), NULL);
    m_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
                              0,0,10,10, m_container, (HMENU)(intptr_t)(idBase+2), GetModuleHandle(NULL), NULL);
    m_send = CreateWindowExW(0, L"BUTTON", L"Send", WS_CHILD|WS_VISIBLE,
                             0,0,80,24, m_container, (HMENU)(intptr_t)(idBase+3), GetModuleHandle(NULL), NULL);
    return m_transcript && m_input && m_send;
}

void ChatPanel::resize(int x, int y, int w, int h) {
    if (!m_container) return;
    MoveWindow(m_container, x, y, w, h, TRUE);
    int pad = 6;
    int transcriptH = h - 80;
    MoveWindow(m_transcript, pad, pad, w - 2*pad, transcriptH, TRUE);
    MoveWindow(m_input, pad, pad + transcriptH + pad, w - 2*pad - 90, 60, TRUE);
    MoveWindow(m_send, w - pad - 80, pad + transcriptH + pad + 18, 80, 24, TRUE);
}

void ChatPanel::appendMessage(const std::string& who, const std::string& text) {
    if (!m_transcript) return;
    // Convert to wide
    std::wstring wwho(who.begin(), who.end());
    std::wstring wtxt(text.begin(), text.end());
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring line = wwho + L": " + wtxt + L"\r\n";
    SendMessageW(m_transcript, EM_REPLACESEL, TRUE, (LPARAM)line.c_str());
}

std::string ChatPanel::getInput() const {
    if (!m_input) return {};
    int len = GetWindowTextLengthW(m_input);
    if (len <= 0) return {};
    std::wstring buf(len + 1, L'\0');
    GetWindowTextW(m_input, buf.data(), (int)buf.size());
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return {};
    std::string result(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), -1, result.data(), utf8Len, nullptr, nullptr);
    result.resize(utf8Len - 1);  // Exclude null terminator
    return result;
}

void ChatPanel::clearInput() {
    if (m_input) SetWindowTextW(m_input, L"");
}

void ChatPanel::appendToolAction(const ToolActionStatus& action) {
    if (!m_transcript) return;
    std::string text = ToolActionStatusFormatter::formatPlainText(action);
    std::wstring wtext(text.begin(), text.end());
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring line = L"  " + wtext;
    SendMessageW(m_transcript, EM_REPLACESEL, TRUE, (LPARAM)line.c_str());
}

void ChatPanel::appendWorkingBubble(const WorkingBubble& bubble) {
    if (!m_transcript) return;
    std::string text = ToolActionStatusFormatter::formatWorkingBubblePlainText(bubble);
    std::wstring wtext(text.begin(), text.end());
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(m_transcript, EM_REPLACESEL, TRUE, (LPARAM)wtext.c_str());
}

} // namespace UI
} // namespace RawrXD
