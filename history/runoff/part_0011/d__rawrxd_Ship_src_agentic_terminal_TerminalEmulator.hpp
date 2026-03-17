#pragma once
// ============================================================================
// TerminalEmulator.hpp - Reverse-engineered from RawrXD_Win32_IDE.cpp callsites
//                        and TerminalEmulator.cpp implementation
// ============================================================================
//
// Dependency chain (walked backwards from the IDE):
//
//   RawrXD_Win32_IDE.cpp
//     line 105:  std::unique_ptr<RawrXD::Agentic::TerminalEmulator> g_terminalEmulator;
//     line 2137: wc.lpfnWndProc = RawrXD::Agentic::TerminalEmulator::WndProc;
//     line 2144: g_terminalEmulator = make_unique<...TerminalEmulator>(hwndParent, rect);
//     line 2145: g_terminalEmulator->getHWND()
//     line 2148: g_terminalEmulator->setOnOutput(...)
//     line 2153: g_terminalEmulator->setOnTitleChange(...)
//     line 2234: g_terminalEmulator->resize(terminalRect)
//
//   TerminalEmulator.cpp  (873 lines)
//     Uses: TerminalCell, TerminalPosition, TerminalSelection structs
//     Uses: m_buffer as vector<vector<TerminalCell>>
//     Uses: m_cursor as TerminalPosition (with .x, .y, .visible, .style)
//     Uses: m_selection as TerminalSelection
//     Uses: m_history, m_currentCommand, m_searchText, m_searchCaseSensitive, m_searchRegex
//     Uses: m_mutex (std::mutex)
//     Uses: m_hErrorRead (HANDLE)
//     Methods: WndProc/TerminalWndProc, HandleMessage, HandleMouseInput,
//              ApplyColors, WriteToTerminal, ProcessOutput, ProcessError,
//              GetWordAtPosition, GetLineAtPosition, HandleResize, UpdateSelection
//
// ============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// Structs — reverse-engineered from TerminalEmulator.cpp member usage
// ============================================================================

struct TerminalConfig {
    std::wstring shell          = L"powershell.exe";
    std::wstring workingDirectory = L"";
    int          width          = 80;
    int          height         = 24;
    int          fontSize       = 12;
    std::wstring fontFamily     = L"Consolas";
    COLORREF     backgroundColor = RGB(12, 12, 12);
    COLORREF     foregroundColor = RGB(204, 204, 204);
};

// Per-character cell in the terminal buffer
struct TerminalCell {
    WCHAR    character       = L' ';
    COLORREF foregroundColor = RGB(204, 204, 204);
    COLORREF backgroundColor = RGB(12, 12, 12);
    bool bold          = false;
    bool italic        = false;
    bool underline     = false;
    bool strikethrough = false;
    bool blink         = false;
    bool reverse       = false;
    bool invisible     = false;
};

// Cursor position (CPP uses .x, .y, .visible, .style)
struct TerminalPosition {
    int  x       = 0;
    int  y       = 0;
    bool visible = true;
    std::wstring style = L"block";   // "block", "underline", "bar"
};

// Selection range (CPP uses .start, .end, .active, .text)
struct TerminalSelection {
    TerminalPosition start;
    TerminalPosition end;
    bool             active = false;
    std::wstring     text;
};

// Event types
enum class TerminalEventType {
    Output,
    Error,
    Exit,
    TitleChange,
    Bell,
    Resize,
    Focus,
    Blur,
    SelectionChanged,
    Scroll
};

struct TerminalEvent {
    TerminalEventType type;
    std::wstring      data;
    int               exitCode  = 0;
    long long         timestamp = 0;
};

struct TerminalCommandResult {
    bool         success       = false;
    int          exitCode      = 0;
    std::wstring output;
    std::wstring error;
    long long    executionTime = 0;
};

using TerminalEventCallback  = std::function<void(const TerminalEvent&)>;
using TerminalOutputCallback = std::function<void(const std::string&)>;

// ============================================================================
// Interface — union of all virtuals called by IDE + CPP
// ============================================================================

class ITerminalEmulator {
public:
    virtual ~ITerminalEmulator() = default;

    // Core lifecycle
    virtual bool Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;
    virtual bool IsRunning()     const = 0;

    // Process control
    virtual bool StartProcess(const std::wstring& command = L"") = 0;
    virtual void StopProcess() = 0;
    virtual void SendInput(const std::wstring& input) = 0;
    virtual void SendKey(WORD virtualKey, bool ctrl = false, bool alt = false, bool shift = false) = 0;
    virtual TerminalCommandResult ExecuteCommand(const std::wstring& command, int timeoutMs = 30000) = 0;

    // Display
    virtual void Clear() = 0;
    virtual void ClearLine() = 0;
    virtual void ClearToEndOfLine() = 0;
    virtual void ClearToEndOfScreen() = 0;
    virtual void ScrollUp(int lines = 1) = 0;
    virtual void ScrollDown(int lines = 1) = 0;
    virtual void ScrollToTop() = 0;
    virtual void ScrollToBottom() = 0;

    // Cursor
    virtual void SetCursorPosition(int x, int y) = 0;
    virtual TerminalPosition GetCursorPosition() const = 0;
    virtual void ShowCursor(bool show) = 0;
    virtual void SetCursorStyle(const std::wstring& style) = 0;

    // Selection / Clipboard
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

    // Events
    virtual void SetEventCallback(TerminalEventCallback callback) = 0;

    // Layout
    virtual void Resize(const RECT& rect) = 0;
    virtual void Focus() = 0;
    virtual bool HasFocus() const = 0;

    // Buffer access
    virtual std::vector<std::vector<TerminalCell>> GetBuffer()        const = 0;
    virtual std::vector<std::vector<TerminalCell>> GetVisibleBuffer() const = 0;
    virtual std::wstring GetBufferText()  const = 0;
    virtual std::wstring GetVisibleText() const = 0;
    virtual int GetBufferHeight() const = 0;
    virtual int GetBufferWidth()  const = 0;

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

    // HWND access
    virtual HWND GetHwnd() const = 0;

    // ── Backward-compat shims expected by RawrXD_Win32_IDE.cpp ──
    virtual void setOnOutput(TerminalOutputCallback callback) = 0;
    virtual void setOnTitleChange(TerminalOutputCallback callback) = 0;
    virtual void resize(const RECT& rect) = 0;
    virtual HWND getHWND() const = 0;
};

// ============================================================================
// Win32TerminalEmulator — concrete implementation
// ============================================================================

class Win32TerminalEmulator : public ITerminalEmulator {
public:
    Win32TerminalEmulator();
    Win32TerminalEmulator(HWND parent, const RECT& rect);   // called by IDE
    virtual ~Win32TerminalEmulator();

    // ── Static window proc referenced by IDE line 2137 ──
    // IDE uses:  wc.lpfnWndProc = TerminalEmulator::WndProc;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return TerminalWndProc(hwnd, msg, wParam, lParam);
    }

    // ── ITerminalEmulator overrides ──
    bool Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) override;
    void Shutdown() override;
    bool IsInitialized() const override { return m_hwndTerminal != nullptr; }
    bool IsRunning()     const override;

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

    void SetEventCallback(TerminalEventCallback callback) override { m_eventCallback = callback; }

    void Resize(const RECT& rect) override;
    void Focus() override;
    bool HasFocus() const override;

    std::vector<std::vector<TerminalCell>> GetBuffer()        const override { return m_buffer; }
    std::vector<std::vector<TerminalCell>> GetVisibleBuffer() const override;
    std::wstring GetBufferText()  const override;
    std::wstring GetVisibleText() const override;
    int GetBufferHeight() const override { return static_cast<int>(m_buffer.size()); }
    int GetBufferWidth()  const override { return m_config.width; }

    void AddToHistory(const std::wstring& command) override;
    std::vector<std::wstring> GetHistory() const override { return m_history; }
    void ClearHistory() override;
    void NavigateHistory(bool up) override;

    void Search(const std::wstring& text, bool caseSensitive = false, bool regex = false) override;
    void SearchNext() override;
    void SearchPrevious() override;
    void ClearSearch() override;

    HWND GetHwnd() const override { return m_hwndTerminal; }

    // Backward-compat shims
    void setOnOutput(TerminalOutputCallback callback) override     { m_compatOutputCallback = callback; }
    void setOnTitleChange(TerminalOutputCallback callback) override { m_compatTitleCallback = callback; }
    void resize(const RECT& rect) override                         { Resize(rect); }
    HWND getHWND() const override                                  { return m_hwndTerminal; }

protected:
    // ── Window ──
    HWND m_hwndTerminal = nullptr;
    HWND m_hwndParent   = nullptr;
    RECT m_rect         = {};
    TerminalConfig m_config;

    // ── Process handles ──
    HANDLE m_hProcess     = nullptr;
    HANDLE m_hThread      = nullptr;
    HANDLE m_hInputRead   = nullptr;
    HANDLE m_hInputWrite  = nullptr;
    HANDLE m_hOutputRead  = nullptr;
    HANDLE m_hOutputWrite = nullptr;
    HANDLE m_hErrorRead   = nullptr;   // used by ProcessError()
    HANDLE m_hErrorWrite  = nullptr;

    // ── Threading ──
    std::atomic<bool> m_running{false};
    std::thread       m_outputThread;
    std::thread       m_errorThread;
    std::mutex        m_mutex;         // used by WriteToTerminal()

    // ── Callbacks ──
    TerminalEventCallback  m_eventCallback;
    TerminalOutputCallback m_compatOutputCallback;
    TerminalOutputCallback m_compatTitleCallback;

    // ── Buffer (vector of rows of cells) ──
    std::vector<std::vector<TerminalCell>> m_buffer;

    // ── Cursor ──
    TerminalPosition m_cursor;

    // ── Selection ──
    TerminalSelection m_selection;

    // ── Scrolling ──
    int m_scrollOffset = 0;

    // ── Command history ──
    std::vector<std::wstring> m_history;
    std::vector<std::wstring> m_commandHistory;   // alias used in some paths
    int          m_historyIndex   = -1;
    std::wstring m_currentCommand;

    // ── Search ──
    std::wstring m_searchText;
    int          m_searchPosition      = -1;
    bool         m_searchCaseSensitive = false;
    bool         m_searchRegex         = false;
    std::wstring m_currentSearch;                  // legacy alias

    // ── Private helpers ──
    void CreateTerminalWindow(HWND parent, const RECT& rect);
    void DestroyTerminalWindow();

    void InitializeProcessHandles();
    void CleanupProcessHandles();

    void StartOutputThreads();
    void StopOutputThreads();

    // Thread entry points (CPP signatures)
    void ProcessOutput();                          // reads m_hOutputRead
    void ProcessError();                           // reads m_hErrorRead

    void WriteToTerminal(const std::wstring& text, bool isError);

    void UpdateDisplay();
    void UpdateScrollbar();
    void UpdateSelection();
    void HandleResize();

    void ApplyColors();

    void ParseAnsiCodes(const std::string& text);

    // Window-message plumbing
    static LRESULT CALLBACK TerminalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void HandleKeyInput(WPARAM wParam, LPARAM lParam);
    void HandleMouseInput(UINT message, WPARAM wParam, LPARAM lParam);
    void HandleMouseWheel(int delta);
    void HandlePaint();

    // Helpers
    std::wstring GetWordAtPosition(int x, int y);
    std::wstring GetLineAtPosition(int y);

    // Legacy helpers that exist in old ReadOutputLoop / ReadErrorLoop paths
    void ReadOutputLoop() { ProcessOutput(); }
    void ReadErrorLoop()  { ProcessError();  }
    void ProcessOutput(const std::string& output);   // overload from old code
};

// Type alias so the IDE can write  RawrXD::Agentic::TerminalEmulator
using TerminalEmulator = Win32TerminalEmulator;

// Factory
std::unique_ptr<ITerminalEmulator> CreateTerminalEmulator();

// Utility functions (defined in CPP)
bool IsProcessRunning(HANDLE hProcess);
std::wstring GetCurrentDirectory();
std::vector<std::wstring> GetEnvironmentVariables();
void SetEnvironmentVariable(const std::wstring& name, const std::wstring& value);
std::wstring ExpandEnvironmentVariables(const std::wstring& str);

} // namespace Agentic
} // namespace RawrXD
