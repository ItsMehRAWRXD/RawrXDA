// Win32IDE_Collab.cpp — Collaboration panel: CRDT + WebSocket + cursor sharing.
// Real onCollabMessage handler: parses JSON ops, applies to CRDTBuffer,
// broadcasts edits, shares cursor positions.

#include "Win32IDE.h"
#include "collab/websocket_hub.h"
#include "collab/crdt_buffer.h"
#include "collab/cursor_widget.h"
#include <commctrl.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>

static const uint16_t COLLAB_DEFAULT_PORT = 5173;
static const wchar_t* COLLAB_CLASS = L"RawrXD_CollabPanel";
static HWND s_hwndCollabPanel = nullptr;
static HWND s_hwndStatus = nullptr;
static HWND s_hwndJoinUrl = nullptr;
static HWND s_hwndStartBtn = nullptr;
static HWND s_hwndStopBtn = nullptr;
static WebSocketHub s_collabHub;
static bool s_collabClassRegistered = false;

// CRDT buffer for collaborative text editing
static CRDTBuffer s_crdtBuffer;
static CursorWidget s_cursorWidget;
static std::mutex s_collabMutex;

// Participant tracking
struct CollabParticipant {
    std::string userId;
    std::string userName;
    int cursorPosition = 0;
    uint32_t color = 0;
    bool active = true;
};
static std::unordered_map<std::string, CollabParticipant> s_participants;
static int s_nextParticipantColor = 0;

static uint32_t assignParticipantColor() {
    // Cycle through distinct colors for up to 8 participants
    static const uint32_t colors[] = {
        0xFF6B6B, 0x4ECDC4, 0xFFE66D, 0x95E1D3,
        0xF38181, 0xAA96DA, 0xFCBAD3, 0xA8D8EA
    };
    return colors[(s_nextParticipantColor++) % 8];
}

// ============================================================================
// Collab message protocol (JSON-based)
// ============================================================================
// Messages:
//   {"type":"join","userId":"x","userName":"y"}
//   {"type":"leave","userId":"x"}
//   {"type":"cursor","userId":"x","position":123}
//   {"type":"op","operation":"<CRDT serialized op>"}
//   {"type":"sync-request","userId":"x"}
//   {"type":"sync-response","text":"<full document>"}
// ============================================================================

// Simple JSON field extractors (no dependency on nlohmann for this small protocol)
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static int extractJsonInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return -1;
    pos += needle.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    std::string numStr;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        numStr += json[pos++];
    }
    return numStr.empty() ? -1 : std::stoi(numStr);
}

static void updateCollabUI(HWND hwnd) {
    bool running = s_collabHub.isRunning();
    if (s_hwndStatus) {
        std::wstring text;
        if (running) {
            text = L"Server running on port ";
            text += std::to_wstring(s_collabHub.getPort());
            text += L" \u2014 CRDT + WebSocket ready. ";
            {
                std::lock_guard<std::mutex> lock(s_collabMutex);
                text += std::to_wstring(s_participants.size());
            }
            text += L" participant(s) connected.";
        } else {
            text = L"Stopped. Click \"Start session\" to host.";
        }
        SetWindowTextW(s_hwndStatus, text.c_str());
    }
    if (s_hwndJoinUrl) {
        std::wstring url = L"ws://127.0.0.1:";
        url += std::to_wstring(running ? s_collabHub.getPort() : COLLAB_DEFAULT_PORT);
        SetWindowTextW(s_hwndJoinUrl, url.c_str());
    }
    if (s_hwndStartBtn) EnableWindow(s_hwndStartBtn, running ? FALSE : TRUE);
    if (s_hwndStopBtn)  EnableWindow(s_hwndStopBtn,  running ? TRUE : FALSE);
}

static void onCollabMessage(const std::string& messageJson, void* /*client*/) {
    std::lock_guard<std::mutex> lock(s_collabMutex);

    std::string msgType = extractJsonString(messageJson, "type");

    if (msgType == "join") {
        // New participant joining the session
        std::string userId = extractJsonString(messageJson, "userId");
        std::string userName = extractJsonString(messageJson, "userName");
        if (userId.empty()) return;

        CollabParticipant participant;
        participant.userId = userId;
        participant.userName = userName.empty() ? ("User-" + userId) : userName;
        participant.color = assignParticipantColor();
        participant.active = true;
        s_participants[userId] = participant;

        // Update cursor widget for this participant
        CursorInfo ci;
        ci.position = 0;
        ci.userName = participant.userName;
        ci.color = participant.color;
        s_cursorWidget.updateCursor(userId, ci);

        // Broadcast join notification to all peers
        std::string notify = "{\"type\":\"joined\",\"userId\":\"" + userId +
                             "\",\"userName\":\"" + participant.userName +
                             "\",\"color\":" + std::to_string(participant.color) + "}";
        s_collabHub.broadcastMessage(notify);

        // Send sync response with current document state
        std::string docText = s_crdtBuffer.getText();
        // Escape for JSON
        std::string escaped;
        for (char c : docText) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') escaped += "\\r";
            else if (c == '\t') escaped += "\\t";
            else escaped += c;
        }
        std::string syncMsg = "{\"type\":\"sync-response\",\"text\":\"" + escaped + "\"}";
        s_collabHub.broadcastMessage(syncMsg);

        OutputDebugStringA(("[Collab] Participant joined: " + participant.userName + "\n").c_str());

    } else if (msgType == "leave") {
        // Participant leaving
        std::string userId = extractJsonString(messageJson, "userId");
        if (userId.empty()) return;

        auto it = s_participants.find(userId);
        if (it != s_participants.end()) {
            OutputDebugStringA(("[Collab] Participant left: " + it->second.userName + "\n").c_str());
            s_cursorWidget.removeCursor(userId);
            s_participants.erase(it);
        }

        // Broadcast leave notification
        std::string notify = "{\"type\":\"left\",\"userId\":\"" + userId + "\"}";
        s_collabHub.broadcastMessage(notify);

    } else if (msgType == "cursor") {
        // Cursor position update from a peer
        std::string userId = extractJsonString(messageJson, "userId");
        int position = extractJsonInt(messageJson, "position");
        if (userId.empty() || position < 0) return;

        auto it = s_participants.find(userId);
        if (it != s_participants.end()) {
            it->second.cursorPosition = position;

            CursorInfo ci;
            ci.position = position;
            ci.userName = it->second.userName;
            ci.color = it->second.color;
            s_cursorWidget.updateCursor(userId, ci);
        }

        // Re-broadcast cursor to other peers
        s_collabHub.broadcastMessage(messageJson);

    } else if (msgType == "op") {
        // CRDT operation from a peer — apply to local buffer
        std::string operation = extractJsonString(messageJson, "operation");
        if (operation.empty()) return;

        s_crdtBuffer.applyRemoteOperation(operation);

        // Re-broadcast to all other peers (fan-out from server)
        s_collabHub.broadcastMessage(messageJson);

        OutputDebugStringA("[Collab] Applied remote CRDT operation.\n");

    } else if (msgType == "sync-request") {
        // Peer requesting full document sync
        std::string docText = s_crdtBuffer.getText();
        std::string escaped;
        for (char c : docText) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') escaped += "\\r";
            else if (c == '\t') escaped += "\\t";
            else escaped += c;
        }
        std::string syncMsg = "{\"type\":\"sync-response\",\"text\":\"" + escaped + "\"}";
        s_collabHub.broadcastMessage(syncMsg);

    } else if (msgType == "sync-response") {
        // Received full document — used by joining clients
        // Server-side: no additional action needed (client processes this)
        OutputDebugStringA("[Collab] Sync response received/forwarded.\n");

    } else {
        // Unknown message type — log and forward
        OutputDebugStringA(("[Collab] Unknown message type: " + msgType + "\n").c_str());
        s_collabHub.broadcastMessage(messageJson);
    }
}

static LRESULT CALLBACK CollabPanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        int y = 10;
        s_hwndStatus = CreateWindowW(L"STATIC", L"Stopped.", WS_CHILD | WS_VISIBLE,
            10, y, 480, 24, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        y += 32;
        CreateWindowW(L"STATIC", L"Join URL (share with peers):", WS_CHILD | WS_VISIBLE,
            10, y, 200, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        y += 24;
        s_hwndJoinUrl = CreateWindowW(L"EDIT", L"ws://127.0.0.1:5173",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
            10, y, 480, 24, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        y += 36;
        s_hwndStartBtn = CreateWindowW(L"BUTTON", L"Start session",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, y, 120, 28, hwnd, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
        s_hwndStopBtn = CreateWindowW(L"BUTTON", L"Stop session",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 140, y, 120, 28, hwnd, (HMENU)2, GetModuleHandleW(nullptr), nullptr);
        s_collabHub.setOnMessageReceived(onCollabMessage);
        updateCollabUI(hwnd);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1) {
            if (s_collabHub.startServer(COLLAB_DEFAULT_PORT)) {
                // Wire CRDT buffer: when local edits generate ops, broadcast them
                s_crdtBuffer.setOnOperationGenerated([](const std::string& op) {
                    std::string msg = "{\"type\":\"op\",\"operation\":\"" + op + "\"}";
                    s_collabHub.broadcastMessage(msg);
                });

                // Wire CRDT buffer: when text changes, update local editor state
                s_crdtBuffer.setOnTextChanged([](const std::string& newText) {
                    OutputDebugStringA(("[Collab] CRDT text updated (" +
                                       std::to_string(newText.size()) + " bytes)\n").c_str());
                });

                // Clear participant state for fresh session
                {
                    std::lock_guard<std::mutex> lock(s_collabMutex);
                    s_participants.clear();
                    s_nextParticipantColor = 0;
                }

                updateCollabUI(hwnd);
                Win32IDE* ide = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                if (ide)
                    ide->appendToOutput("[Collaboration] Server started on port " + std::to_string(COLLAB_DEFAULT_PORT) + " — CRDT + WebSocket + cursor sharing active.\r\n", "Output", Win32IDE::OutputSeverity::Info);
            }
        } else if (LOWORD(wParam) == 2) {
            // Notify all participants before stopping
            s_collabHub.broadcastMessage("{\"type\":\"server-shutdown\"}");

            s_collabHub.stopServer();
            {
                std::lock_guard<std::mutex> lock(s_collabMutex);
                s_participants.clear();
            }
            updateCollabUI(hwnd);
            Win32IDE* ide = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (ide)
                ide->appendToOutput("[Collaboration] Server stopped. All participants disconnected.\r\n", "Output", Win32IDE::OutputSeverity::Info);
        }
        return 0;
    }
    case WM_CLOSE:
        if (s_collabHub.isRunning()) s_collabHub.stopServer();
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        s_hwndCollabPanel = nullptr;
        s_hwndStatus = nullptr;
        s_hwndJoinUrl = nullptr;
        s_hwndStartBtn = nullptr;
        s_hwndStopBtn = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static bool ensureCollabClass() {
    if (s_collabClassRegistered) return true;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = CollabPanelWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = COLLAB_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) return false;
    s_collabClassRegistered = true;
    return true;
}

void Win32IDE::showCollabPanel() {
    if (s_hwndCollabPanel && IsWindow(s_hwndCollabPanel)) {
        SetForegroundWindow(s_hwndCollabPanel);
        ShowWindow(s_hwndCollabPanel, SW_SHOW);
        return;
    }
    if (!ensureCollabClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register Collaboration panel class.", L"Collab", MB_OK | MB_ICONERROR);
        return;
    }
    s_hwndCollabPanel = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, COLLAB_CLASS,
        L"RawrXD — Collaboration (CRDT + WebSocket)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 220,
        m_hwndMain, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (s_hwndCollabPanel) {
        SetWindowLongPtrW(s_hwndCollabPanel, GWLP_USERDATA, (LONG_PTR)this);
        updateCollabUI(s_hwndCollabPanel);
        ShowWindow(s_hwndCollabPanel, SW_SHOW);
        UpdateWindow(s_hwndCollabPanel);
        appendToOutput("[Collaboration] Panel opened. Use \"Start session\" to host a live CRDT/WebSocket server.\r\n", "Output", OutputSeverity::Info);
    }
}
