#include "TerminalEmulator.hpp"
#include <Richedit.h>
#include <algorithm>

using namespace RawrXD::Agentic;

// Load RichEdit library
#pragma comment(lib, "riched20.lib")

// ============================================================================
// Win32TerminalEmulator Implementation
// ============================================================================

Win32TerminalEmulator::Win32TerminalEmulator()
    : m_hwndTerminal(nullptr)
    , m_hwndParent(nullptr)
    , m_running(false)
    , m_scrollOffset(0)
    , m_historyIndex(-1)
    , m_searchPosition(-1)
{
    // Load RichEdit
    LoadLibraryW(L"riched20.dll");
}

Win32TerminalEmulator::Win32TerminalEmulator(HWND parent, const RECT& rect) 
    : Win32TerminalEmulator()
{
    Initialize(parent, rect, m_config);
}

Win32TerminalEmulator::~Win32TerminalEmulator() {
    Shutdown();
}

bool Win32TerminalEmulator::Initialize(HWND parentWindow, const RECT& rect, const TerminalConfig& config) {
    if (m_hwndTerminal) return true;

    m_hwndParent = parentWindow;
    m_rect = rect;
    m_config = config;

    CreateTerminalWindow(parentWindow, rect);
    if (!m_hwndTerminal) return false;

    InitializeProcessHandles();
    StartOutputThreads();

    return true;
}

void Win32TerminalEmulator::Shutdown() {
    StopProcess();
    StopOutputThreads();
    CleanupProcessHandles();
    DestroyTerminalWindow();
}

bool Win32TerminalEmulator::IsRunning() const {
    return m_running.load();
}

bool Win32TerminalEmulator::StartProcess(const std::wstring& command) {
    if (m_running) return true;

    std::wstring cmdLine = command.empty() ? m_config.shell : command;

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = m_hInputRead;
    si.hStdOutput = m_hOutputWrite;
    si.hStdError = m_hErrorWrite;

    PROCESS_INFORMATION pi = {0};

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    if (CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE,
                      CREATE_NO_WINDOW, nullptr,
                      m_config.workingDirectory.empty() ? nullptr : m_config.workingDirectory.c_str(),
                      &si, &pi)) {

        m_hProcess = pi.hProcess;
        m_hThread = pi.hThread;
        m_running = true;

        if (m_eventCallback) {
            TerminalEvent event;
            event.type = TerminalEventType::Output;
            event.data = L"Process started: " + cmdLine + L"\r\n";
            event.timestamp = GetTickCount64();
            m_eventCallback(event);
        }

        return true;
    }

    return false;
}

void Win32TerminalEmulator::StopProcess() {
    if (!m_running) return;

    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        WaitForSingleObject(m_hProcess, 5000);
        CloseHandle(m_hProcess);
        CloseHandle(m_hThread);
        m_hProcess = nullptr;
        m_hThread = nullptr;
    }

    m_running = false;
}

void Win32TerminalEmulator::SendInput(const std::wstring& input) {
    if (!m_running || !m_hInputWrite) return;

    DWORD written;
    WriteFile(m_hInputWrite, input.c_str(), input.length() * sizeof(wchar_t), &written, nullptr);
}

void Win32TerminalEmulator::SendKey(WORD virtualKey, bool ctrl, bool alt, bool shift) {
    if (!m_hwndTerminal) return;

    // Send key to terminal window
    UINT scanCode = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
    LPARAM lParam = (scanCode << 16) | 1; // Repeat count = 1

    if (ctrl) lParam |= (1 << 29);  // CTRL pressed
    if (alt) lParam |= (1 << 29) | (1 << 28);  // ALT pressed
    if (shift) lParam |= (1 << 30); // SHIFT pressed

    PostMessage(m_hwndTerminal, WM_KEYDOWN, virtualKey, lParam);
    PostMessage(m_hwndTerminal, WM_KEYUP, virtualKey, lParam | (1 << 31));
}

TerminalCommandResult Win32TerminalEmulator::ExecuteCommand(const std::wstring& command, int timeoutMs) {
    TerminalCommandResult result;

    if (!StartProcess(command)) {
        result.success = false;
        result.exitCode = -1;
        result.error = L"Failed to start process";
        return result;
    }

    // Wait for completion
    DWORD waitResult = WaitForSingleObject(m_hProcess, timeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode;
        GetExitCodeProcess(m_hProcess, &exitCode);
        result.exitCode = exitCode;
        result.success = (exitCode == 0);
    } else {
        result.success = false;
        result.error = L"Command timed out";
        StopProcess();
    }

    result.executionTime = GetTickCount64() - result.executionTime;
    return result;
}

void Win32TerminalEmulator::Clear() {
    if (m_hwndTerminal) {
        SetWindowTextW(m_hwndTerminal, L"");
        m_buffer.clear();
        m_cursor = {0, 0};
    }
}

void Win32TerminalEmulator::ClearLine() {
    if (m_hwndTerminal && !m_buffer.empty()) {
        m_buffer.back().clear();
        UpdateDisplay();
    }
}

void Win32TerminalEmulator::ClearToEndOfLine() {
    if (m_hwndTerminal && !m_buffer.empty()) {
        auto& line = m_buffer[m_cursor.y];
        if (m_cursor.x < static_cast<int>(line.size())) {
            line.resize(m_cursor.x);
            UpdateDisplay();
        }
    }
}

void Win32TerminalEmulator::ClearToEndOfScreen() {
    if (m_hwndTerminal) {
        m_buffer.resize(m_cursor.y + 1);
        UpdateDisplay();
    }
}

void Win32TerminalEmulator::ScrollUp(int lines) {
    m_scrollOffset = std::max(0, m_scrollOffset - lines);
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::ScrollDown(int lines) {
    int maxOffset = std::max(0, static_cast<int>(m_buffer.size()) - m_config.height);
    m_scrollOffset = std::min(maxOffset, m_scrollOffset + lines);
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::ScrollToTop() {
    m_scrollOffset = 0;
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::ScrollToBottom() {
    int maxOffset = std::max(0, static_cast<int>(m_buffer.size()) - m_config.height);
    m_scrollOffset = maxOffset;
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::SetCursorPosition(int x, int y) {
    m_cursor.x = std::max(0, std::min(x, m_config.width - 1));
    m_cursor.y = std::max(0, std::min(y, m_config.height - 1));
    UpdateDisplay();
}

void Win32TerminalEmulator::ShowCursor(bool show) {
    m_cursor.visible = show;
    UpdateDisplay();
}

void Win32TerminalEmulator::SetCursorStyle(const std::wstring& style) {
    m_cursor.style = style;
    UpdateDisplay();
}

void Win32TerminalEmulator::SelectAll() {
    m_selection.active = true;
    m_selection.start = {0, 0};
    m_selection.end = {m_config.width - 1, m_config.height - 1};
    UpdateDisplay();
}

void Win32TerminalEmulator::SelectNone() {
    m_selection.active = false;
    UpdateDisplay();
}

void Win32TerminalEmulator::SelectWord() {
    // Basic word selection - find word boundaries
    if (m_buffer.empty()) return;

    auto& line = m_buffer[m_cursor.y];
    if (m_cursor.x >= static_cast<int>(line.size())) return;

    // Find word start
    int start = m_cursor.x;
    while (start > 0 && iswalnum(line[start - 1].character)) start--;

    // Find word end
    int end = m_cursor.x;
    while (end < static_cast<int>(line.size()) && iswalnum(line[end].character)) end++;

    m_selection.active = true;
    m_selection.start = {start, m_cursor.y};
    m_selection.end = {end, m_cursor.y};
    UpdateDisplay();
}

void Win32TerminalEmulator::SelectLine() {
    m_selection.active = true;
    m_selection.start = {0, m_cursor.y};
    m_selection.end = {m_config.width - 1, m_cursor.y};
    UpdateDisplay();
}

void Win32TerminalEmulator::CopySelection() {
    if (!m_selection.active) return;

    std::wstring selectedText = GetSelectedText();
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (selectedText.length() + 1) * sizeof(wchar_t));
        if (hGlobal) {
            LPWSTR pGlobal = (LPWSTR)GlobalLock(hGlobal);
            wcscpy_s(pGlobal, selectedText.length() + 1, selectedText.c_str());
            GlobalUnlock(hGlobal);
            SetClipboardData(CF_UNICODETEXT, hGlobal);
        }
        CloseClipboard();
    }
}

void Win32TerminalEmulator::PasteClipboard() {
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            LPCWSTR pText = (LPCWSTR)GlobalLock(hData);
            if (pText) {
                SendInput(pText);
            }
            GlobalUnlock(hData);
        }
        CloseClipboard();
    }
}

std::wstring Win32TerminalEmulator::GetSelectedText() const {
    if (!m_selection.active) return L"";

    std::wstring result;
    int startY = std::min(m_selection.start.y, m_selection.end.y);
    int endY = std::max(m_selection.start.y, m_selection.end.y);

    for (int y = startY; y <= endY; ++y) {
        if (y >= static_cast<int>(m_buffer.size())) break;

        const auto& line = m_buffer[y];
        int startX = (y == m_selection.start.y) ? m_selection.start.x : 0;
        int endX = (y == m_selection.end.y) ? m_selection.end.x : static_cast<int>(line.size()) - 1;

        startX = std::max(0, std::min(startX, static_cast<int>(line.size()) - 1));
        endX = std::max(0, std::min(endX, static_cast<int>(line.size()) - 1));

        for (int x = startX; x <= endX; ++x) {
            result += line[x].character;
        }

        if (y < endY) result += L"\r\n";
    }

    return result;
}

void Win32TerminalEmulator::UpdateConfig(const TerminalConfig& config) {
    m_config = config;
    ApplyColors();
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::SetFontSize(int size) {
    m_config.fontSize = size;
    ApplyColors();
}

void Win32TerminalEmulator::SetColors(COLORREF foreground, COLORREF background) {
    m_config.foregroundColor = foreground;
    m_config.backgroundColor = background;
    ApplyColors();
}

void Win32TerminalEmulator::SetSize(int width, int height) {
    m_config.width = width;
    m_config.height = height;
    m_buffer.resize(height);
    for (auto& line : m_buffer) {
        line.resize(width);
    }
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::Resize(const RECT& rect) {
    m_rect = rect;
    if (m_hwndTerminal) {
        MoveWindow(m_hwndTerminal, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    }
}

void Win32TerminalEmulator::Focus() {
    if (m_hwndTerminal) {
        SetFocus(m_hwndTerminal);
    }
}

bool Win32TerminalEmulator::HasFocus() const {
    return m_hwndTerminal && GetFocus() == m_hwndTerminal;
}

void Win32TerminalEmulator::AddToHistory(const std::wstring& command) {
    if (!command.empty()) {
        m_history.push_back(command);
        m_historyIndex = -1;
    }
}

void Win32TerminalEmulator::ClearHistory() {
    m_history.clear();
    m_historyIndex = -1;
}

void Win32TerminalEmulator::NavigateHistory(bool up) {
    if (m_history.empty()) return;

    if (up) {
        if (m_historyIndex < static_cast<int>(m_history.size()) - 1) {
            m_historyIndex++;
        }
    } else {
        if (m_historyIndex > 0) {
            m_historyIndex--;
        } else {
            m_historyIndex = -1;
            return;
        }
    }

    if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_history.size())) {
        m_currentCommand = m_history[m_history.size() - 1 - m_historyIndex];
        // Position cursor at end of command
        SetCursorPosition(m_currentCommand.length(), m_cursor.y);
    }
}

void Win32TerminalEmulator::Search(const std::wstring& text, bool caseSensitive, bool regex) {
    m_searchText = text;
    m_searchCaseSensitive = caseSensitive;
    m_searchRegex = regex;
    m_searchPosition = -1;

    if (!text.empty()) {
        SearchNext();
    }
}

void Win32TerminalEmulator::SearchNext() {
    if (m_searchText.empty() || m_buffer.empty()) return;

    int startPos = m_searchPosition + 1;
    for (size_t y = 0; y < m_buffer.size(); ++y) {
        const auto& line = m_buffer[y];
        std::wstring lineText;
        for (const auto& cell : line) {
            lineText += cell.character;
        }

        size_t pos = lineText.find(m_searchText, startPos);
        if (pos != std::wstring::npos) {
            m_searchPosition = pos;
            m_cursor.x = pos;
            m_cursor.y = y;
            UpdateDisplay();
            return;
        }
        startPos = 0;
    }

    m_searchPosition = -1; // Not found
}

void Win32TerminalEmulator::SearchPrevious() {
    if (m_searchText.empty() || m_buffer.empty()) return;

    int startPos = m_searchPosition - 1;
    if (startPos < 0) startPos = 0;

    for (int y = m_buffer.size() - 1; y >= 0; --y) {
        const auto& line = m_buffer[y];
        std::wstring lineText;
        for (const auto& cell : line) {
            lineText += cell.character;
        }

        size_t pos = lineText.rfind(m_searchText, startPos);
        if (pos != std::wstring::npos) {
            m_searchPosition = pos;
            m_cursor.x = pos;
            m_cursor.y = y;
            UpdateDisplay();
            return;
        }
    }

    m_searchPosition = -1; // Not found
}

void Win32TerminalEmulator::ClearSearch() {
    m_searchText.clear();
    m_searchPosition = -1;
}

// Private helper methods
void Win32TerminalEmulator::CreateTerminalWindow(HWND parent, const RECT& rect) {
    m_hwndTerminal = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        RICHEDIT_CLASSW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (m_hwndTerminal) {
        // Subclass the window
        SetWindowLongPtr(m_hwndTerminal, GWLP_USERDATA, (LONG_PTR)this);
        SetWindowLongPtr(m_hwndTerminal, GWLP_WNDPROC, (LONG_PTR)TerminalWndProc);

        ApplyColors();
        UpdateScrollbar();
    }
}

void Win32TerminalEmulator::DestroyTerminalWindow() {
    if (m_hwndTerminal) {
        DestroyWindow(m_hwndTerminal);
        m_hwndTerminal = nullptr;
    }
}

void Win32TerminalEmulator::InitializeProcessHandles() {
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    CreatePipe(&m_hInputRead, &m_hInputWrite, &sa, 0);
    CreatePipe(&m_hOutputRead, &m_hOutputWrite, &sa, 0);
    CreatePipe(&m_hErrorRead, &m_hErrorWrite, &sa, 0);
}

void Win32TerminalEmulator::CleanupProcessHandles() {
    if (m_hInputRead) CloseHandle(m_hInputRead);
    if (m_hInputWrite) CloseHandle(m_hInputWrite);
    if (m_hOutputRead) CloseHandle(m_hOutputRead);
    if (m_hOutputWrite) CloseHandle(m_hOutputWrite);
    if (m_hErrorRead) CloseHandle(m_hErrorRead);
    if (m_hErrorWrite) CloseHandle(m_hErrorWrite);

    m_hInputRead = m_hInputWrite = nullptr;
    m_hOutputRead = m_hOutputWrite = nullptr;
    m_hErrorRead = m_hErrorWrite = nullptr;
}

void Win32TerminalEmulator::StartOutputThreads() {
    m_outputThread = std::thread(&Win32TerminalEmulator::ProcessOutput, this);
    m_errorThread = std::thread(&Win32TerminalEmulator::ProcessError, this);
}

void Win32TerminalEmulator::StopOutputThreads() {
    m_running = false;

    if (m_outputThread.joinable()) m_outputThread.join();
    if (m_errorThread.joinable()) m_errorThread.join();
}

void Win32TerminalEmulator::ProcessOutput() {
    char buffer[4096];
    DWORD bytesRead;

    while (m_running) {
        if (ReadFile(m_hOutputRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            std::string output(buffer, bytesRead);
            std::wstring woutput(output.begin(), output.end());
            WriteToTerminal(woutput, false);
        } else {
            Sleep(10);
        }
    }
}

void Win32TerminalEmulator::ProcessError() {
    char buffer[4096];
    DWORD bytesRead;

    while (m_running) {
        if (ReadFile(m_hErrorRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            std::string error(buffer, bytesRead);
            std::wstring werror(error.begin(), error.end());
            WriteToTerminal(werror, true);
        } else {
            Sleep(10);
        }
    }
}

void Win32TerminalEmulator::WriteToTerminal(const std::wstring& text, bool isError) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (wchar_t ch : text) {
        if (ch == L'\n') {
            m_cursor.y++;
            m_cursor.x = 0;
            if (m_cursor.y >= m_config.height) {
                // Scroll up
                m_buffer.erase(m_buffer.begin());
                m_buffer.push_back(std::vector<TerminalCell>(m_config.width));
                m_cursor.y = m_config.height - 1;
            }
        } else if (ch == L'\r') {
            m_cursor.x = 0;
        } else if (ch == L'\t') {
            // Handle tab
            int tabWidth = 4;
            int spaces = tabWidth - (m_cursor.x % tabWidth);
            for (int i = 0; i < spaces; ++i) {
                if (m_cursor.x < m_config.width) {
                    m_buffer[m_cursor.y][m_cursor.x].character = L' ';
                    m_buffer[m_cursor.y][m_cursor.x].foregroundColor = isError ? RGB(255, 100, 100) : m_config.foregroundColor;
                    m_cursor.x++;
                }
            }
        } else {
            if (m_cursor.x < m_config.width && m_cursor.y < m_config.height) {
                m_buffer[m_cursor.y][m_cursor.x].character = ch;
                m_buffer[m_cursor.y][m_cursor.x].foregroundColor = isError ? RGB(255, 100, 100) : m_config.foregroundColor;
                m_cursor.x++;
            }
        }
    }

    UpdateDisplay();
}

void Win32TerminalEmulator::UpdateDisplay() {
    if (!m_hwndTerminal) return;

    std::wstring displayText;
    int startLine = m_scrollOffset;
    int endLine = std::min(startLine + m_config.height, static_cast<int>(m_buffer.size()));

    for (int y = startLine; y < endLine; ++y) {
        const auto& line = m_buffer[y];
        for (const auto& cell : line) {
            displayText += cell.character;
        }
        if (y < endLine - 1) displayText += L"\r\n";
    }

    SetWindowTextW(m_hwndTerminal, displayText.c_str());
}

void Win32TerminalEmulator::HandleResize() {
    UpdateScrollbar();
    UpdateDisplay();
}

void Win32TerminalEmulator::HandleKeyInput(WPARAM wParam, LPARAM lParam) {
    // Handle special keys
    switch (wParam) {
        case VK_UP:
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                NavigateHistory(true);
            }
            return;
        case VK_DOWN:
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                NavigateHistory(false);
            }
            return;
        case 'C':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                CopySelection();
            }
            return;
        case 'V':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                PasteClipboard();
            }
            return;
        case VK_RETURN:
            if (!m_currentCommand.empty()) {
                SendInput(m_currentCommand + L"\n");
                AddToHistory(m_currentCommand);
                m_currentCommand.clear();
            }
            return;
    }
}

void Win32TerminalEmulator::HandleMouseInput(UINT message, WPARAM wParam, LPARAM lParam) {
    // Handle mouse selection
    if (message == WM_LBUTTONDOWN) {
        // Start selection
        int x = LOWORD(lParam) / 8; // Approximate character width
        int y = HIWORD(lParam) / 16; // Approximate line height
        m_selection.start = {x, y + m_scrollOffset};
        m_selection.active = true;
    } else if (message == WM_MOUSEMOVE && (wParam & MK_LBUTTON)) {
        // Update selection
        int x = LOWORD(lParam) / 8;
        int y = HIWORD(lParam) / 16;
        m_selection.end = {x, y + m_scrollOffset};
        UpdateDisplay();
    }
}

void Win32TerminalEmulator::UpdateSelection() {
    // Update visual selection in RichEdit
}

std::wstring Win32TerminalEmulator::GetWordAtPosition(int x, int y) {
    if (y >= static_cast<int>(m_buffer.size())) return L"";
    const auto& line = m_buffer[y];
    if (x >= static_cast<int>(line.size())) return L"";

    // Find word boundaries
    int start = x;
    while (start > 0 && iswalnum(line[start - 1].character)) start--;

    int end = x;
    while (end < static_cast<int>(line.size()) && iswalnum(line[end].character)) end++;

    std::wstring word;
    for (int i = start; i < end; ++i) {
        word += line[i].character;
    }

    return word;
}

std::wstring Win32TerminalEmulator::GetLineAtPosition(int y) {
    if (y >= static_cast<int>(m_buffer.size())) return L"";
    const auto& line = m_buffer[y];

    std::wstring lineText;
    for (const auto& cell : line) {
        lineText += cell.character;
    }

    return lineText;
}

void Win32TerminalEmulator::ApplyColors() {
    if (!m_hwndTerminal) return;

    // Set background color
    SendMessage(m_hwndTerminal, EM_SETBKGNDCOLOR, 0, m_config.backgroundColor);

    // Set text color
    CHARFORMAT2W cf = {0};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = m_config.foregroundColor;
    SendMessage(m_hwndTerminal, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Update font
    LOGFONTW lf = {0};
    lf.lfHeight = -MulDiv(m_config.fontSize, GetDeviceCaps(GetDC(m_hwndTerminal), LOGPIXELSY), 72);
    wcscpy_s(lf.lfFaceName, m_config.fontFamily.c_str());
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;

    HFONT hFont = CreateFontIndirectW(&lf);
    SendMessage(m_hwndTerminal, WM_SETFONT, (WPARAM)hFont, TRUE);
    DeleteObject(hFont);
}

void Win32TerminalEmulator::UpdateScrollbar() {
    if (!m_hwndTerminal) return;

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, static_cast<int>(m_buffer.size()) - 1);
    si.nPage = m_config.height;
    si.nPos = m_scrollOffset;

    SetScrollInfo(m_hwndTerminal, SB_VERT, &si, TRUE);
}

LRESULT CALLBACK Win32TerminalEmulator::TerminalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32TerminalEmulator* terminal = (Win32TerminalEmulator*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (terminal) {
        return terminal->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Win32TerminalEmulator::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            HandleKeyInput(wParam, lParam);
            break;
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
            HandleMouseInput(msg, wParam, lParam);
            break;
        case WM_VSCROLL:
            switch (LOWORD(wParam)) {
                case SB_LINEUP:
                    ScrollUp(1);
                    break;
                case SB_LINEDOWN:
                    ScrollDown(1);
                    break;
                case SB_PAGEUP:
                    ScrollUp(m_config.height);
                    break;
                case SB_PAGEDOWN:
                    ScrollDown(m_config.height);
                    break;
                case SB_THUMBTRACK:
                    m_scrollOffset = HIWORD(wParam);
                    UpdateDisplay();
                    break;
            }
            break;
    }

    return CallWindowProcW(DefWindowProcW, hwnd, msg, wParam, lParam);
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<ITerminalEmulator> CreateTerminalEmulator() {
    return std::make_unique<Win32TerminalEmulator>();
}

// ============================================================================
// Utility Functions
// ============================================================================

bool IsProcessRunning(HANDLE hProcess) {
    DWORD exitCode;
    return GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
}

std::wstring GetCurrentDirectory() {
    DWORD size = GetCurrentDirectoryW(0, nullptr);
    std::vector<wchar_t> buffer(size);
    GetCurrentDirectoryW(size, buffer.data());
    return std::wstring(buffer.data());
}

std::vector<std::wstring> GetEnvironmentVariables() {
    std::vector<std::wstring> vars;
    LPWCH env = GetEnvironmentStringsW();
    if (env) {
        for (LPWCH var = env; *var; var += wcslen(var) + 1) {
            vars.push_back(var);
        }
        FreeEnvironmentStringsW(env);
    }
    return vars;
}

void SetEnvironmentVariable(const std::wstring& name, const std::wstring& value) {
    SetEnvironmentVariableW(name.c_str(), value.c_str());
}

std::wstring ExpandEnvironmentVariables(const std::wstring& str) {
    DWORD size = ExpandEnvironmentStringsW(str.c_str(), nullptr, 0);
    std::vector<wchar_t> buffer(size);
    ExpandEnvironmentStringsW(str.c_str(), buffer.data(), size);
    return std::wstring(buffer.data());
}

// ============================================================================
// Global State
// ============================================================================

bool g_terminalInitialized = false;
std::unique_ptr<ITerminalEmulator> g_terminalEmulator;