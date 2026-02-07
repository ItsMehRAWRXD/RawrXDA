// ============================================================================
// Win32IDE_AgentHistory.cpp — Persisted Agent History + Replay
// ============================================================================
// Append-only JSONL event log for all agent interactions:
//   - Every agent command, subagent spawn, chain step, swarm task, tool call,
//     failure detection/correction, ghost text, plan step, todo update, and
//     session lifecycle event is recorded.
//   - Events are buffered in memory (ring buffer, max 1000) and periodically
//     flushed to %APPDATA%\RawrXD\agent_history.jsonl.
//   - Replay loads a session's events and re-executes them for debugging.
//   - History panel (Win32 listview + detail pane) shows the timeline.
//   - Pruning keeps the log bounded (default: 30 days, 10 MB).
//
// Event schema mirrors subagent_core.h types for chain/swarm awareness.
// All timestamps are epoch milliseconds (UTC).
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <commctrl.h>
#include <set>

// ============================================================================
// AgentEvent — SERIALIZATION
// ============================================================================

std::string AgentEvent::typeString() const {
    switch (type) {
        case AgentEventType::AgentStarted:          return "AgentStarted";
        case AgentEventType::AgentCompleted:        return "AgentCompleted";
        case AgentEventType::AgentFailed:           return "AgentFailed";
        case AgentEventType::SubAgentSpawned:       return "SubAgentSpawned";
        case AgentEventType::SubAgentResult:        return "SubAgentResult";
        case AgentEventType::ChainStepStarted:      return "ChainStepStarted";
        case AgentEventType::ChainStepCompleted:    return "ChainStepCompleted";
        case AgentEventType::SwarmStarted:          return "SwarmStarted";
        case AgentEventType::SwarmTaskCompleted:     return "SwarmTaskCompleted";
        case AgentEventType::SwarmMerged:           return "SwarmMerged";
        case AgentEventType::ToolInvoked:           return "ToolInvoked";
        case AgentEventType::TodoUpdated:           return "TodoUpdated";
        case AgentEventType::PlanGenerated:         return "PlanGenerated";
        case AgentEventType::PlanStepExecuted:      return "PlanStepExecuted";
        case AgentEventType::FailureDetected:       return "FailureDetected";
        case AgentEventType::FailureCorrected:      return "FailureCorrected";
        case AgentEventType::GhostTextRequested:    return "GhostTextRequested";
        case AgentEventType::GhostTextAccepted:     return "GhostTextAccepted";
        case AgentEventType::SettingsChanged:       return "SettingsChanged";
        case AgentEventType::SessionEvent:          return "SessionEvent";
        default:                                    return "Unknown";
    }
}

// Escape a string for JSON embedding (handles quotes, backslashes, newlines)
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string AgentEvent::toJSONL() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"" << typeString() << "\"";
    oss << ",\"ts\":" << timestampMs;
    oss << ",\"session\":\"" << jsonEscape(sessionId) << "\"";
    if (!parentId.empty())
        oss << ",\"parent\":\"" << jsonEscape(parentId) << "\"";
    if (!agentId.empty())
        oss << ",\"agent\":\"" << jsonEscape(agentId) << "\"";
    if (!prompt.empty())
        oss << ",\"prompt\":\"" << jsonEscape(prompt) << "\"";
    if (!result.empty())
        oss << ",\"result\":\"" << jsonEscape(result) << "\"";
    if (!metadata.empty())
        oss << ",\"meta\":" << metadata;  // Already JSON — emit raw
    if (durationMs > 0)
        oss << ",\"dur\":" << durationMs;
    oss << ",\"ok\":" << (success ? "true" : "false");
    oss << "}";
    return oss.str();
}

AgentEvent AgentEvent::fromJSONL(const std::string& line) {
    AgentEvent ev;
    ev.timestampMs = 0;
    ev.durationMs  = 0;
    ev.success     = false;
    ev.type        = AgentEventType::SessionEvent;

    try {
        nlohmann::json j = nlohmann::json::parse(line);

        std::string typeStr = j.value("type", std::string("Unknown"));

        // Map type string back to enum
        static const struct { const char* name; AgentEventType type; } typeMap[] = {
            {"AgentStarted",       AgentEventType::AgentStarted},
            {"AgentCompleted",     AgentEventType::AgentCompleted},
            {"AgentFailed",        AgentEventType::AgentFailed},
            {"SubAgentSpawned",    AgentEventType::SubAgentSpawned},
            {"SubAgentResult",     AgentEventType::SubAgentResult},
            {"ChainStepStarted",   AgentEventType::ChainStepStarted},
            {"ChainStepCompleted", AgentEventType::ChainStepCompleted},
            {"SwarmStarted",       AgentEventType::SwarmStarted},
            {"SwarmTaskCompleted", AgentEventType::SwarmTaskCompleted},
            {"SwarmMerged",        AgentEventType::SwarmMerged},
            {"ToolInvoked",        AgentEventType::ToolInvoked},
            {"TodoUpdated",        AgentEventType::TodoUpdated},
            {"PlanGenerated",      AgentEventType::PlanGenerated},
            {"PlanStepExecuted",   AgentEventType::PlanStepExecuted},
            {"FailureDetected",    AgentEventType::FailureDetected},
            {"FailureCorrected",   AgentEventType::FailureCorrected},
            {"GhostTextRequested", AgentEventType::GhostTextRequested},
            {"GhostTextAccepted",  AgentEventType::GhostTextAccepted},
            {"SettingsChanged",    AgentEventType::SettingsChanged},
            {"SessionEvent",       AgentEventType::SessionEvent},
            {nullptr, AgentEventType::SessionEvent}
        };
        for (int i = 0; typeMap[i].name; i++) {
            if (typeStr == typeMap[i].name) {
                ev.type = typeMap[i].type;
                break;
            }
        }

        // Use int cast for uint64_t fields (our nlohmann::json is minimal)
        if (j.contains("ts")) {
            // timestampMs may exceed int range — parse as two halves if needed
            // For our minimal json: value<int> is safe up to ~2B; use string fallback
            ev.timestampMs = (uint64_t)(unsigned int)j.value("ts", (int)0);
        }

        ev.sessionId  = j.value("session", std::string(""));
        ev.parentId   = j.value("parent", std::string(""));
        ev.agentId    = j.value("agent", std::string(""));
        ev.prompt     = j.value("prompt", std::string(""));
        ev.result     = j.value("result", std::string(""));
        ev.durationMs = j.value("dur", (int)0);

        if (j.contains("meta")) {
            // Metadata is stored as raw JSON — re-serialize
            ev.metadata = j["meta"].dump(0);
        }

        if (j.contains("ok")) {
            // ok is a bool, but our minimal json returns it in value<int> via truthiness
            std::string okStr = j.value("ok", std::string("false"));
            ev.success = (okStr == "true" || okStr == "1");
        }

    } catch (const std::exception& e) {
        LOG_WARNING("Failed to parse agent history event: " + std::string(e.what()));
        ev.result = "Parse error: " + std::string(e.what());
    }

    return ev;
}

// ============================================================================
// INITIALIZATION / SHUTDOWN
// ============================================================================

void Win32IDE::initAgentHistory() {
    m_agentHistoryEnabled = true;
    m_currentSessionId    = generateSessionId();
    m_historyStats        = {};
    m_eventBuffer.clear();
    m_eventBuffer.reserve(MAX_EVENT_BUFFER);

    // Record session start
    recordSimpleEvent(AgentEventType::SessionEvent, "IDE session started");

    LOG_INFO("Agent history initialized (session=" + m_currentSessionId + ")");
}

void Win32IDE::shutdownAgentHistory() {
    if (!m_agentHistoryEnabled) return;

    // Record session end
    recordSimpleEvent(AgentEventType::SessionEvent, "IDE session ended");

    // Flush all buffered events to disk
    flushEventLog();

    LOG_INFO("Agent history shut down (events=" +
             std::to_string(m_historyStats.totalEvents) + ")");
}

// ============================================================================
// EVENT RECORDING — the core instrumentation API
// ============================================================================

void Win32IDE::recordEvent(AgentEventType type, const std::string& agentId,
                            const std::string& prompt, const std::string& result,
                            int durationMs, bool success,
                            const std::string& parentId,
                            const std::string& metadata) {
    if (!m_agentHistoryEnabled) return;

    AgentEvent ev;
    ev.type        = type;
    ev.timestampMs = currentEpochMs();
    ev.sessionId   = m_currentSessionId;
    ev.parentId    = parentId;
    ev.agentId     = agentId;
    ev.prompt      = truncateForLog(prompt);
    ev.result      = truncateForLog(result);
    ev.metadata    = metadata;
    ev.durationMs  = durationMs;
    ev.success     = success;

    // Update statistics
    m_historyStats.totalEvents++;
    m_historyStats.totalDurationMs += durationMs;

    switch (type) {
        case AgentEventType::AgentStarted:       m_historyStats.agentStarted++;      break;
        case AgentEventType::AgentCompleted:      m_historyStats.agentCompleted++;     break;
        case AgentEventType::AgentFailed:         m_historyStats.agentFailed++;        break;
        case AgentEventType::SubAgentSpawned:     m_historyStats.subAgentSpawned++;    break;
        case AgentEventType::ChainStepStarted:
        case AgentEventType::ChainStepCompleted:  m_historyStats.chainSteps++;         break;
        case AgentEventType::SwarmStarted:
        case AgentEventType::SwarmTaskCompleted:
        case AgentEventType::SwarmMerged:         m_historyStats.swarmTasks++;          break;
        case AgentEventType::ToolInvoked:         m_historyStats.toolInvocations++;     break;
        case AgentEventType::FailureDetected:     m_historyStats.failuresDetected++;    break;
        case AgentEventType::FailureCorrected:    m_historyStats.failuresCorrected++;   break;
        case AgentEventType::GhostTextAccepted:   m_historyStats.ghostTextAccepted++;   break;
        default: break;
    }

    // Thread-safe buffer insertion
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        if (m_eventBuffer.size() >= MAX_EVENT_BUFFER) {
            // Ring buffer: flush to disk and clear
            flushEventLog();
            m_eventBuffer.clear();
        }
        m_eventBuffer.push_back(std::move(ev));
    }
}

void Win32IDE::recordSimpleEvent(AgentEventType type, const std::string& description) {
    recordEvent(type, "", description, "", 0, true);
}

// ============================================================================
// FLUSH — write buffered events to JSONL file
// ============================================================================

void Win32IDE::flushEventLog() {
    // Caller must hold m_eventBufferMutex OR call from shutdown (single-threaded)
    if (m_eventBuffer.empty()) return;

    std::string path = getHistoryFilePath();

    // Ensure directory exists
    char appDataPath[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath) == S_OK) {
        std::string dir = std::string(appDataPath) + "\\RawrXD";
        CreateDirectoryA(dir.c_str(), nullptr);
    }

    std::ofstream f(path, std::ios::app | std::ios::binary);
    if (!f) {
        LOG_ERROR("Failed to open agent history file: " + path);
        return;
    }

    int written = 0;
    for (const auto& ev : m_eventBuffer) {
        f << ev.toJSONL() << "\n";
        written++;
    }

    f.close();
    LOG_DEBUG("Flushed " + std::to_string(written) + " events to " + path);
}

// ============================================================================
// HISTORY FILE PATH
// ============================================================================

std::string Win32IDE::getHistoryFilePath() const {
    char appDataPath[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath) == S_OK) {
        std::string dir = std::string(appDataPath) + "\\RawrXD";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\agent_history.jsonl";
    }
    return "agent_history.jsonl";
}

// ============================================================================
// LOAD HISTORY — read JSONL from disk
// ============================================================================

std::vector<AgentEvent> Win32IDE::loadHistory(int maxEvents) const {
    std::vector<AgentEvent> events;
    std::string path = getHistoryFilePath();

    std::ifstream f(path);
    if (!f) return events;

    // Read all lines, then take the last maxEvents
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[0] == '{') {
            lines.push_back(std::move(line));
        }
    }
    f.close();

    // Take last N lines
    int startIdx = 0;
    if ((int)lines.size() > maxEvents) {
        startIdx = (int)lines.size() - maxEvents;
    }

    events.reserve(maxEvents);
    for (int i = startIdx; i < (int)lines.size(); i++) {
        events.push_back(AgentEvent::fromJSONL(lines[i]));
    }

    return events;
}

std::vector<AgentEvent> Win32IDE::loadHistoryForSession(const std::string& sessionId) const {
    std::vector<AgentEvent> events;
    std::string path = getHistoryFilePath();

    std::ifstream f(path);
    if (!f) return events;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] != '{') continue;
        // Quick check before full parse
        if (line.find(sessionId) == std::string::npos) continue;

        AgentEvent ev = AgentEvent::fromJSONL(line);
        if (ev.sessionId == sessionId) {
            events.push_back(std::move(ev));
        }
    }

    return events;
}

std::vector<AgentEvent> Win32IDE::filterHistory(AgentEventType typeFilter, int maxEvents) const {
    std::vector<AgentEvent> all = loadHistory(5000);  // Load generously
    std::vector<AgentEvent> filtered;
    filtered.reserve(maxEvents);

    for (auto it = all.rbegin(); it != all.rend() && (int)filtered.size() < maxEvents; ++it) {
        if (it->type == typeFilter) {
            filtered.push_back(*it);
        }
    }

    // Reverse to chronological order
    std::reverse(filtered.begin(), filtered.end());
    return filtered;
}

// ============================================================================
// PRUNING — keep the log bounded
// ============================================================================

void Win32IDE::pruneHistory(int maxAgeDays, int maxFileBytes) {
    std::string path = getHistoryFilePath();

    // Check file size first
    std::ifstream sizeCheck(path, std::ios::ate | std::ios::binary);
    if (!sizeCheck) return;
    auto fileSize = sizeCheck.tellg();
    sizeCheck.close();

    if (fileSize < maxFileBytes && maxAgeDays <= 0) return;

    // Read all events
    std::ifstream f(path);
    if (!f) return;

    std::vector<std::string> kept;
    std::string line;

    uint64_t cutoffMs = 0;
    if (maxAgeDays > 0) {
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(24 * maxAgeDays);
        cutoffMs = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            cutoff.time_since_epoch()).count();
    }

    while (std::getline(f, line)) {
        if (line.empty() || line[0] != '{') continue;

        bool keep = true;

        // Age-based pruning
        if (cutoffMs > 0) {
            // Quick parse for timestamp
            AgentEvent ev = AgentEvent::fromJSONL(line);
            if (ev.timestampMs > 0 && ev.timestampMs < cutoffMs) {
                keep = false;
            }
        }

        if (keep) {
            kept.push_back(std::move(line));
        }
    }
    f.close();

    // Size-based pruning: keep only the newest events
    if (maxFileBytes > 0) {
        long totalSize = 0;
        int startIdx = (int)kept.size() - 1;
        for (int i = (int)kept.size() - 1; i >= 0; i--) {
            totalSize += (long)kept[i].size() + 1;  // +1 for newline
            if (totalSize > maxFileBytes) {
                startIdx = i + 1;
                break;
            }
            startIdx = i;
        }
        if (startIdx > 0) {
            kept.erase(kept.begin(), kept.begin() + startIdx);
        }
    }

    // Write back
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
    if (out) {
        for (const auto& l : kept) {
            out << l << "\n";
        }
        out.close();
        LOG_INFO("Pruned agent history: " + std::to_string(kept.size()) + " events retained");
    }
}

// ============================================================================
// AGENT HISTORY PANEL — Win32 Timeline UI
// ============================================================================

void Win32IDE::showAgentHistoryPanel() {
    // If panel already exists, bring to front
    if (m_hwndHistoryPanel && IsWindow(m_hwndHistoryPanel)) {
        ShowWindow(m_hwndHistoryPanel, SW_SHOW);
        SetForegroundWindow(m_hwndHistoryPanel);
        updateAgentHistoryPanel();
        return;
    }

    // Create the history panel window
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    m_hwndHistoryPanel = CreateWindowExA(
        exStyle, "STATIC", "Agent History Timeline",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!m_hwndHistoryPanel) {
        // Fallback: show stats in MessageBox
        MessageBoxA(m_hwndMain, getAgentHistoryStats().c_str(),
                    "Agent History", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Apply theme background
    SetClassLongPtrA(m_hwndHistoryPanel, GCLP_HBRBACKGROUND,
                     (LONG_PTR)CreateSolidBrush(m_currentTheme.panelBg));

    // --- Stats label at the top ---
    m_hwndHistoryStats = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 10, 770, 40,
        m_hwndHistoryPanel, nullptr, m_hInstance, nullptr);

    // --- ListView for events ---
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    m_hwndHistoryList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_VSCROLL,
        10, 55, 770, 300,
        m_hwndHistoryPanel, nullptr, m_hInstance, nullptr);

    // Set extended styles
    ListView_SetExtendedListViewStyle(m_hwndHistoryList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Add columns
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = (LPSTR)"Time";
    col.cx = 80;
    col.iSubItem = 0;
    ListView_InsertColumn(m_hwndHistoryList, 0, &col);

    col.pszText = (LPSTR)"Type";
    col.cx = 140;
    col.iSubItem = 1;
    ListView_InsertColumn(m_hwndHistoryList, 1, &col);

    col.pszText = (LPSTR)"Agent";
    col.cx = 100;
    col.iSubItem = 2;
    ListView_InsertColumn(m_hwndHistoryList, 2, &col);

    col.pszText = (LPSTR)"Prompt";
    col.cx = 250;
    col.iSubItem = 3;
    ListView_InsertColumn(m_hwndHistoryList, 3, &col);

    col.pszText = (LPSTR)"Duration";
    col.cx = 70;
    col.iSubItem = 4;
    ListView_InsertColumn(m_hwndHistoryList, 4, &col);

    col.pszText = (LPSTR)"OK";
    col.cx = 40;
    col.iSubItem = 5;
    ListView_InsertColumn(m_hwndHistoryList, 5, &col);

    // --- Detail pane (read-only edit) ---
    m_hwndHistoryDetail = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 365, 770, 140,
        m_hwndHistoryPanel, nullptr, m_hInstance, nullptr);

    // --- Buttons ---
    int btnY = 515;
    m_hwndHistoryBtnRefresh = CreateWindowExA(0, "BUTTON", "Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, btnY, 80, 30, m_hwndHistoryPanel, (HMENU)7101, m_hInstance, nullptr);

    m_hwndHistoryBtnReplay = CreateWindowExA(0, "BUTTON", "Replay Session",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        100, btnY, 120, 30, m_hwndHistoryPanel, (HMENU)7102, m_hInstance, nullptr);

    m_hwndHistoryBtnExport = CreateWindowExA(0, "BUTTON", "Export",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        230, btnY, 80, 30, m_hwndHistoryPanel, (HMENU)7103, m_hInstance, nullptr);

    m_hwndHistoryBtnClear = CreateWindowExA(0, "BUTTON", "Clear History",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        320, btnY, 110, 30, m_hwndHistoryPanel, (HMENU)7104, m_hInstance, nullptr);

    // Apply font
    HFONT hFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessageA(m_hwndHistoryList, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(m_hwndHistoryDetail, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(m_hwndHistoryStats, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(m_hwndHistoryBtnRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(m_hwndHistoryBtnReplay, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(m_hwndHistoryBtnExport, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(m_hwndHistoryBtnClear, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(m_hwndHistoryPanel, SW_SHOW);
    UpdateWindow(m_hwndHistoryPanel);

    // Populate
    updateAgentHistoryPanel();
}

void Win32IDE::updateAgentHistoryPanel() {
    if (!m_hwndHistoryPanel || !IsWindow(m_hwndHistoryPanel)) return;

    // Update stats label
    std::ostringstream statsText;
    statsText << "Session: " << m_currentSessionId
              << "  |  Events: " << m_historyStats.totalEvents
              << "  |  Agent: " << m_historyStats.agentCompleted << "/" << m_historyStats.agentStarted
              << "  |  Failures: " << m_historyStats.failuresDetected
              << "  |  Ghost: " << m_historyStats.ghostTextAccepted;
    SetWindowTextA(m_hwndHistoryStats, statsText.str().c_str());

    // Clear and repopulate list view
    ListView_DeleteAllItems(m_hwndHistoryList);

    // Combine in-memory buffer + most recent disk events
    std::vector<AgentEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        events = m_eventBuffer;  // Copy current buffer
    }

    // If buffer is small, also load from disk for context
    if (events.size() < 200) {
        auto diskEvents = loadHistory(200);
        // Merge: disk events first, then buffer (avoiding duplicates by timestamp)
        std::vector<AgentEvent> merged;
        merged.reserve(diskEvents.size() + events.size());

        for (auto& ev : diskEvents) {
            merged.push_back(std::move(ev));
        }
        // Only add buffer events that are newer than the last disk event
        uint64_t lastDiskTs = diskEvents.empty() ? 0 : diskEvents.back().timestampMs;
        for (auto& ev : events) {
            if (ev.timestampMs > lastDiskTs) {
                merged.push_back(std::move(ev));
            }
        }
        events = std::move(merged);
    }

    // Populate ListView — show most recent first (reverse)
    int rowIdx = 0;
    for (int i = (int)events.size() - 1; i >= 0 && rowIdx < 500; i--, rowIdx++) {
        const auto& ev = events[i];

        // Column 0: Time (HH:MM:SS from epoch)
        auto tp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(ev.timestampMs));
        auto tt = std::chrono::system_clock::to_time_t(tp);
        struct tm tmBuf = {};
        localtime_s(&tmBuf, &tt);
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
                 tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

        LVITEMA item = {};
        item.mask = LVIF_TEXT;
        item.iItem = rowIdx;
        item.iSubItem = 0;
        item.pszText = timeBuf;
        ListView_InsertItem(m_hwndHistoryList, &item);

        // Column 1: Type
        std::string typeStr = ev.typeString();
        ListView_SetItemText(m_hwndHistoryList, rowIdx, 1, (LPSTR)typeStr.c_str());

        // Column 2: Agent ID (truncated)
        std::string agentStr = ev.agentId.empty() ? "-" : ev.agentId.substr(0, 12);
        ListView_SetItemText(m_hwndHistoryList, rowIdx, 2, (LPSTR)agentStr.c_str());

        // Column 3: Prompt (first 80 chars)
        std::string promptStr = ev.prompt.substr(0, 80);
        // Replace newlines with spaces for display
        std::replace(promptStr.begin(), promptStr.end(), '\n', ' ');
        std::replace(promptStr.begin(), promptStr.end(), '\r', ' ');
        ListView_SetItemText(m_hwndHistoryList, rowIdx, 3, (LPSTR)promptStr.c_str());

        // Column 4: Duration
        std::string durStr = ev.durationMs > 0 ?
            std::to_string(ev.durationMs) + "ms" : "-";
        ListView_SetItemText(m_hwndHistoryList, rowIdx, 4, (LPSTR)durStr.c_str());

        // Column 5: OK
        std::string okStr = ev.success ? "Y" : "N";
        ListView_SetItemText(m_hwndHistoryList, rowIdx, 5, (LPSTR)okStr.c_str());
    }
}

// ============================================================================
// REPLAY — re-execute a session's agent commands
// ============================================================================

void Win32IDE::showAgentReplayDialog() {
    // Load unique session IDs from history
    auto allEvents = loadHistory(5000);
    std::vector<std::string> sessions;
    std::set<std::string> seen;

    for (auto it = allEvents.rbegin(); it != allEvents.rend(); ++it) {
        if (!it->sessionId.empty() && seen.find(it->sessionId) == seen.end()) {
            seen.insert(it->sessionId);
            sessions.push_back(it->sessionId);
            if (sessions.size() >= 20) break;  // Show at most 20 recent sessions
        }
    }

    if (sessions.empty()) {
        MessageBoxA(m_hwndMain, "No previous sessions found in agent history.",
                    "Replay", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Build selection string
    std::ostringstream oss;
    oss << "Select a session to replay:\r\n\r\n";
    for (int i = 0; i < (int)sessions.size(); i++) {
        // Count events for this session
        int eventCount = 0;
        for (const auto& ev : allEvents) {
            if (ev.sessionId == sessions[i]) eventCount++;
        }
        oss << (i + 1) << ". " << sessions[i] << " (" << eventCount << " events)\r\n";
    }
    oss << "\r\nEnter session number (1-" << sessions.size() << "):";

    // Simple input via MessageBox (full dialog is a future upgrade)
    // For now, replay the most recent non-current session
    std::string replaySessionId;
    for (const auto& sid : sessions) {
        if (sid != m_currentSessionId) {
            replaySessionId = sid;
            break;
        }
    }

    if (replaySessionId.empty()) {
        MessageBoxA(m_hwndMain, "No other sessions available for replay.",
                    "Replay", MB_OK | MB_ICONINFORMATION);
        return;
    }

    int choice = MessageBoxA(m_hwndMain,
        ("Replay session: " + replaySessionId + "?\r\n\r\n"
         "This will re-execute the agent commands from that session.\r\n"
         "The original files will NOT be modified.").c_str(),
        "Replay Session", MB_YESNO | MB_ICONQUESTION);

    if (choice == IDYES) {
        replaySession(replaySessionId);
    }
}

void Win32IDE::replaySession(const std::string& sessionId) {
    auto events = loadHistoryForSession(sessionId);
    if (events.empty()) {
        appendToOutput("[History] No events found for session: " + sessionId,
                       "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[History] Replaying session: " + sessionId +
                   " (" + std::to_string(events.size()) + " events)",
                   "General", OutputSeverity::Info);

    // Record replay start
    recordSimpleEvent(AgentEventType::SessionEvent,
                      "Replay started for session: " + sessionId);

    // Replay only AgentStarted events (these are the user-initiated prompts)
    int replayCount = 0;
    int totalReplayable = 0;

    for (const auto& ev : events) {
        if (ev.type == AgentEventType::AgentStarted && !ev.prompt.empty()) {
            totalReplayable++;
        }
    }

    if (totalReplayable == 0) {
        appendToOutput("[History] No replayable agent commands in session.",
                       "General", OutputSeverity::Warning);
        return;
    }

    // Execute on background thread to avoid blocking UI
    std::string sid = sessionId;
    std::vector<AgentEvent> replayEvents = std::move(events);

    std::thread replayThread([this, sid, replayEvents = std::move(replayEvents),
                              totalReplayable]() {
        int stepIdx = 0;
        for (const auto& ev : replayEvents) {
            if (ev.type != AgentEventType::AgentStarted || ev.prompt.empty()) continue;

            stepIdx++;

            // Post progress to UI thread
            PostMessageA(m_hwndMain, WM_AGENT_HISTORY_REPLAY_DONE,
                         (WPARAM)stepIdx, (LPARAM)totalReplayable);

            // Re-execute the prompt
            if (m_agenticBridge && m_agenticBridge->IsInitialized()) {
                std::string prompt = "[REPLAY " + std::to_string(stepIdx) + "/" +
                                     std::to_string(totalReplayable) + "] " + ev.prompt;

                auto response = m_agenticBridge->ExecuteAgentCommand(prompt);

                // Record the replay result
                recordEvent(AgentEventType::AgentCompleted, "replay",
                           ev.prompt, response.content, 0, true, "",
                           "{\"replayOf\":\"" + jsonEscape(sid) + "\"}");
            }

            // Small delay between replay steps to avoid overwhelming the model
            Sleep(500);
        }

        // Post completion
        PostMessageA(m_hwndMain, WM_AGENT_HISTORY_REPLAY_DONE,
                     (WPARAM)totalReplayable, (LPARAM)totalReplayable);
    });

    replayThread.detach();
}

void Win32IDE::onReplayStepDone(int stepIndex, int totalSteps) {
    if (stepIndex >= totalSteps) {
        appendToOutput("[History] Replay complete (" + std::to_string(totalSteps) + " steps)",
                       "General", OutputSeverity::Info);
        recordSimpleEvent(AgentEventType::SessionEvent, "Replay completed");
    } else {
        appendToOutput("[History] Replay step " + std::to_string(stepIndex) + "/" +
                       std::to_string(totalSteps),
                       "General", OutputSeverity::Debug);
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

std::string Win32IDE::getAgentHistoryStats() const {
    std::ostringstream oss;
    oss << "=== Agent History Statistics ===\r\n";
    oss << "Session:               " << m_currentSessionId << "\r\n";
    oss << "History Enabled:       " << (m_agentHistoryEnabled ? "YES" : "NO") << "\r\n";
    oss << "---\r\n";
    oss << "Total Events:          " << m_historyStats.totalEvents << "\r\n";
    oss << "Agent Started:         " << m_historyStats.agentStarted << "\r\n";
    oss << "Agent Completed:       " << m_historyStats.agentCompleted << "\r\n";
    oss << "Agent Failed:          " << m_historyStats.agentFailed << "\r\n";
    oss << "SubAgents Spawned:     " << m_historyStats.subAgentSpawned << "\r\n";
    oss << "Chain Steps:           " << m_historyStats.chainSteps << "\r\n";
    oss << "Swarm Tasks:           " << m_historyStats.swarmTasks << "\r\n";
    oss << "Tool Invocations:      " << m_historyStats.toolInvocations << "\r\n";
    oss << "Failures Detected:     " << m_historyStats.failuresDetected << "\r\n";
    oss << "Failures Corrected:    " << m_historyStats.failuresCorrected << "\r\n";
    oss << "Ghost Text Accepted:   " << m_historyStats.ghostTextAccepted << "\r\n";
    oss << "---\r\n";
    oss << "Total Duration:        " << m_historyStats.totalDurationMs << " ms\r\n";

    if (m_historyStats.agentStarted > 0) {
        float avgDur = (float)m_historyStats.totalDurationMs / m_historyStats.agentStarted;
        oss << "Avg Duration/Request:  " << (int)avgDur << " ms\r\n";
    }

    if (m_historyStats.agentStarted > 0) {
        float successRate = (float)m_historyStats.agentCompleted / m_historyStats.agentStarted;
        oss << "Success Rate:          " << (int)(successRate * 100) << "%\r\n";
    }

    oss << "\r\n[History file: " << getHistoryFilePath() << "]\r\n";

    // In-memory buffer status
    {
        // Note: Not locking mutex here since this is read-only and for display
        oss << "Buffer:                " << m_eventBuffer.size() << "/" << MAX_EVENT_BUFFER << "\r\n";
    }

    return oss.str();
}

// ============================================================================
// TOGGLE
// ============================================================================

void Win32IDE::toggleAgentHistory() {
    m_agentHistoryEnabled = !m_agentHistoryEnabled;
    appendToOutput(std::string("[History] Agent history ") +
                   (m_agentHistoryEnabled ? "Enabled" : "Disabled"),
                   "General", OutputSeverity::Info);

    if (m_agentHistoryEnabled && m_currentSessionId.empty()) {
        m_currentSessionId = generateSessionId();
    }
}

// ============================================================================
// UTILITY HELPERS
// ============================================================================

std::string Win32IDE::generateSessionId() const {
    // Format: YYYYMMDD-HHMMSS-XXXX (date + time + random suffix)
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf = {};
    localtime_s(&tmBuf, &tt);

    // Random 4-char hex suffix
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 0xFFFF);
    int suffix = dis(gen);

    char buf[32];
    snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d-%04X",
             tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
             tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
             suffix);

    return std::string(buf);
}

std::string Win32IDE::agentEventTypeString(AgentEventType type) const {
    // Delegate to AgentEvent::typeString via a temp object
    AgentEvent temp;
    temp.type = type;
    return temp.typeString();
}

uint64_t Win32IDE::currentEpochMs() const {
    auto now = std::chrono::system_clock::now();
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

std::string Win32IDE::truncateForLog(const std::string& text, size_t maxLen) const {
    if (text.size() <= maxLen) return text;
    return text.substr(0, maxLen - 3) + "...";
}
