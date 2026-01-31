#pragma once
#include <string>
#include <vector>
#include <windows.h>

// Unified AgentCommand
enum AgentCommand {
    AGENT_OPEN_FILE,
    AGENT_CLOSE_FILE,
    AGENT_INSERT_TEXT,
    AGENT_GET_TEXT,
    AGENT_SET_TEXT,
    AGENT_RUN_CMD,
    AGENT_LIST_TABS,
    AGENT_SAVE_FILE,
    AGENT_CHANGE_LAYOUT,
    AGENT_SHOW_DIALOG,
    AGENT_SELECT_TAB,
    AGENT_GET_LAYOUT,
    AGENT_SET_LAYOUT,
    AGENT_AUTOMATE_SELF_CONTAINED_BUILD
};

// Unified AgentContext
struct AgentContext {
    HWND hwnd;
    int mode = 0; // 0: control, 1: ask
};

// Editor/terminal API declarations for external linkage
struct OpenFileInfo { std::wstring path; };
extern std::vector<OpenFileInfo> gOpenFiles;
extern void LoadFileIntoEdit(HWND hEdit, const std::wstring& path);
extern void AppendEditorText(HWND hwnd, const std::wstring& text);
extern std::wstring GetEditorText(HWND hwnd);
extern void WriteToTerminal(const std::wstring& text);

void AgentHandleCommand(AgentContext* ctx, AgentCommand cmd, const std::wstring& payload);
void RunAgent(AgentContext* ctx);
