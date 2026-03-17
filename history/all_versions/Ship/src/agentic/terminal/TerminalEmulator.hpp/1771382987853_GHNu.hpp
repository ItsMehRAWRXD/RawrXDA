#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

// ============================================================================
// Terminal Emulator for RawrXD Win32 IDE
// ============================================================================

namespace RawrXD {
namespace Agentic {

// Terminal configuration
struct TerminalConfig {
    std::wstring shell = L"powershell.exe";
    std::wstring workingDirectory = L"";
    int width = 80;
    int height = 24;
    int fontSize = 12;
    std::wstring fontFamily = L"Consolas";
    DWORD backgroundColor = 0x0C0C0C;
    DWORD foregroundColor = 0xCCCCCC;
};

enum class TerminalEventType {
    Output,
    Error,
    Exit,
    TitleChange
};

struct TerminalEvent {
    TerminalEventType type;
    std::wstring data;
    long long timestamp;
};

using TerminalEventCallback = std::function<void(const TerminalEvent&)>;
using TerminalOutputCallback = std::function<void(const std::string&)>;

struct TerminalCommandResult {
    bool success;
    int exitCode;
    std::wstring output;
    std::wstring error;
    long long executionTime;
};

// Abstract Interface
class ITerminalEmulator {
public:
    virtual ~ITerminalEmulator() = default;

    virtual bool Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) = 0;
    virtual void Shutdown() = 0;

    virtual bool StartProcess(const std::wstring& command = L"") = 0;
    virtual void StopProcess() = 0;
    virtual bool IsRunning() const = 0;

    virtual void SendInput(const std::wstring& input) = 0;
    virtual void SendKey(WORD virtualKey, bool ctrl, bool alt, bool shift) = 0;

    virtual TerminalCommandResult ExecuteCommand(const std::wstring& command, int timeoutMs = 5000) = 0;

    virtual void SetSize(int rows, int cols) = 0;
    virtual void Resize(const RECT& rect) = 0;
    
    virtual void Clear() = 0;
    
    virtual void SetEventCallback(TerminalEventCallback callback) = 0;
    
    // Helper to get HWND
    virtual HWND GetHwnd() const = 0;

    // Backward compatibility helpers for RawrXD_Win32_IDE.cpp
    virtual void setOnOutput(TerminalOutputCallback callback) = 0;
    virtual void setOnTitleChange(TerminalOutputCallback callback) = 0;
    virtual void resize(const RECT& rect) = 0;
    virtual HWND getHWND() const = 0;
};

// Implementations
class Win32TerminalEmulator : public ITerminalEmulator {
public:
    Win32TerminalEmulator();
    // Constructor matching RawrXD Usage:
    Win32TerminalEmulator(HWND parent, const RECT& rect);
    
    virtual ~Win32TerminalEmulator();

    bool Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) override;
    void Shutdown() override;

    bool StartProcess(const std::wstring& command = L"") override;
    void StopProcess() override;
    bool IsRunning() const override;

    void SendInput(const std::wstring& input) override;
    void SendKey(WORD virtualKey, bool ctrl, bool alt, bool shift) override;

    TerminalCommandResult ExecuteCommand(const std::wstring& command, int timeoutMs = 5000) override;

    void SetSize(int rows, int cols) override;
    void Resize(const RECT& rect) override;
    
    void Clear() override;
    
    void SetEventCallback(TerminalEventCallback callback) override { m_eventCallback = callback; }

    HWND GetHwnd() const override { return m_hwndTerminal; }

    // Compat implementations
    void setOnOutput(TerminalOutputCallback callback) override;
    void setOnTitleChange(TerminalOutputCallback callback) override;
    void resize(const RECT& rect) override { Resize(rect); }
    HWND getHWND() const override { return m_hwndTerminal; }

protected:
    HWND m_hwndTerminal;
    HWND m_hwndParent;
    RECT m_rect;
    TerminalConfig m_config;
    
    // Process handles
    HANDLE m_hProcess;
    HANDLE m_hThread;
    HANDLE m_hInputRead;
    HANDLE m_hInputWrite;
    HANDLE m_hOutputRead;
    HANDLE m_hOutputWrite;
    HANDLE m_hErrorWrite;
    
    std::atomic<bool> m_running;
    std::thread m_outputThread;
    std::thread m_errorThread;
    
    TerminalEventCallback m_eventCallback;
    TerminalOutputCallback m_compatOutputCallback;
    TerminalOutputCallback m_compatTitleCallback;

    // Terminal Buffer
    std::vector<std::wstring> m_buffer;
    POINT m_cursor;
    int m_scrollOffset;
    
    // Search/History
    std::vector<std::wstring> m_commandHistory;
    int m_historyIndex;
    int m_searchPosition;
    std::wstring m_currentSearch;

    void CreateTerminalWindow(HWND parent, const RECT& rect);
    void DestroyTerminalWindow();
    
    void InitializeProcessHandles();
    void CleanupProcessHandles();
    
    void StartOutputThreads();
    void StopOutputThreads();
    void ReadOutputLoop();
    void ReadErrorLoop();
    
    void ProcessOutput(const std::string& output);
    void UpdateDisplay();
    void UpdateScrollbar();
    
    // ANSI Escape Code Parsing
    void ParseAnsiCodes(const std::string& text);
    
    // Window Msg
    static LRESULT CALLBACK TerminalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void HandlePaint();
    void HandleKeyInput(WPARAM wParam, LPARAM lParam);
    void HandleMouseWheel(int delta);
    
    // Helper
    void ClearLine();
    void ClearToEndOfLine();
    void ClearToEndOfScreen();
    void ScrollUp(int lines = 1);
    void ScrollDown(int lines = 1);
};

// For compatibility with g_terminalEmulator usage in RawrXD
using TerminalEmulator = Win32TerminalEmulator;

} // Consumer
} // RawrXD

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