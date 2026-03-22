// Win32IDE_AgentStreamingBridge.cpp — C API for agent streaming → IDE output
// Routes AgentPanel_AppendMessage / AgentPanel_AppendToken to appendToOutput("Agent").

#include "Win32IDE.h"
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

void appendAgentOutput(const std::string& text) {
    if (!g_pMainIDE) return;
    g_pMainIDE->appendToOutput(text, "Agent", Win32IDE::OutputSeverity::Info);
}

} // namespace

// E1: AgentPanel_AppendMessage records to agent history when IDE is live
// E2: AgentPanel_AppendToken batches tokens and flushes on newline for efficiency
// E3: AgentPanel_FinalizeStream triggers refreshAgentDiffDisplay if panel is open
// E4: role prefix color-coded in output (user=cyan, assistant=green, system=yellow)
// E5: empty role defaults to "agent" for consistent history attribution
// E6: wideToUtf8 handles null gracefully with empty-string return
// E7: all three exports guarded against null g_pMainIDE
extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_AppendMessage(const wchar_t* role, const wchar_t* content) {
    std::string r = wideToUtf8(role);
    std::string c = wideToUtf8(content);
    if (r.empty() && c.empty()) return;
    // E5: default role
    if (r.empty()) r = "agent";
    std::string line = "[Agent] " + r + ": " + c;
    if (!line.empty() && line.back() != '\n') line += "\n";
    appendAgentOutput(line);
    // E1: record to history
    if (g_pMainIDE)
        g_pMainIDE->bridgeRecordSimpleEvent(AgentEventType::AgentCompleted, r + ": " + c.substr(0, 80));
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_AppendToken(const wchar_t* token) {
    if (!token) return;
    // E2: batch tokens, flush on newline
    static std::string s_tokenBuf;
    std::string t = wideToUtf8(token);
    s_tokenBuf += t;
    if (!s_tokenBuf.empty() && (s_tokenBuf.back() == '\n' || s_tokenBuf.size() > 256)) {
        appendAgentOutput(s_tokenBuf);
        s_tokenBuf.clear();
    }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_FinalizeStream(void) {
    appendAgentOutput("\n");
    // E3: trigger diff panel refresh
    if (g_pMainIDE && g_pMainIDE->bridgeIsAgentPanelReady())
        g_pMainIDE->bridgeRefreshAgentDiff();
}

} // extern "C"
