#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <condition_variable>

namespace RawrXD::Agentic {

struct TerminalChar {
    wchar_t ch;
    COLORREF fgColor = RGB(192, 192, 192);
    COLORREF bgColor = RGB(0, 0, 0);
    bool bold = false;
    bool underline = false;
    bool reverse = false;

    TerminalChar(wchar_t c = L' ') : ch(c) {}
};

struct TerminalLine {
    std::vector<TerminalChar> chars;
    bool wrapped = false;

    TerminalLine() = default;
    TerminalLine(size_t width) { chars.resize(width); }

    void resize(size_t width) {
        chars.resize(width);
    }

    size_t length() const { return chars.size(); }
};

struct TerminalCursor {
    int x = 0;
    int y = 0;
    bool visible = true;
    std::chrono::steady_clock::time_point lastBlink;

    void move(int newX, int newY, int maxX, int maxY) {
        x = std::max(0, std::min(newX, maxX - 1));
        y = std::max(0, std::min(newY, maxY - 1));
    }

    void moveRelative(int dx, int dy, int maxX, int maxY) {
        move(x + dx, y + dy, maxX, maxY);
    }
};

struct TerminalAttributes {
    COLORREF fgColor = RGB(192, 192, 192);
    COLORREF bgColor = RGB(0, 0, 0);
    bool bold = false;
    bool underline = false;
    bool reverse = false;
    bool conceal = false;
};

class TerminalBuffer {
private:
    std::vector<TerminalLine> lines_;
    std::deque<std::string> scrollback_;
    size_t width_ = 80;
    size_t height_ = 24;
    size_t maxScrollback_ = 1000;
    std::mutex mutex_;

public:
    TerminalBuffer(size_t width = 80, size_t height = 24, size_t maxScrollback = 1000)
        : width_(width), height_(height), maxScrollback_(maxScrollback) {
        resize(width, height);
    }

    void resize(size_t newWidth, size_t newHeight) {
        std::lock_guard<std::mutex> lock(mutex_);

        width_ = newWidth;
        height_ = newHeight;

        // Resize existing lines
        for (auto& line : lines_) {
            line.resize(width_);
        }

        // Add or remove lines as needed
        if (lines_.size() < height_) {
            lines_.resize(height_, TerminalLine(width_));
        } else if (lines_.size() > height_) {
            // Move excess lines to scrollback
            for (size_t i = height_; i < lines_.size(); ++i) {
                std::string lineText;
                for (const auto& ch : lines_[i].chars) {
                    lineText += ch.ch;
                }
                scrollback_.push_back(lineText);
            }
            lines_.resize(height_);
        }

        // Maintain scrollback limit
        while (scrollback_.size() > maxScrollback_) {
            scrollback_.pop_front();
        }
    }

    void putChar(int x, int y, TerminalChar ch) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (y >= 0 && y < (int)lines_.size() && x >= 0 && x < (int)width_) {
            lines_[y].chars[x] = ch;
        }
    }

    TerminalChar getChar(int x, int y) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (y >= 0 && y < (int)lines_.size() && x >= 0 && x < (int)width_) {
            return lines_[y].chars[x];
        }
        return TerminalChar(L' ');
    }

    void scrollUp(int lines = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < lines && !lines_.empty(); ++i) {
            std::string lineText;
            for (const auto& ch : lines_.front().chars) {
                lineText += ch.ch;
            }
            scrollback_.push_back(lineText);
            lines_.erase(lines_.begin());
            lines_.push_back(TerminalLine(width_));
        }

        while (scrollback_.size() > maxScrollback_) {
            scrollback_.pop_front();
        }
    }

    void scrollDown(int lines = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < lines && !scrollback_.empty(); ++i) {
            std::string lineText = scrollback_.back();
            scrollback_.pop_back();

            lines_.insert(lines_.begin(), TerminalLine(width_));
            for (size_t j = 0; j < lineText.length() && j < width_; ++j) {
                lines_.front().chars[j].ch = lineText[j];
            }
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& line : lines_) {
            for (auto& ch : line.chars) {
                ch = TerminalChar(L' ');
            }
        }
    }

    void clearLine(int y) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (y >= 0 && y < (int)lines_.size()) {
            for (auto& ch : lines_[y].chars) {
                ch = TerminalChar(L' ');
            }
        }
    }

    void clearFromCursor(int x, int y) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (y >= 0 && y < (int)lines_.size()) {
            for (int i = x; i < (int)width_; ++i) {
                lines_[y].chars[i] = TerminalChar(L' ');
            }
        }
    }

    size_t getWidth() const { return width_; }
    size_t getHeight() const { return height_; }
    size_t getScrollbackSize() const { return scrollback_.size(); }

    std::vector<TerminalLine> getVisibleLines() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }

    std::deque<std::string> getScrollback() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return scrollback_;
    }
};

class ANSIEscapeParser {
private:
    enum class State {
        Normal,
        Escape,
        CSI,
        OSC
    };

    State state_ = State::Normal;
    std::string sequence_;
    TerminalAttributes currentAttrs_;

public:
    struct ParsedCommand {
        enum class Type {
            PrintChar,
            MoveCursor,
            SetAttributes,
            ClearScreen,
            ClearLine,
            Scroll,
            SetTitle,
            Unknown
        };

        Type type;
        int param1 = 0;
        int param2 = 0;
        int param3 = 0;
        TerminalAttributes attrs;
        std::string text;
        wchar_t ch = L'\0';
    };

    std::vector<ParsedCommand> parse(const std::string& input) {
        std::vector<ParsedCommand> commands;

        for (char c : input) {
            switch (state_) {
                case State::Normal:
                    if (c == '\x1b') {
                        state_ = State::Escape;
                        sequence_.clear();
                    } else if (c == '\n') {
                        commands.push_back({ParsedCommand::Type::PrintChar, 0, 0, 0, {}, {}, L'\n'});
                    } else if (c == '\r') {
                        commands.push_back({ParsedCommand::Type::PrintChar, 0, 0, 0, {}, {}, L'\r'});
                    } else if (c == '\t') {
                        commands.push_back({ParsedCommand::Type::PrintChar, 0, 0, 0, {}, {}, L'\t'});
                    } else {
                        commands.push_back({ParsedCommand::Type::PrintChar, 0, 0, 0, {}, {}, (wchar_t)c});
                    }
                    break;

                case State::Escape:
                    if (c == '[') {
                        state_ = State::CSI;
                        sequence_ = "[";
                    } else if (c == ']') {
                        state_ = State::OSC;
                        sequence_ = "]";
                    } else {
                        state_ = State::Normal;
                    }
                    break;

                case State::CSI:
                    sequence_ += c;
                    if (c >= '@' && c <= '~') {
                        // End of CSI sequence
                        commands.push_back(parseCSISequence(sequence_));
                        state_ = State::Normal;
                    }
                    break;

                case State::OSC:
                    sequence_ += c;
                    if (c == '\x07' || (c == '\\' && sequence_.back() == '\x1b')) {
                        // End of OSC sequence
                        commands.push_back(parseOSCSequence(sequence_));
                        state_ = State::Normal;
                    }
                    break;
            }
        }

        return commands;
    }

private:
    ParsedCommand parseCSISequence(const std::string& seq) {
        ParsedCommand cmd;
        cmd.type = ParsedCommand::Type::Unknown;

        // Parse parameters
        std::vector<int> params;
        std::string paramStr;
        for (size_t i = 1; i < seq.length() - 1; ++i) {
            if (seq[i] == ';') {
                if (!paramStr.empty()) {
                    params.push_back(std::stoi(paramStr));
                    paramStr.clear();
                }
            } else if (isdigit(seq[i])) {
                paramStr += seq[i];
            }
        }
        if (!paramStr.empty()) {
            params.push_back(std::stoi(paramStr));
        }

        char command = seq.back();

        switch (command) {
            case 'H': // CUP - Cursor Position
            case 'f': // HVP - Horizontal Vertical Position
                cmd.type = ParsedCommand::Type::MoveCursor;
                cmd.param1 = params.empty() ? 1 : params[0]; // row (1-based)
                cmd.param2 = params.size() < 2 ? 1 : params[1]; // column (1-based)
                break;

            case 'A': // CUU - Cursor Up
                cmd.type = ParsedCommand::Type::MoveCursor;
                cmd.param1 = - (params.empty() ? 1 : params[0]);
                cmd.param2 = 0;
                break;

            case 'B': // CUD - Cursor Down
                cmd.type = ParsedCommand::Type::MoveCursor;
                cmd.param1 = params.empty() ? 1 : params[0];
                cmd.param2 = 0;
                break;

            case 'C': // CUF - Cursor Forward
                cmd.type = ParsedCommand::Type::MoveCursor;
                cmd.param1 = 0;
                cmd.param2 = params.empty() ? 1 : params[0];
                break;

            case 'D': // CUB - Cursor Back
                cmd.type = ParsedCommand::Type::MoveCursor;
                cmd.param1 = 0;
                cmd.param2 = - (params.empty() ? 1 : params[0]);
                break;

            case 'J': // ED - Erase in Display
                cmd.type = ParsedCommand::Type::ClearScreen;
                cmd.param1 = params.empty() ? 0 : params[0];
                break;

            case 'K': // EL - Erase in Line
                cmd.type = ParsedCommand::Type::ClearLine;
                cmd.param1 = params.empty() ? 0 : params[0];
                break;

            case 'm': // SGR - Select Graphic Rendition
                cmd.type = ParsedCommand::Type::SetAttributes;
                cmd.attrs = parseSGRAttributes(params);
                break;

            case 'S': // SU - Scroll Up
                cmd.type = ParsedCommand::Type::Scroll;
                cmd.param1 = - (params.empty() ? 1 : params[0]);
                break;

            case 'T': // SD - Scroll Down
                cmd.type = ParsedCommand::Type::Scroll;
                cmd.param1 = params.empty() ? 1 : params[0];
                break;
        }

        return cmd;
    }

    ParsedCommand parseOSCSequence(const std::string& seq) {
        ParsedCommand cmd;
        cmd.type = ParsedCommand::Type::Unknown;

        // OSC sequences like ]0;title\x07
        if (seq.length() > 2 && seq[1] == '0' && seq[2] == ';') {
            size_t end = seq.find('\x07');
            if (end != std::string::npos) {
                cmd.type = ParsedCommand::Type::SetTitle;
                cmd.text = seq.substr(3, end - 3);
            }
        }

        return cmd;
    }

    TerminalAttributes parseSGRAttributes(const std::vector<int>& params) {
        TerminalAttributes attrs = currentAttrs_;

        for (size_t i = 0; i < params.size(); ++i) {
            int param = params[i];

            switch (param) {
                case 0: // Reset all
                    attrs = TerminalAttributes();
                    break;
                case 1: // Bold
                    attrs.bold = true;
                    break;
                case 4: // Underline
                    attrs.underline = true;
                    break;
                case 7: // Reverse
                    attrs.reverse = true;
                    break;
                case 8: // Conceal
                    attrs.conceal = true;
                    break;
                case 22: // Normal intensity (not bold)
                    attrs.bold = false;
                    break;
                case 24: // Not underlined
                    attrs.underline = false;
                    break;
                case 27: // Not reverse
                    attrs.reverse = false;
                    break;
                case 28: // Not concealed
                    attrs.conceal = false;
                    break;
                case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: // Foreground colors
                    attrs.fgColor = getANSIColor(param - 30, false);
                    break;
                case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: // Background colors
                    attrs.bgColor = getANSIColor(param - 40, false);
                    break;
                case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97: // Bright foreground colors
                    attrs.fgColor = getANSIColor(param - 90, true);
                    break;
                case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: // Bright background colors
                    attrs.bgColor = getANSIColor(param - 100, true);
                    break;
            }
        }

        currentAttrs_ = attrs;
        return attrs;
    }

    COLORREF getANSIColor(int index, bool bright) {
        static const COLORREF ansiColors[16] = {
            RGB(0, 0, 0),       // Black
            RGB(128, 0, 0),     // Red
            RGB(0, 128, 0),     // Green
            RGB(128, 128, 0),   // Yellow
            RGB(0, 0, 128),     // Blue
            RGB(128, 0, 128),   // Magenta
            RGB(0, 128, 128),   // Cyan
            RGB(192, 192, 192), // White
            RGB(128, 128, 128), // Bright Black
            RGB(255, 0, 0),     // Bright Red
            RGB(0, 255, 0),     // Bright Green
            RGB(255, 255, 0),   // Bright Yellow
            RGB(0, 0, 255),     // Bright Blue
            RGB(255, 0, 255),   // Bright Magenta
            RGB(0, 255, 255),   // Bright Cyan
            RGB(255, 255, 255)  // Bright White
        };

        if (bright) index += 8;
        return ansiColors[index % 16];
    }
};

class TerminalEmulator {
private:
    HWND hwnd_;
    TerminalBuffer buffer_;
    TerminalCursor cursor_;
    TerminalAttributes currentAttrs_;
    ANSIEscapeParser parser_;

    std::unique_ptr<std::thread> renderThread_;
    std::atomic<bool> running_;
    std::mutex renderMutex_;
    std::condition_variable renderCV_;

    HFONT font_;
    int charWidth_;
    int charHeight_;

    // Shell process
    HANDLE hProcess_ = nullptr;
    HANDLE hInputRead_ = nullptr;
    HANDLE hInputWrite_ = nullptr;
    HANDLE hOutputRead_ = nullptr;
    HANDLE hOutputWrite_ = nullptr;
    std::unique_ptr<std::thread> shellReaderThread_;

    std::function<void(const std::string&)> onOutput_;
    std::function<void(const std::string&)> onTitleChange_;

public:
    TerminalEmulator(HWND parent, const RECT& rect)
        : buffer_(80, 24), running_(false) {

        // Create terminal window
        hwnd_ = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"RawrXDTerminal",
            L"",
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            parent,
            nullptr,
            GetModuleHandle(nullptr),
            this
        );

        if (hwnd_) {
            // Create font
            font_ = CreateFont(
                12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
            );

            // Calculate character dimensions
            HDC hdc = GetDC(hwnd_);
            SelectObject(hdc, font_);
            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            charWidth_ = tm.tmAveCharWidth;
            charHeight_ = tm.tmHeight;
            ReleaseDC(hwnd_, hdc);

            // Start shell
            startShell();
        }
    }

    ~TerminalEmulator() {
        running_ = false;
        if (renderThread_ && renderThread_->joinable()) {
            renderThread_->join();
        }
        if (shellReaderThread_ && shellReaderThread_->joinable()) {
            shellReaderThread_->join();
        }
        if (hProcess_) {
            TerminateProcess(hProcess_, 0);
            CloseHandle(hProcess_);
        }
        if (font_) DeleteObject(font_);
        if (hwnd_) DestroyWindow(hwnd_);
    }

    void resize(const RECT& rect) {
        if (hwnd_) {
            MoveWindow(hwnd_, rect.left, rect.top,
                      rect.right - rect.left, rect.bottom - rect.top, TRUE);

            int newWidth = (rect.right - rect.left) / charWidth_;
            int newHeight = (rect.bottom - rect.top) / charHeight_;
            buffer_.resize(newWidth, newHeight);
        }
    }

    void writeInput(const std::string& text) {
        if (hInputWrite_) {
            DWORD written;
            WriteFile(hInputWrite_, text.c_str(), text.length(), &written, nullptr);
        }
    }

    void setOnOutput(std::function<void(const std::string&)> callback) {
        onOutput_ = callback;
    }

    void setOnTitleChange(std::function<void(const std::string&)> callback) {
        onTitleChange_ = callback;
    }

    HWND getHWND() const { return hwnd_; }

    void render(HDC hdc) {
        std::lock_guard<std::mutex> lock(renderMutex_);

        RECT clientRect;
        GetClientRect(hwnd_, &clientRect);

        // Clear background
        HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        // Select font
        SelectObject(hdc, font_);
        SetBkMode(hdc, TRANSPARENT);

        // Render visible lines
        auto lines = buffer_.getVisibleLines();
        for (size_t y = 0; y < lines.size(); ++y) {
            for (size_t x = 0; x < lines[y].chars.size(); ++x) {
                const auto& ch = lines[y].chars[x];
                if (ch.ch != L' ') {
                    SetTextColor(hdc, ch.fgColor);
                    TextOut(hdc, x * charWidth_, y * charHeight_,
                           &ch.ch, 1);
                }
            }
        }

        // Render cursor
        if (cursor_.visible) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - cursor_.lastBlink).count();

            if (elapsed % 1000 < 500) { // Blink every 500ms
                RECT cursorRect = {
                    (LONG)(cursor_.x * charWidth_),
                    (LONG)(cursor_.y * charHeight_),
                    (LONG)((cursor_.x + 1) * charWidth_),
                    (LONG)((cursor_.y + 1) * charHeight_)
                };

                InvertRect(hdc, &cursorRect);
            }
        }
    }

    void processOutput(const std::string& output) {
        auto commands = parser_.parse(output);

        for (const auto& cmd : commands) {
            executeCommand(cmd);
        }

        InvalidateRect(hwnd_, nullptr, FALSE);
    }

private:
    void startShell() {
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

        // Create pipes
        CreatePipe(&hOutputRead_, &hOutputWrite_, &sa, 0);
        CreatePipe(&hInputRead_, &hInputWrite_, &sa, 0);

        // Set handles to inherit
        SetHandleInformation(hOutputWrite_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hInputRead_, HANDLE_FLAG_INHERIT, 0);

        // Start cmd.exe
        STARTUPINFO si = { sizeof(STARTUPINFO) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hInputRead_;
        si.hStdOutput = hOutputWrite_;
        si.hStdError = hOutputWrite_;

        PROCESS_INFORMATION pi;
        if (CreateProcess(
            nullptr,
            (LPSTR)"cmd.exe",
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi
        )) {
            hProcess_ = pi.hProcess;
            CloseHandle(pi.hThread);

            // Start reader thread
            shellReaderThread_ = std::make_unique<std::thread>([this]() {
                readShellOutput();
            });
        }
    }

    void readShellOutput() {
        char buffer[4096];
        DWORD bytesRead;

        while (running_ && ReadFile(hOutputRead_, buffer, sizeof(buffer), &bytesRead, nullptr)) {
            if (bytesRead > 0) {
                std::string output(buffer, bytesRead);
                processOutput(output);

                if (onOutput_) {
                    onOutput_(output);
                }
            }
        }
    }

    void executeCommand(const ANSIEscapeParser::ParsedCommand& cmd) {
        switch (cmd.type) {
            case ANSIEscapeParser::ParsedCommand::Type::PrintChar:
                printChar(cmd.ch);
                break;

            case ANSIEscapeParser::ParsedCommand::Type::MoveCursor:
                if (cmd.param1 != 0 || cmd.param2 != 0) {
                    cursor_.moveRelative(cmd.param2, cmd.param1,
                                       buffer_.getWidth(), buffer_.getHeight());
                } else {
                    cursor_.move(cmd.param2 - 1, cmd.param1 - 1,
                               buffer_.getWidth(), buffer_.getHeight());
                }
                break;

            case ANSIEscapeParser::ParsedCommand::Type::SetAttributes:
                currentAttrs_ = cmd.attrs;
                break;

            case ANSIEscapeParser::ParsedCommand::Type::ClearScreen:
                buffer_.clear();
                cursor_.move(0, 0, buffer_.getWidth(), buffer_.getHeight());
                break;

            case ANSIEscapeParser::ParsedCommand::Type::ClearLine:
                buffer_.clearLine(cursor_.y);
                break;

            case ANSIEscapeParser::ParsedCommand::Type::Scroll:
                if (cmd.param1 > 0) {
                    buffer_.scrollUp(cmd.param1);
                } else if (cmd.param1 < 0) {
                    buffer_.scrollDown(-cmd.param1);
                }
                break;

            case ANSIEscapeParser::ParsedCommand::Type::SetTitle:
                if (onTitleChange_) {
                    onTitleChange_(cmd.text);
                }
                break;

            default:
                break;
        }
    }

    void printChar(wchar_t ch) {
        if (ch == L'\n') {
            cursor_.move(0, cursor_.y + 1, buffer_.getWidth(), buffer_.getHeight());
        } else if (ch == L'\r') {
            cursor_.move(0, cursor_.y, buffer_.getWidth(), buffer_.getHeight());
        } else if (ch == L'\t') {
            int tabWidth = 4;
            int nextTab = ((cursor_.x / tabWidth) + 1) * tabWidth;
            cursor_.move(nextTab, cursor_.y, buffer_.getWidth(), buffer_.getHeight());
        } else {
            TerminalChar tchar(ch);
            tchar.fgColor = currentAttrs_.fgColor;
            tchar.bgColor = currentAttrs_.bgColor;
            tchar.bold = currentAttrs_.bold;
            tchar.underline = currentAttrs_.underline;
            tchar.reverse = currentAttrs_.reverse;

            buffer_.putChar(cursor_.x, cursor_.y, tchar);
            cursor_.moveRelative(1, 0, buffer_.getWidth(), buffer_.getHeight());
        }
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        TerminalEmulator* terminal = nullptr;

        if (msg == WM_CREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            terminal = (TerminalEmulator*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)terminal);
        } else {
            terminal = (TerminalEmulator*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (terminal) {
            return terminal->handleMessage(msg, wParam, lParam);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd_, &ps);
                render(hdc);
                EndPaint(hwnd_, &ps);
                return 0;
            }

            case WM_CHAR: {
                if (wParam >= 32 || wParam == VK_RETURN || wParam == VK_BACK) {
                    char ch = (char)wParam;
                    writeInput(std::string(1, ch));
                }
                return 0;
            }

            case WM_KEYDOWN: {
                switch (wParam) {
                    case VK_RETURN:
                        writeInput("\r\n");
                        break;
                    case VK_BACK:
                        writeInput("\b");
                        break;
                }
                return 0;
            }

            case WM_SIZE: {
                int width = LOWORD(lParam) / charWidth_;
                int height = HIWORD(lParam) / charHeight_;
                buffer_.resize(width, height);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }

        return DefWindowProc(hwnd_, msg, wParam, lParam);
    }
};

} // namespace RawrXD::Agentic