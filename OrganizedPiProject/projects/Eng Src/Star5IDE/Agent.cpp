#include "Agent.h"
#include <windows.h>
#include <string>
#include <vector>

// Why: Add agent support for automating self-contained builds and interactive Ask mode.
// What: Add AGENT_AUTOMATE_SELF_CONTAINED_BUILD command and AgentMode to AgentContext.
// Outcome: Agent can automate build settings and prompt user for confirmation.

enum AgentMode {
    AGENT_MODE_CONTROL,
    AGENT_MODE_ASK
};

// Dummy implementations for IDE API
void LoadFileIntoEdit(HWND hEdit, const std::wstring& path) {
    // TODO: Implement file loading logic
}
void AppendEditorText(HWND hwnd, const std::wstring& text) {
    // TODO: Implement text append logic
}
std::wstring GetEditorText(HWND hwnd) {
    // TODO: Implement text retrieval logic
    return L"";
}
void SetEditorText(HWND hwnd, const std::wstring& text) {
    // TODO: Implement text set logic
}
void WriteToTerminal(const std::wstring& text) {
    // TODO: Implement terminal output logic
}
void SaveFile(HWND hwnd, const std::wstring& path) {
    // TODO: Implement file save logic
}
void ChangeLayout(const std::wstring& layoutName) {
    // TODO: Implement layout change logic
}
void ShowDialog(HWND hwnd, int dialogId) {
    // TODO: Implement dialog show logic
}
void SelectTab(HWND hwnd, int tabIndex) {
    // TODO: Implement tab selection logic
}
std::wstring GetCurrentLayout() {
    // TODO: Implement get layout logic
    return L"";
}
void SetCurrentLayout(const std::wstring& layoutName) {
    // TODO: Implement set layout logic
}

std::vector<OpenFileInfo> gOpenFiles;

void AutomateSelfContainedBuild(HWND hwnd) {
    // Why: Automate static linking for self-contained EXE builds.
    // What: Would update project settings to use /MT (stub here).
    // Outcome: Future builds will produce portable executables.
    MessageBox(hwnd, L"Automated self-contained build: Runtime Library set to /MT (stub)", L"Agent Build Automation", MB_OK);
}

void AgentHandleCommand(AgentContext* ctx, AgentCommand cmd, const std::wstring& payload) {
    if (ctx->mode == AGENT_MODE_ASK) {
        int res = MessageBox(ctx->hwnd, L"Agent asks: Perform this action?", L"Agent Ask", MB_YESNO);
        if (res != IDYES) return;
    }
    switch (cmd) {
        case AGENT_OPEN_FILE:
            LoadFileIntoEdit(GetDlgItem(ctx->hwnd, 0 /*IDC_MAIN_EDIT*/), payload);
            break;
        case AGENT_INSERT_TEXT:
            AppendEditorText(ctx->hwnd, payload);
            break;
        case AGENT_GET_TEXT: {
            std::wstring txt = GetEditorText(ctx->hwnd);
            MessageBox(ctx->hwnd, txt.c_str(), L"Agent ? Editor Text", MB_OK);
            break;
        }
        case AGENT_RUN_CMD:
            WriteToTerminal(payload + L"\n");
            break;
        case AGENT_LIST_TABS: {
            std::wstring files;
            for (auto& f : gOpenFiles) files += f.path + L"\n";
            MessageBox(ctx->hwnd, files.c_str(), L"Agent ? Open Tabs", MB_OK);
            break;
        }
        case AGENT_AUTOMATE_SELF_CONTAINED_BUILD:
            AutomateSelfContainedBuild(ctx->hwnd);
            break;
        // ...repeat for other commands as needed...
    }
}

void RunAgent(AgentContext* ctx) {
    // Example: open a file, inject code, run a command, automate build
    AgentHandleCommand(ctx, AGENT_OPEN_FILE, L"C:\\temp\\demo.cpp");
    AgentHandleCommand(ctx, AGENT_INSERT_TEXT, L"\n// Added by Agent\n");
    AgentHandleCommand(ctx, AGENT_RUN_CMD, L"dir");
    AgentHandleCommand(ctx, AGENT_AUTOMATE_SELF_CONTAINED_BUILD, L"");
}
