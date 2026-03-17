#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

// ============================================================================
// Terminal Emulator for RawrXD Win32 IDE
// ============================================================================

// Terminal configuration
struct TerminalConfig {
    std::wstring shell = L"powershell.exe";
    std::wstring workingDirectory = L"";
    int width = 80;
    int height = 24;
    int fontSize = 12;
    std::wstring fontFamily = L"Consolas";
    COLORREF backgroundColor = RGB(20, 20, 20);
    COLORREF foregroundColor = RGB(200, 255, 100);
    COLORREF cursorColor = RGB(255, 255, 255);
    bool wordWrap = false;
    bool scrollToBottom = true;
    bool showScrollbar = true;
    int bufferSize = 1000;
    bool enableMouse = true;
    bool enableKeyboard = true;
    bool enableSelection = true;
    bool enableCopyPaste = true;
    std::vector<std::wstring> environmentVariables;
};

// Terminal cell (character with attributes)
struct TerminalCell {
    WCHAR character = L' ';
    COLORREF foregroundColor = RGB(200, 255, 100);
    COLORREF backgroundColor = RGB(20, 20, 20);
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool blink = false;
    bool reverse = false;
    bool invisible = false;
};

// Terminal cursor position
struct TerminalPosition {
    int x = 0;
    int y = 0;
    bool visible = true;
    std::wstring style = L"block"; // "block", "underline", "bar"
};

// Terminal selection
struct TerminalSelection {
    TerminalPosition start;
    TerminalPosition end;
    bool active = false;
    std::wstring text;
};

// Terminal event types
enum class TerminalEventType {
    Output,
    Error,
    Exit,
    TitleChanged,
    Bell,
    Resize,
    Focus,
    Blur,
    SelectionChanged,
    Scroll
};

// Terminal event
struct TerminalEvent {
    TerminalEventType type;
    std::wstring data;
    int exitCode = 0;
    long long timestamp;
};

// Terminal command result
struct TerminalCommandResult {
    int exitCode = 0;
    std::wstring output;
    std::wstring error;
    bool success = false;
    long long executionTime = 0;
};

// Terminal interface
class ITerminalEmulator {
public:
    virtual ~ITerminalEmulator() = default;

    // Core terminal operations
    virtual bool Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    virtual bool IsRunning() const = 0;

    // Process management
    virtual bool StartProcess(const std::wstring& command = L"") = 0;
    virtual void StopProcess() = 0;
    virtual void SendInput(const std::wstring& input) = 0;
    virtual void SendKey(WORD virtualKey, bool ctrl = false, bool alt = false, bool shift = false) = 0;
    virtual TerminalCommandResult ExecuteCommand(const std::wstring& command, int timeoutMs = 30000) = 0;

    // Display operations
    virtual void Clear() = 0;
    virtual void ClearLine() = 0;
    virtual void ClearToEndOfLine() = 0;
    virtual void ClearToEndOfScreen() = 0;
    virtual void ScrollUp(int lines = 1) = 0;
    virtual void ScrollDown(int lines = 1) = 0;
    virtual void ScrollToTop() = 0;
    virtual void ScrollToBottom() = 0;

    // Cursor operations
    virtual void SetCursorPosition(int x, int y) = 0;
    virtual TerminalPosition GetCursorPosition() const = 0;
    virtual void ShowCursor(bool show) = 0;
    virtual void SetCursorStyle(const std::wstring& style) = 0;

    // Selection operations
    virtual void SelectAll() = 0;
    virtual void SelectNone() = 0;
    virtual void SelectWord() = 0;
    virtual void SelectLine() = 0;
    virtual void CopySelection() = 0;
    virtual void PasteClipboard() = 0;
    virtual TerminalSelection GetSelection() const = 0;
    virtual std::wstring GetSelectedText() const = 0;

    // Configuration
    virtual void UpdateConfig(const TerminalConfig& config) = 0;
    virtual void SetFontSize(int size) = 0;
    virtual void SetColors(COLORREF foreground, COLORREF background) = 0;
    virtual void SetSize(int width, int height) = 0;

    // Event handling
    virtual void SetEventCallback(std::function<void(const TerminalEvent&)> callback) = 0;

    // Layout
    virtual void Resize(const RECT& rect) = 0;
    virtual void Focus() = 0;
    virtual bool HasFocus() const = 0;

    // Buffer operations
    virtual std::vector<std::vector<TerminalCell>> GetBuffer() const = 0;
    virtual std::vector<std::vector<TerminalCell>> GetVisibleBuffer() const = 0;
    virtual std::wstring GetBufferText() const = 0;
    virtual std::wstring GetVisibleText() const = 0;
    virtual int GetBufferHeight() const = 0;
    virtual int GetBufferWidth() const = 0;

    // History
    virtual void AddToHistory(const std::wstring& command) = 0;
    virtual std::vector<std::wstring> GetHistory() const = 0;
    virtual void ClearHistory() = 0;
    virtual void NavigateHistory(bool up) = 0;

    // Search
    virtual void Search(const std::wstring& text, bool caseSensitive = false, bool regex = false) = 0;
    virtual void SearchNext() = 0;
    virtual void SearchPrevious() = 0;
    virtual void ClearSearch() = 0;
};

// Win32 RichEdit-based terminal emulator
class Win32TerminalEmulator : public ITerminalEmulator {
private:
    HWND m_hwndTerminal;
    HWND m_hwndParent;
    RECT m_rect;
    TerminalConfig m_config;
    std::function<void(const TerminalEvent&)> m_eventCallback;

    // Process management
    HANDLE m_hProcess = nullptr;
    HANDLE m_hThread = nullptr;
    HANDLE m_hInputRead = nullptr;
    HANDLE m_hInputWrite = nullptr;
    HANDLE m_hOutputRead = nullptr;
    HANDLE m_hOutputWrite = nullptr;
    HANDLE m_hErrorRead = nullptr;
    HANDLE m_hErrorWrite = nullptr;

    // Threads
    std::thread m_outputThread;
    std::thread m_errorThread;
    std::atomic<bool> m_running;
    std::mutex m_mutex;

    // Buffer and display
    std::vector<std::vector<TerminalCell>> m_buffer;
    TerminalPosition m_cursor;
    TerminalSelection m_selection;
    int m_scrollOffset = 0;
    std::queue<std::wstring> m_outputQueue;
    std::queue<std::wstring> m_errorQueue;

    // History
    std::vector<std::wstring> m_history;
    int m_historyIndex = -1;
    std::wstring m_currentCommand;

    // Search
    std::wstring m_searchText;
    bool m_searchCaseSensitive = false;
    bool m_searchRegex = false;
    int m_searchPosition = -1;

public:
    Win32TerminalEmulator();
    ~Win32TerminalEmulator() override;

    // ITerminalEmulator implementation
    bool Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) override;
    void Shutdown() override;
    bool IsInitialized() const override { return m_hwndTerminal != nullptr; }
    bool IsRunning() const override { return m_running.load(); }

    bool StartProcess(const std::wstring& command = L"") override;
    void StopProcess() override;
    void SendInput(const std::wstring& input) override;
    void SendKey(WORD virtualKey, bool ctrl = false, bool alt = false, bool shift = false) override;
    TerminalCommandResult ExecuteCommand(const std::wstring& command, int timeoutMs = 30000) override;

    void Clear() override;
    void ClearLine() override;
    void ClearToEndOfLine() override;
    void ClearToEndOfScreen() override;
    void ScrollUp(int lines = 1) override;
    void ScrollDown(int lines = 1) override;
    void ScrollToTop() override;
    void ScrollToBottom() override;

    void SetCursorPosition(int x, int y) override;
    TerminalPosition GetCursorPosition() const override { return m_cursor; }
    void ShowCursor(bool show) override;
    void SetCursorStyle(const std::wstring& style) override;

    void SelectAll() override;
    void SelectNone() override;
    void SelectWord() override;
    void SelectLine() override;
    void CopySelection() override;
    void PasteClipboard() override;
    TerminalSelection GetSelection() const override { return m_selection; }
    std::wstring GetSelectedText() const override;

    void UpdateConfig(const TerminalConfig& config) override;
    void SetFontSize(int size) override;
    void SetColors(COLORREF foreground, COLORREF background) override;
    void SetSize(int width, int height) override;

    void SetEventCallback(std::function<void(const TerminalEvent&)> callback) override { m_eventCallback = callback; }

    void Resize(const RECT& rect) override;
    void Focus() override;
    bool HasFocus() const override;

    std::vector<std::vector<TerminalCell>> GetBuffer() const override { return m_buffer; }
    std::vector<std::vector<TerminalCell>> GetVisibleBuffer() const override;
    std::wstring GetBufferText() const override;
    std::wstring GetVisibleText() const override;
    int GetBufferHeight() const override { return static_cast<int>(m_buffer.size()); }
    int GetBufferWidth() const override { return m_buffer.empty() ? 0 : static_cast<int>(m_buffer[0].size()); }

    void AddToHistory(const std::wstring& command) override;
    std::vector<std::wstring> GetHistory() const override { return m_history; }
    void ClearHistory() override;
    void NavigateHistory(bool up) override;

    void Search(const std::wstring& text, bool caseSensitive = false, bool regex = false) override;
    void SearchNext() override;
    void SearchPrevious() override;
    void ClearSearch() override;

private:
    // Helper methods
    void CreateTerminalWindow(HWND parent, const RECT& rect);
    void DestroyTerminalWindow();
    void InitializeProcessHandles();
    void CleanupProcessHandles();
    void StartOutputThreads();
    void StopOutputThreads();
    void ProcessOutput();
    void ProcessError();
    void UpdateDisplay();
    void WriteToTerminal(const std::wstring& text, bool isError = false);
    void HandleResize();
    void HandleKeyInput(WPARAM wParam, LPARAM lParam);
    void HandleMouseInput(UINT message, WPARAM wParam, LPARAM lParam);
    void UpdateSelection();
    std::wstring GetWordAtPosition(int x, int y);
    std::wstring GetLineAtPosition(int y);
    void ApplyColors();
    void UpdateScrollbar();
    void ScrollToOffset(int offset);

    // RichEdit operations
    void SetRichEditText(const std::wstring& text);
    std::wstring GetRichEditText() const;
    void SetRichEditSelection(int start, int end);
    void GetRichEditSelection(int& start, int& end) const;
    void SetRichEditFont();
    void SetRichEditColors();

    // Static window procedure
    static LRESULT CALLBACK TerminalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Factory function
std::unique_ptr<ITerminalEmulator> CreateTerminalEmulator();

// Utility functions
bool IsProcessRunning(HANDLE hProcess);
std::wstring GetCurrentDirectory();
std::vector<std::wstring> GetEnvironmentVariables();
void SetEnvironmentVariable(const std::wstring& name, const std::wstring& value);
std::wstring ExpandEnvironmentVariables(const std::wstring& str);

// Error handling
class TerminalException : public std::exception {
public:
    TerminalException(const std::string& message, DWORD errorCode = 0) : m_message(message), m_errorCode(errorCode) {}
    const char* what() const noexcept override { return m_message.c_str(); }
    DWORD GetErrorCode() const { return m_errorCode; }

private:
    std::string m_message;
    DWORD m_errorCode;
};

// Global terminal state
extern bool g_terminalInitialized;
extern std::unique_ptr<ITerminalEmulator> g_terminalEmulator;

#endif // TERMINAL_EMULATOR_HPP