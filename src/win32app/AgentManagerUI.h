#pragma once
// AgentManagerUI.h — Win32 multi-agent dashboard panel
// ListView showing active agent sessions with Start/Stop controls.
// No Qt. No exceptions.

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

struct AgentRecord {
    uint32_t    id;
    std::string name;       // e.g. "Agent-0"
    std::string model;      // e.g. "codestral:22b"
    std::string status;     // "idle" | "running" | "error"
    uint32_t    toolCalls;
    uint64_t    tokensTotal;
};

class AgentManagerUI {
public:
    static AgentManagerUI& instance();

    // Create the panel as a child window inside hwndParent.
    bool create(HWND hwndParent, int x, int y, int w, int h);

    // Destroy the panel.
    void destroy();

    // Refresh the ListView from current agent state.
    void refresh(const std::vector<AgentRecord>& agents);

    // Append a log line to the log edit.
    void appendLog(const std::string& line);

    // Handle WM_COMMAND from parent (button clicks).
    bool handleCommand(WPARAM wParam, LPARAM lParam);

    // Handle WM_SIZE to reflow child controls.
    void resize(int x, int y, int w, int h);

    bool isCreated() const { return m_hwndPanel != nullptr; }
    HWND hwnd() const      { return m_hwndPanel; }

private:
    AgentManagerUI() = default;
    ~AgentManagerUI() = default;
    AgentManagerUI(const AgentManagerUI&) = delete;
    AgentManagerUI& operator=(const AgentManagerUI&) = delete;

    void populateListView(const std::vector<AgentRecord>& agents);

    HWND m_hwndPanel    = nullptr;
    HWND m_hwndList     = nullptr;  // SysListView32
    HWND m_hwndLog      = nullptr;  // EDIT (multiline)
    HWND m_hwndBtnStart = nullptr;
    HWND m_hwndBtnStop  = nullptr;

    static constexpr int ID_BTN_START = 1001;
    static constexpr int ID_BTN_STOP  = 1002;
    static constexpr int ID_LIST      = 1003;
    static constexpr int ID_LOG       = 1004;
};

} // namespace RawrXD
