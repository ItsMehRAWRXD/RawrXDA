#pragma once

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <json/json.h>

// ============================================================================
// STRUCTURES
// ============================================================================

struct ChatMessage {
    std::string sender;      // "Agent" or "User"
    std::string content;     // Message text
    std::string timestamp;   // When sent
    size_t tokenCount;       // Approximate token count
    std::vector<std::string> attachedFiles; // File paths
};

struct FileUpload {
    std::string filePath;
    std::string fileName;
    size_t fileSize;
    std::string fileType;
    std::vector<uint8_t> preview; // First 1MB
};

struct ContextWindow {
    size_t maxTokens;        // 256,000
    size_t currentTokens;
    size_t maxMessages;
    std::deque<ChatMessage> messages;
    size_t oldestMessageIndex;
};

struct ChatSession {
    std::string sessionId;
    std::string sessionName;
    std::string createdAt;
    std::string lastModified;
    std::vector<ChatMessage> messageHistory;
    ContextWindow contextWindow;
};

// ============================================================================
// MAIN CHAT APPLICATION CLASS
// ============================================================================

class Win32ChatApp
{
public:
    Win32ChatApp(HINSTANCE hInstance);
    ~Win32ChatApp();

    // Window and UI
    bool createWindow();
    void showWindow();
    void hideWindow();
    void toggleVisibility();
    int runMessageLoop();

    // Taskbar integration
    void createTrayIcon();
    void removeTrayIcon();
    void showContextMenu(int x, int y);

    // Chat UI
    void createChatUI();
    void appendAgentMessage(const std::string& message);
    void appendUserMessage(const std::string& message);
    void clearChat();
    void loadChatHistory(const std::string& sessionId);
    void saveChatHistory();

    // File handling
    void onFileDropped(const std::vector<std::string>& files);
    void uploadFile(const std::string& filePath);
    void displayFilePreview(const FileUpload& file);

    // Context management
    void updateContextWindow();
    void pruneOldMessages();
    size_t estimateTokenCount(const std::string& text);
    void displayContextStats();

    // Model connection
    void sendPromptToModel(const std::string& prompt);
    void receiveModelResponse(const std::string& response);
    void onModelStreamChunk(const std::string& chunk);

    // Settings and persistence
    void loadSettings();
    void saveSettings();
    void loadSessionList();
    void createNewSession();

private:
    // Static window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK AgentPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK UserPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Instance message handlers
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void onCreate(HWND hwnd);
    void onDestroy();
    void onSize(int width, int height);
    void onCommand(int id, int notifyCode);

    // UI Layout
    void layoutChatPanels();
    void layoutFilePanel();

    // Message formatting
    std::string formatTimestamp();
    std::string formatFileSize(size_t bytes);
    void formatMessageWithMarkdown(const std::string& raw, std::string& formatted);

    // Context calculations
    std::string getContextStats() const;
    void logContextUsage();

    // Member variables
    HINSTANCE m_hInstance;
    HWND m_hwndMain;
    HWND m_hwndAgentPanel;        // Top: Agent responses
    HWND m_hwndUserPanel;         // Bottom: User input
    HWND m_hwndFilePanel;         // Right: File previews
    HWND m_hwndSendButton;
    HWND m_hwndClearButton;
    HWND m_hwndUploadButton;
    HWND m_hwndContextStats;
    HWND m_hwndStatusBar;

    // Tray icon
    NOTIFYICONDATA m_nid;
    UINT m_trayIconId;
    bool m_isMinimized;
    bool m_isVisible;

    // Chat data
    ChatSession m_currentSession;
    std::vector<ChatSession> m_sessionList;
    std::map<std::string, ChatSession> m_sessionMap;
    std::deque<FileUpload> m_uploadedFiles;

    // UI state
    int m_panelSplitY;
    int m_panelSplitX;
    bool m_filePanelVisible;
    bool m_draggingSplitter;

    // Theme and styling
    HBRUSH m_agentPanelBrush;
    HBRUSH m_userPanelBrush;
    HBRUSH m_backgroundBrush;
    HFONT m_chatFont;
    COLORREF m_agentTextColor;
    COLORREF m_userTextColor;
    COLORREF m_agentBgColor;
    COLORREF m_userBgColor;

    // Settings
    std::string m_appDataPath;
    std::string m_settingsPath;
    std::string m_historyPath;
    bool m_darkMode;
    int m_fontSize;
    int m_windowWidth;
    int m_windowHeight;

    // Model connection
    std::string m_modelEndpoint;    // e.g., "http://localhost:11434"
    std::string m_currentModel;
    bool m_isConnected;
    bool m_isWaitingForResponse;

    // Context window
    ContextWindow m_contextWindow;
    size_t m_maxContextTokens;
};
