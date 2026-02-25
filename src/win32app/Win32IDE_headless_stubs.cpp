// =============================================================================
// Win32IDE_headless_stubs.cpp — functional headless implementations for
// Win32IDE methods used by non-GUI targets (RawrEngine/Gold variants).
// =============================================================================
#include "Win32IDE.h"
#include <windows.h>

#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct HeadlessState {
    std::mutex mtx;
    std::vector<std::string> outputTabs;
    std::vector<std::string> outputLines;
    std::vector<std::string> problems;
    std::unordered_map<uintptr_t, std::string> treeItems;
    std::atomic<uintptr_t> nextTreeId{1};
};

HeadlessState& state() {
    static HeadlessState s;
    return s;
}

static const char* severityToString(Win32IDE::OutputSeverity sev) {
    switch (sev) {
        case Win32IDE::OutputSeverity::Debug: return "DEBUG";
        case Win32IDE::OutputSeverity::Info: return "INFO";
        case Win32IDE::OutputSeverity::Warning: return "WARN";
        case Win32IDE::OutputSeverity::Error: return "ERROR";
        default: return "INFO";
    }
}

} // namespace

void Win32IDE::appendToOutput(const std::string& text, const std::string& channel, Win32IDE::OutputSeverity sev) {
    std::lock_guard<std::mutex> lock(state().mtx);

    std::ostringstream oss;
    oss << "[" << severityToString(sev) << "][" << (channel.empty() ? "Output" : channel) << "] " << text;
    state().outputLines.push_back(oss.str());

    // Reuse log sink for headless observability.
    logMessage(channel.empty() ? "Output" : channel, text);
}

void Win32IDE::onInferenceComplete(const std::string& response) {
    appendToOutput("[InferenceComplete] " + response + "\n", "AI", Win32IDE::OutputSeverity::Info);
}

void Win32IDE::addOutputTab(const std::string& tabName) {
    std::lock_guard<std::mutex> lock(state().mtx);
    if (tabName.empty()) return;
    for (const auto& t : state().outputTabs) {
        if (t == tabName) return;
    }
    state().outputTabs.push_back(tabName);
    logMessage("OutputTabs", "Added tab: " + tabName);
}

void Win32IDE::addProblem(const std::string& filePath, int line, int column, const std::string& message, int severity) {
    std::ostringstream oss;
    oss << filePath << ":" << line << ":" << column
        << " [sev=" << severity << "] " << message;
    {
        std::lock_guard<std::mutex> lock(state().mtx);
        state().problems.push_back(oss.str());
    }
    logMessage("Problems", oss.str());
}

HTREEITEM Win32IDE::addTreeItem(HTREEITEM hParent, const std::string& text, const std::string& fullPath, bool isDirectory) {
    (void)hParent;
    const uintptr_t id = state().nextTreeId.fetch_add(1);
    std::ostringstream oss;
    oss << (isDirectory ? "[D] " : "[F] ") << text;
    if (!fullPath.empty()) oss << " -> " << fullPath;
    {
        std::lock_guard<std::mutex> lock(state().mtx);
        state().treeItems[id] = oss.str();
    }
    return reinterpret_cast<HTREEITEM>(id);
}

void Win32IDE::HandleCopilotStreamUpdate(const char* chunk, size_t len) {
    if (!chunk || len == 0) return;
    std::string s(chunk, len);
    appendToOutput(s, "CopilotStream", Win32IDE::OutputSeverity::Info);
}
