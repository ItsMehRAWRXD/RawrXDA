// ============================================================================
// Agent Chat Pane — Full dump: bubbles, controls, handlers (Cursor-style)
// Extracted from Win32IDE createChatPanel / HandleCopilot* / m_chatHistory
// ============================================================================
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <utility>

// ---- Control IDs (Win32IDE.cpp / Win32IDE.h) ----
#define IDC_SECONDARY_SIDEBAR      1200
#define IDC_SECONDARY_SIDEBAR_HEADER 1201
#define IDC_COPILOT_CHAT_INPUT     1202
#define IDC_COPILOT_CHAT_OUTPUT    1203
#define IDC_COPILOT_SEND_BTN       1204
#define IDC_COPILOT_CLEAR_BTN      1205
#define IDC_AI_CONTEXT_SLIDER      1206
#define IDC_AI_CONTEXT_LABEL       1207
#define IDC_AI_MAX_MODE            5001
#define IDC_AI_DEEP_THINK          5002
#define IDC_AI_DEEP_RESEARCH       5003
#define IDC_AI_NO_REFUSAL          5004
#define IDM_AI_MODE_MAX            4200

// ---- Bubble / message model (role + content) ----
using ChatEntry = std::pair<std::string, std::string>; // role, message
using ChatHistory = std::vector<ChatEntry>;

// ---- HWNDs (one container, one output, one input, buttons, toggles) ----
struct AgentChatPaneHWNDs {
    HWND hSecondarySidebar;      // IDC_SECONDARY_SIDEBAR, STATIC, 300x600
    HWND hHeader;               // "AI Chat", STATIC
    HWND hModelLabel;            // "Model:", STATIC
    HWND hModelSelector;         // COMBOBOX, CBS_DROPDOWN
    HWND hMaxTokensLabel;       // "512", STATIC
    HWND hMaxTokensSlider;      // TRACKBAR, 32..2048
    HWND hContextLabel;         // "4K", STATIC
    HWND hContextSlider;        // TRACKBAR, 7 steps (2048..1M)
    HWND hContextCombo;         // COMBOBOX (4200), 2048/4k/32k/64k/128k/256k/512k/1M
    HWND hChkMaxMode;           // IDC_AI_MAX_MODE, BS_AUTOCHECKBOX
    HWND hChkDeepThink;         // IDC_AI_DEEP_THINK
    HWND hChkDeepResearch;      // IDC_AI_DEEP_RESEARCH
    HWND hChkNoRefusal;         // IDC_AI_NO_REFUSAL
    HWND hCopilotChatOutput;    // IDC_COPILOT_CHAT_OUTPUT, EDIT ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL, 5,200,290,210
    HWND hCopilotChatInput;     // IDC_COPILOT_CHAT_INPUT,  EDIT ES_MULTILINE|ES_WANTRETURN|WS_VSCROLL, 5,415,290,85
    HWND hCopilotSendBtn;       // IDC_COPILOT_SEND_BTN, "Send", 5,505,140,30
    HWND hCopilotClearBtn;      // IDC_COPILOT_CLEAR_BTN, "Clear", 150,505,140,30
};

// ---- Message flow (bubbles rendered into output EDIT) ----
// Display format: "\n> User: " + userMessage + "\n" then "AI: " + response (+ "\n" when complete)
// appendChatMessage(user, message) -> "[HH:MM:SS] user: message\n\n" -> appendToOutput(..., "Output", ...)
// Stream: HandleCopilotStreamUpdate(token, len) -> EM_SETSEL end, EM_REPLACESEL chunk, WM_VSCROLL SB_BOTTOM

// ---- Handlers (entry points) ----
// createChatPanel()           -> CreateWindowExW for all above
// HandleCopilotSend()         -> GetWindowTextW(hCopilotChatInput), display "> User: ...", generateResponseAsync(..., onResponse), onResponse displays "AI: ..."
// HandleCopilotClear()        -> SetWindowText(hCopilotChatOutput, "Welcome to RawrXD AI Chat!..."), m_chatHistory.clear()
// HandleCopilotStreamUpdate() -> append chunk to hCopilotChatOutput, scroll to bottom
// appendChatMessage(user,msg) -> formatted "[timestamp] user: message" -> appendToOutput (Output panel; not the sidebar EDIT)
// generateResponseAsync()     -> async LLM call, callback appends to hCopilotChatOutput

// ---- State ----
// std::vector<std::pair<std::string,std::string>> m_chatHistory;  // role, message
// bool m_chatMode;  // command input vs terminal mode
// std::string m_ollamaModelOverride;
// int m_currentMaxTokens, m_currentContextSize;
// m_secondarySidebarVisible, m_secondarySidebarWidth (320)

// ---- Welcome / initial content ----
// "Welcome to RawrXD AI Chat!\n\nSelect a model and type your message to begin."
// Post-load: "You can now ask questions in the chat panel.\r\n" + agentic welcome to Copilot
