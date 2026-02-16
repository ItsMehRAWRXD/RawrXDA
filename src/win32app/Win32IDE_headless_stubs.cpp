// =============================================================================
// Win32IDE_headless_stubs.cpp — No-op implementations for RawrEngine / Gold
// when linking vscode_extension_api.cpp without the full Win32 IDE GUI.
// Resolves LNK2019 for: appendToOutput, onInferenceComplete, addOutputTab,
// addProblem, addTreeItem, HandleCopilotStreamUpdate.
// =============================================================================
#include "Win32IDE.h"
#include <windows.h>

void Win32IDE::appendToOutput(const std::string&, const std::string&, OutputSeverity) {
    // No-op: headless build has no output panel
}

void Win32IDE::onInferenceComplete(const std::string&) {
    // No-op: headless build has no chat UI
}

void Win32IDE::addOutputTab(const std::string&) {
    // No-op: headless build has no output tabs
}

void Win32IDE::addProblem(const std::string&, int, int, const std::string&, int) {
    // No-op: headless build has no problems panel
}

HTREEITEM Win32IDE::addTreeItem(HTREEITEM, const std::string&, const std::string&, bool) {
    return nullptr;
}

void Win32IDE::HandleCopilotStreamUpdate(const char*, size_t) {
    // No-op: headless build has no copilot panel
}
