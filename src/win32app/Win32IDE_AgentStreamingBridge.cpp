// Win32IDE_AgentStreamingBridge.cpp — C API for agent streaming → IDE output
// Routes AgentPanel_AppendMessage / AgentPanel_AppendToken to appendToOutput("Agent").

#include "Win32IDE.h"
#include <mutex>
#include <new>
#include <string>

namespace {

std::string wideToUtf8(const wchar_t* wstr) {
    if (!wstr) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// Safely post text to the Agent output tab from any thread.
// Heap-allocates a copy of text; the UI-thread handler (WM_IDE_OUTPUT_APPEND_SAFE) frees it.
void postAgentOutputSafe(std::string text) {
    if (!g_pMainIDE) return;
    HWND hwnd = g_pMainIDE->getMainWindow();
    if (!hwnd || !IsWindow(hwnd)) return;
    auto* payload = new (std::nothrow) std::string(std::move(text));
    if (!payload) return;
    if (!PostMessage(hwnd, WM_IDE_OUTPUT_APPEND_SAFE, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

std::mutex s_tokenBufMtx;
std::string s_tokenBuf;

} // namespace

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_AppendMessage(const wchar_t* role, const wchar_t* content) {
    std::string r = wideToUtf8(role);
    std::string c = wideToUtf8(content);
    if (r.empty() && c.empty()) return;
    if (r.empty()) r = "agent";
    std::string line = "[Agent] " + r + ": " + c;
    if (!line.empty() && line.back() != '\n') line += "\n";
    postAgentOutputSafe(std::move(line));
    if (g_pMainIDE) {
        g_pMainIDE->bridgeRecordSimpleEvent(AgentEventType::AgentCompleted, r + ": " + c.substr(0, 80));
    }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_AppendToken(const wchar_t* token) {
    if (!token) return;
    std::string t = wideToUtf8(token);
    std::string toFlush;
    {
        std::lock_guard<std::mutex> lk(s_tokenBufMtx);
        s_tokenBuf += t;
        if (!s_tokenBuf.empty() && (s_tokenBuf.back() == '\n' || s_tokenBuf.size() > 256)) {
            toFlush = std::move(s_tokenBuf);
            s_tokenBuf.clear();
        }
    }
    if (!toFlush.empty()) {
        postAgentOutputSafe(std::move(toFlush));
    }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_FinalizeStream(void) {
    std::string toFlush;
    {
        std::lock_guard<std::mutex> lk(s_tokenBufMtx);
        if (!s_tokenBuf.empty()) {
            toFlush = std::move(s_tokenBuf);
            s_tokenBuf.clear();
        }
    }
    if (!toFlush.empty()) {
        postAgentOutputSafe(std::move(toFlush));
    }
    postAgentOutputSafe("\n");
    if (g_pMainIDE) {
        HWND hwnd = g_pMainIDE->getMainWindow();
        if (hwnd && IsWindow(hwnd)) {
            PostMessage(hwnd, WM_APP + 112, 0, 0);
        }
    }
}

} // extern "C"
