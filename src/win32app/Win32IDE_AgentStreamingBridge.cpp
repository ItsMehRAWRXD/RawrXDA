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

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_AppendMessage(const wchar_t* role, const wchar_t* content) {
    std::string r = wideToUtf8(role);
    std::string c = wideToUtf8(content);
    if (r.empty() && c.empty()) return;
    std::string line = "[Agent] ";
    if (!r.empty()) line += r + ": ";
    line += c;
    if (!line.empty() && line.back() != '\n') line += "\n";
    appendAgentOutput(line);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_AppendToken(const wchar_t* token) {
    if (!token) return;
    appendAgentOutput(wideToUtf8(token));
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void AgentPanel_FinalizeStream(void) {
    appendAgentOutput("\n");
}

} // extern "C"
