// pty_terminal.hpp - PTY Terminal Emulator with ConPTY Integration
// Author: RAW RXD IDE Team
// License: MIT

#ifndef PTY_TERMINAL_HPP
#define PTY_TERMINAL_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <deque>
#include <chrono>

#ifdef _WIN32
    #include <consoleapi.h>
    #include <consoleapi2.h>
    #include <wincon.h>
    #include <winconp.h>
    #pragma comment(lib, "kernel32.lib")
#else
    #include <pty.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <signal.h>
#endif

namespace rawrxd {

// ═══════════════════════════════════════════════════════════════════════
// TERMINAL TYPES
// ═══════════════════════════════════════════════════════════════════════

struct TerminalSize {
    int rows;
    int cols;
    int width_pixels;
    int height_pixels;
};

struct TerminalCell {
    char32_t character;
    uint16_t attributes;  // Foreground/background colors, styles
    bool is_wide;
};

struct TerminalCursor {
    int row;
    int col;
    bool visible;
    bool blink;
    uint16_t style;  // Block, underline, bar
};

struct TerminalScrollback {
    std::deque<std::vector<TerminalCell>> lines;
    int max_lines;
    int current_offset;
};

struct TerminalSelection {
    int start_row;
    int start_col;
    int end_row;
    int end_col;
    bool active;
};

struct TerminalConfig {
    int rows = 24;
    int cols = 80;
    int scrollback_lines = 10000;
    std::string shell = "";
    std::string cwd = "";
    std::map<std::string, std::string> env;
    bool enable_vt = true;
    bool enable_mouse = true;
    bool enable_bracketed_paste = true;
    uint16_t default_fg_color = 0x07;  // Light gray
    uint16_t default_bg_color = 0x00;  // Black
    uint16_t default_attrs = 0x07;
};

enum class TerminalEventType {
    Output,
    TitleChanged,
    Bell,
    Resize,
    Close,
    Focus,
    SelectionChanged
};

struct TerminalEvent {
    TerminalEventType type;
    std::string data;
    int param1;
    int param2;
};

// ═══════════════════════════════════════════════════════════════════════
// VT100/VT220 TERMINAL EMULATOR
// ═══════════════════════════════════════════════════════════════════════

class TerminalEmulator {
public:
    TerminalEmulator(const TerminalConfig& config = {});
    ~TerminalEmulator();

    // Process Control
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    
    // Input
    void write(const std::string& data);
    void writeKey(int key_code, bool shift = false, bool ctrl = false, bool alt = false);
    void writeText(const std::string& text);
    void writePaste(const std::string& text);
    
    // Output
    std::string read();
    std::string readNonBlocking();
    
    // Terminal Control
    void resize(int rows, int cols);
    void reset();
    void clear();
    void clearScrollback();
    
    // State
    TerminalSize getSize() const;
    TerminalCursor getCursor() const;
    TerminalScrollback getScrollback() const;
    TerminalSelection getSelection() const;
    
    // Selection
    void setSelection(int start_row, int start_col, int end_row, int end_col);
    void clearSelection();
    std::string getSelectedText();
    
    // Events
    void onEvent(std::function<void(const TerminalEvent&)> callback);
    
    // Configuration
    void setConfig(const TerminalConfig& config) { config_ = config; }
    TerminalConfig getConfig() const { return config_; }
    
    // Screen Buffer
    std::vector<std::vector<TerminalCell>> getScreenBuffer();
    void setScreenBuffer(const std::vector<std::vector<TerminalCell>>& buffer);

private:
    TerminalConfig config_;
    std::atomic<bool> running_{false};
    
    // Screen state
    std::vector<std::vector<TerminalCell>> screen_;
    TerminalCursor cursor_;
    TerminalScrollback scrollback_;
    TerminalSelection selection_;
    
    // Saved cursor state (for save/restore)
    TerminalCursor saved_cursor_;
    
    // Tab stops
    std::vector<int> tab_stops_;
    
    // Current attributes
    uint16_t current_attrs_;
    
    // VT parser state
    enum class ParserState {
        Ground,
        Escape,
        CSIEntry,
        CSIParam,
        CSIIntermediate,
        OSCString,
        DCSString,
        SOSString,
        PMString,
        APCString
    };
    
    ParserState parser_state_;
    std::string parser_buffer_;
    std::vector<int> parser_params_;
    
    // Process handles
#ifdef _WIN32
    HANDLE hPseudoConsole_ = nullptr;
    HANDLE hProcess_ = nullptr;
    HANDLE hThread_ = nullptr;
    HANDLE hPipeIn_ = nullptr;   // Write to process
    HANDLE hPipeOut_ = nullptr;  // Read from process
#else
    int master_fd_ = -1;
    pid_t pid_ = -1;
#endif
    
    std::thread read_thread_;
    std::mutex write_mutex_;
    std::mutex state_mutex_;
    
    std::vector<std::function<void(const TerminalEvent&)>> event_callbacks_;
    
    // VT Parser
    void processInput(const std::string& data);
    void processChar(char c);
    void processEscapeSequence(char c);
    void processCSISequence(char c);
    void processOSCSequence(const std::string& seq);
    
    // Terminal operations
    void executeControl(char c);
    void executeCSI(const std::vector<int>& params, char final_char);
    void executeOSC(const std::string& seq);
    
    // Cursor operations
    void cursorUp(int n = 1);
    void cursorDown(int n = 1);
    void cursorForward(int n = 1);
    void cursorBack(int n = 1);
    void cursorNextLine(int n = 1);
    void cursorPreviousLine(int n = 1);
    void cursorHorizontalAbsolute(int col);
    void cursorPosition(int row, int col);
    void eraseInDisplay(int mode);
    void eraseInLine(int mode);
    void insertLines(int n);
    void deleteLines(int n);
    void scrollUp(int n = 1);
    void scrollDown(int n = 1);
    
    // Character operations
    void putChar(char32_t c);
    void putCharAt(int row, int col, char32_t c);
    void newline();
    void carriageReturn();
    void tab();
    void backspace();
    void bell();
    
    // Screen operations
    void clearScreen();
    void clearLine();
    void scrollScreen(int delta);
    
    // Attribute operations
    void setAttribute(int attr);
    void resetAttributes();
    void setForegroundColor(int color);
    void setBackgroundColor(int color);
    
    // Helper functions
    void clampCursor();
    void ensureRowExists(int row);
    void emitEvent(const TerminalEvent& event);
    void readLoop();
    
    // Platform-specific
    bool createPseudoTerminal();
    void destroyPseudoTerminal();
    bool spawnProcess();
};

// ═══════════════════════════════════════════════════════════════════════
// INLINE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════

inline TerminalEmulator::TerminalEmulator(const TerminalConfig& config)
    : config_(config) {
    
    // Initialize screen buffer
    screen_.resize(config.rows);
    for (auto& row : screen_) {
        row.resize(config.cols);
        for (auto& cell : row) {
            cell.character = ' ';
            cell.attributes = config.default_attrs;
            cell.is_wide = false;
        }
    }
    
    // Initialize cursor
    cursor_.row = 0;
    cursor_.col = 0;
    cursor_.visible = true;
    cursor_.blink = true;
    cursor_.style = 1;  // Block
    
    // Initialize scrollback
    scrollback_.max_lines = config.scrollback_lines;
    scrollback_.current_offset = 0;
    
    // Initialize tab stops (every 8 columns)
    for (int i = 0; i < config.cols; i += 8) {
        tab_stops_.push_back(i);
    }
    
    // Initialize parser
    parser_state_ = ParserState::Ground;
    current_attrs_ = config.default_attrs;
}

inline TerminalEmulator::~TerminalEmulator() {
    stop();
}

inline bool TerminalEmulator::createPseudoTerminal() {
#ifdef _WIN32
    // Use ConPTY (Windows 10+)
    COORD size = {(SHORT)config_.cols, (SHORT)config_.rows};
    
    HRESULT hr = CreatePseudoConsole(
        size,
        hPipeIn_,
        hPipeOut_,
        0,
        &hPseudoConsole_
    );
    
    if (FAILED(hr)) {
        return false;
    }
    
    return true;
#else
    // Use Unix PTY
    struct winsize ws;
    ws.ws_row = config_.rows;
    ws.ws_col = config_.cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    
    pid_ = forkpty(&master_fd_, nullptr, nullptr, &ws);
    
    if (pid_ < 0) {
        return false;
    }
    
    if (pid_ == 0) {
        // Child process
        return spawnProcess();
    }
    
    // Parent process
    return true;
#endif
}

inline bool TerminalEmulator::spawnProcess() {
#ifdef _WIN32
    // Initialize startup info
    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    
    // Create attribute list
    SIZE_T attr_list_size;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_list_size);
    
    auto attr_list = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(attr_list_size);
    InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_list_size);
    
    // Set ConPTY attribute
    UpdateProcThreadAttribute(
        attr_list,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hPseudoConsole_,
        sizeof(HPCON),
        nullptr,
        nullptr
    );
    
    si.lpAttributeList = attr_list;
    
    // Get shell path
    std::wstring shell = config_.shell.empty() ? 
        L"powershell.exe" : 
        std::wstring(config_.shell.begin(), config_.shell.end());
    
    std::wstring cwd = config_.cwd.empty() ?
        L"" :
        std::wstring(config_.cwd.begin(), config_.cwd.end());
    
    // Create process
    PROCESS_INFORMATION pi = {};
    BOOL success = CreateProcessW(
        nullptr,
        const_cast<LPWSTR>(shell.c_str()),
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        cwd.empty() ? nullptr : cwd.c_str(),
        &si.StartupInfo,
        &pi
    );
    
    free(attr_list);
    
    if (!success) {
        return false;
    }
    
    hProcess_ = pi.hProcess;
    hThread_ = pi.hThread;
    
    return true;
#else
    // Set environment
    for (const auto& [key, value] : config_.env) {
        setenv(key.c_str(), value.c_str(), 1);
    }
    
    // Change directory
    if (!config_.cwd.empty()) {
        chdir(config_.cwd.c_str());
    }
    
    // Get shell
    const char* shell = config_.shell.empty() ?
        getenv("SHELL") ? getenv("SHELL") : "/bin/bash" :
        config_.shell.c_str();
    
    // Execute shell
    execl(shell, shell, nullptr);
    
    // Should never reach here
    return false;
#endif
}

inline bool TerminalEmulator::start() {
    if (running_) return true;
    
    // Create pipes
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    
    HANDLE hPipeInRead, hPipeInWrite;
    HANDLE hPipeOutRead, hPipeOutWrite;
    
    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, &sa, 0)) return false;
    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, &sa, 0)) return false;
    
    // Set inherit flags
    SetHandleInformation(hPipeInWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hPipeOutRead, HANDLE_FLAG_INHERIT, 0);
    
    hPipeIn_ = hPipeInRead;
    hPipeOut_ = hPipeOutWrite;
    
    // Create pseudo terminal
    if (!createPseudoTerminal()) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        CloseHandle(hPipeOutWrite);
        return false;
    }
    
    // Close handles that ConPTY now owns
    CloseHandle(hPipeIn_);
    CloseHandle(hPipeOut_);
    
    hPipeIn_ = hPipeInWrite;
    hPipeOut_ = hPipeOutRead;
#endif
    
    // Spawn process
    if (!spawnProcess()) {
        destroyPseudoTerminal();
        return false;
    }
    
    running_ = true;
    
    // Start read thread
    read_thread_ = std::thread([this] { readLoop(); });
    
    return true;
}

inline void TerminalEmulator::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Close pipes
#ifdef _WIN32
    if (hPipeIn_) {
        CloseHandle(hPipeIn_);
        hPipeIn_ = nullptr;
    }
    if (hPipeOut_) {
        CloseHandle(hPipeOut_);
        hPipeOut_ = nullptr;
    }
#else
    if (master_fd_ >= 0) {
        close(master_fd_);
        master_fd_ = -1;
    }
    
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, 0);
        pid_ = -1;
    }
#endif
    
    // Wait for read thread
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
    
    destroyPseudoTerminal();
}

inline void TerminalEmulator::destroyPseudoTerminal() {
#ifdef _WIN32
    if (hPseudoConsole_) {
        ClosePseudoConsole(hPseudoConsole_);
        hPseudoConsole_ = nullptr;
    }
    
    if (hProcess_) {
        TerminateProcess(hProcess_, 0);
        CloseHandle(hProcess_);
        hProcess_ = nullptr;
    }
    
    if (hThread_) {
        CloseHandle(hThread_);
        hThread_ = nullptr;
    }
#endif
}

inline void TerminalEmulator::readLoop() {
    char buffer[4096];
    
    while (running_) {
#ifdef _WIN32
        DWORD read = 0;
        if (!ReadFile(hPipeOut_, buffer, sizeof(buffer), &read, nullptr)) {
            break;
        }
#else
        ssize_t read = ::read(master_fd_, buffer, sizeof(buffer));
        if (read <= 0) {
            break;
        }
#endif
        
        // Process output
        std::string data(buffer, read);
        processInput(data);
        
        // Emit event
        TerminalEvent event;
        event.type = TerminalEventType::Output;
        event.data = data;
        emitEvent(event);
    }
    
    // Emit close event
    TerminalEvent event;
    event.type = TerminalEventType::Close;
    emitEvent(event);
}

inline void TerminalEmulator::write(const std::string& data) {
    if (!running_) return;
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    
#ifdef _WIN32
    DWORD written = 0;
    WriteFile(hPipeIn_, data.c_str(), (DWORD)data.size(), &written, nullptr);
#else
    ::write(master_fd_, data.c_str(), data.size());
#endif
}

inline void TerminalEmulator::writeKey(int key_code, bool shift, bool ctrl, bool alt) {
    // Convert key code to escape sequence
    std::string seq;
    
    if (ctrl && key_code >= 'A' && key_code <= 'Z') {
        // Ctrl+A to Ctrl+Z
        seq = (char)(key_code - 'A' + 1);
    } else if (key_code == 0x0D) {  // Enter
        seq = "\r";
    } else if (key_code == 0x08) {  // Backspace
        seq = "\b";
    } else if (key_code == 0x09) {  // Tab
        seq = "\t";
    } else if (key_code == 0x1B) {  // Escape
        seq = "\x1B";
    } else if (key_code >= 0x70 && key_code <= 0x7B) {  // F1-F12
        seq = "\x1B[";
        if (shift) seq += "1;2";
        seq += std::to_string(key_code - 0x70 + 11);
        seq += "~";
    } else if (key_code == 0x25) {  // Left arrow
        seq = shift ? "\x1B[1;2D" : "\x1B[D";
    } else if (key_code == 0x26) {  // Up arrow
        seq = shift ? "\x1B[1;2A" : "\x1B[A";
    } else if (key_code == 0x27) {  // Right arrow
        seq = shift ? "\x1B[1;2C" : "\x1B[C";
    } else if (key_code == 0x28) {  // Down arrow
        seq = shift ? "\x1B[1;2B" : "\x1B[B";
    } else if (key_code == 0x21) {  // Page Up
        seq = shift ? "\x1B[5;2~" : "\x1B[5~";
    } else if (key_code == 0x22) {  // Page Down
        seq = shift ? "\x1B[6;2~" : "\x1B[6~";
    } else if (key_code == 0x23) {  // End
        seq = shift ? "\x1B[1;2F" : "\x1B[F";
    } else if (key_code == 0x24) {  // Home
        seq = shift ? "\x1B[1;2H" : "\x1B[H";
    } else if (key_code == 0x2D) {  // Insert
        seq = shift ? "\x1B[2;2~" : "\x1B[2~";
    } else if (key_code == 0x2E) {  // Delete
        seq = shift ? "\x1B[3;2~" : "\x1B[3~";
    }
    
    if (!seq.empty()) {
        write(seq);
    }
}

inline void TerminalEmulator::writeText(const std::string& text) {
    write(text);
}

inline void TerminalEmulator::writePaste(const std::string& text) {
    if (config_.enable_bracketed_paste) {
        write("\x1B[200~" + text + "\x1B[201~");
    } else {
        write(text);
    }
}

inline void TerminalEmulator::resize(int rows, int cols) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    config_.rows = rows;
    config_.cols = cols;
    
    // Resize screen buffer
    screen_.resize(rows);
    for (auto& row : screen_) {
        row.resize(cols);
    }
    
    // Resize pseudo terminal
#ifdef _WIN32
    if (hPseudoConsole_) {
        COORD size = {(SHORT)cols, (SHORT)rows};
        ResizePseudoConsole(hPseudoConsole_, size);
    }
#else
    if (master_fd_ >= 0) {
        struct winsize ws;
        ws.ws_row = rows;
        ws.ws_col = cols;
        ioctl(master_fd_, TIOCSWINSZ, &ws);
    }
#endif
    
    // Emit resize event
    TerminalEvent event;
    event.type = TerminalEventType::Resize;
    event.param1 = rows;
    event.param2 = cols;
    emitEvent(event);
}

inline void TerminalEmulator::processInput(const std::string& data) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    for (char c : data) {
        processChar(c);
    }
}

inline void TerminalEmulator::processChar(char c) {
    switch (parser_state_) {
        case ParserState::Ground:
            if (c == 0x1B) {
                parser_state_ = ParserState::Escape;
                parser_buffer_.clear();
                parser_params_.clear();
            } else {
                executeControl(c);
            }
            break;
            
        case ParserState::Escape:
            processEscapeSequence(c);
            break;
            
        case ParserState::CSIEntry:
            if (c >= '0' && c <= '9') {
                parser_state_ = ParserState::CSIParam;
                parser_buffer_ = c;
            } else if (c == '?') {
                parser_buffer_ = c;
            } else if (c >= 0x20 && c <= 0x2F) {
                parser_state_ = ParserState::CSIIntermediate;
                parser_buffer_ = c;
            } else {
                executeCSI(parser_params_, c);
                parser_state_ = ParserState::Ground;
            }
            break;
            
        case ParserState::CSIParam:
            if (c >= '0' && c <= '9') {
                parser_buffer_ += c;
            } else if (c == ';') {
                parser_params_.push_back(std::stoi(parser_buffer_));
                parser_buffer_.clear();
            } else if (c >= 0x20 && c <= 0x2F) {
                parser_state_ = ParserState::CSIIntermediate;
                parser_buffer_ = c;
            } else {
                if (!parser_buffer_.empty()) {
                    parser_params_.push_back(std::stoi(parser_buffer_));
                }
                executeCSI(parser_params_, c);
                parser_state_ = ParserState::Ground;
            }
            break;
            
        case ParserState::CSIIntermediate:
            if (c >= 0x20 && c <= 0x2F) {
                parser_buffer_ += c;
            } else {
                executeCSI(parser_params_, c);
                parser_state_ = ParserState::Ground;
            }
            break;
            
        case ParserState::OSCString:
            if (c == 0x07 || c == 0x5C) {
                processOSCSequence(parser_buffer_);
                parser_state_ = ParserState::Ground;
            } else {
                parser_buffer_ += c;
            }
            break;
            
        default:
            parser_state_ = ParserState::Ground;
            break;
    }
}

inline void TerminalEmulator::processEscapeSequence(char c) {
    if (c == '[') {
        parser_state_ = ParserState::CSIEntry;
    } else if (c == ']') {
        parser_state_ = ParserState::OSCString;
        parser_buffer_.clear();
    } else if (c == '7') {
        // Save cursor
        saved_cursor_ = cursor_;
        parser_state_ = ParserState::Ground;
    } else if (c == '8') {
        // Restore cursor
        cursor_ = saved_cursor_;
        parser_state_ = ParserState::Ground;
    } else if (c == 'M') {
        // Reverse index
        scrollDown(1);
        parser_state_ = ParserState::Ground;
    } else if (c == 'D') {
        // Index
        scrollUp(1);
        parser_state_ = ParserState::Ground;
    } else if (c == 'E') {
        // Next line
        newline();
        parser_state_ = ParserState::Ground;
    } else if (c == 'c') {
        // Reset
        reset();
        parser_state_ = ParserState::Ground;
    } else {
        parser_state_ = ParserState::Ground;
    }
}

inline void TerminalEmulator::executeControl(char c) {
    switch (c) {
        case 0x00:  // NUL
            break;
        case 0x07:  // BEL
            bell();
            break;
        case 0x08:  // BS
            backspace();
            break;
        case 0x09:  // HT
            tab();
            break;
        case 0x0A:  // LF
            newline();
            break;
        case 0x0B:  // VT
            newline();
            break;
        case 0x0C:  // FF
            newline();
            break;
        case 0x0D:  // CR
            carriageReturn();
            break;
        case 0x0E:  // SO
            break;
        case 0x0F:  // SI
            break;
        case 0x18:  // CAN
        case 0x1A:  // SUB
            parser_state_ = ParserState::Ground;
            break;
        default:
            if (c >= 0x20) {
                putChar(c);
            }
            break;
    }
}

inline void TerminalEmulator::executeCSI(const std::vector<int>& params, char final_char) {
    switch (final_char) {
        case 'A':  // CUU - Cursor Up
            cursorUp(params.empty() ? 1 : params[0]);
            break;
        case 'B':  // CUD - Cursor Down
            cursorDown(params.empty() ? 1 : params[0]);
            break;
        case 'C':  // CUF - Cursor Forward
            cursorForward(params.empty() ? 1 : params[0]);
            break;
        case 'D':  // CUB - Cursor Back
            cursorBack(params.empty() ? 1 : params[0]);
            break;
        case 'E':  // CNL - Cursor Next Line
            cursorNextLine(params.empty() ? 1 : params[0]);
            break;
        case 'F':  // CPL - Cursor Previous Line
            cursorPreviousLine(params.empty() ? 1 : params[0]);
            break;
        case 'G':  // CHA - Cursor Horizontal Absolute
            cursorHorizontalAbsolute(params.empty() ? 1 : params[0]);
            break;
        case 'H':  // CUP - Cursor Position
        case 'f':
            cursorPosition(
                params.empty() ? 1 : params[0],
                params.size() < 2 ? 1 : params[1]
            );
            break;
        case 'J':  // ED - Erase in Display
            eraseInDisplay(params.empty() ? 0 : params[0]);
            break;
        case 'K':  // EL - Erase in Line
            eraseInLine(params.empty() ? 0 : params[0]);
            break;
        case 'L':  // IL - Insert Lines
            insertLines(params.empty() ? 1 : params[0]);
            break;
        case 'M':  // DL - Delete Lines
            deleteLines(params.empty() ? 1 : params[0]);
            break;
        case 'P':  // DCH - Delete Characters
            // TODO: Implement
            break;
        case 'S':  // SU - Scroll Up
            scrollUp(params.empty() ? 1 : params[0]);
            break;
        case 'T':  // SD - Scroll Down
            scrollDown(params.empty() ? 1 : params[0]);
            break;
        case 'm':  // SGR - Select Graphic Rendition
            if (params.empty()) {
                resetAttributes();
            } else {
                for (size_t i = 0; i < params.size(); i++) {
                    if (params[i] == 0) {
                        resetAttributes();
                    } else if (params[i] >= 30 && params[i] <= 37) {
                        setForegroundColor(params[i] - 30);
                    } else if (params[i] >= 40 && params[i] <= 47) {
                        setBackgroundColor(params[i] - 40);
                    } else if (params[i] >= 90 && params[i] <= 97) {
                        setForegroundColor(params[i] - 90 + 8);
                    } else if (params[i] >= 100 && params[i] <= 107) {
                        setBackgroundColor(params[i] - 100 + 8);
                    } else {
                        setAttribute(params[i]);
                    }
                }
            }
            break;
        case 'r':  // DECSTBM - Set Top and Bottom Margins
            // TODO: Implement
            break;
        case 's':  // Save cursor position
            saved_cursor_ = cursor_;
            break;
        case 'u':  // Restore cursor position
            cursor_ = saved_cursor_;
            break;
        default:
            break;
    }
}

inline void TerminalEmulator::processOSCSequence(const std::string& seq) {
    // Parse OSC sequence
    size_t semicolon = seq.find(';');
    if (semicolon == std::string::npos) return;
    
    int command = std::stoi(seq.substr(0, semicolon));
    std::string param = seq.substr(semicolon + 1);
    
    switch (command) {
        case 0:  // Set window title and icon name
        case 2:  // Set window title
            {
                TerminalEvent event;
                event.type = TerminalEventType::TitleChanged;
                event.data = param;
                emitEvent(event);
            }
            break;
        default:
            break;
    }
}

inline void TerminalEmulator::cursorUp(int n) {
    cursor_.row = std::max(0, cursor_.row - n);
}

inline void TerminalEmulator::cursorDown(int n) {
    cursor_.row = std::min(config_.rows - 1, cursor_.row + n);
}

inline void TerminalEmulator::cursorForward(int n) {
    cursor_.col = std::min(config_.cols - 1, cursor_.col + n);
}

inline void TerminalEmulator::cursorBack(int n) {
    cursor_.col = std::max(0, cursor_.col - n);
}

inline void TerminalEmulator::cursorNextLine(int n) {
    cursor_.row = std::min(config_.rows - 1, cursor_.row + n);
    cursor_.col = 0;
}

inline void TerminalEmulator::cursorPreviousLine(int n) {
    cursor_.row = std::max(0, cursor_.row - n);
    cursor_.col = 0;
}

inline void TerminalEmulator::cursorHorizontalAbsolute(int col) {
    cursor_.col = std::max(0, std::min(config_.cols - 1, col - 1));
}

inline void TerminalEmulator::cursorPosition(int row, int col) {
    cursor_.row = std::max(0, std::min(config_.rows - 1, row - 1));
    cursor_.col = std::max(0, std::min(config_.cols - 1, col - 1));
}

inline void TerminalEmulator::eraseInDisplay(int mode) {
    switch (mode) {
        case 0:  // Erase from cursor to end of screen
            for (int col = cursor_.col; col < config_.cols; col++) {
                screen_[cursor_.row][col].character = ' ';
                screen_[cursor_.row][col].attributes = current_attrs_;
            }
            for (int row = cursor_.row + 1; row < config_.rows; row++) {
                for (int col = 0; col < config_.cols; col++) {
                    screen_[row][col].character = ' ';
                    screen_[row][col].attributes = current_attrs_;
                }
            }
            break;
        case 1:  // Erase from start of screen to cursor
            for (int row = 0; row < cursor_.row; row++) {
                for (int col = 0; col < config_.cols; col++) {
                    screen_[row][col].character = ' ';
                    screen_[row][col].attributes = current_attrs_;
                }
            }
            for (int col = 0; col <= cursor_.col; col++) {
                screen_[cursor_.row][col].character = ' ';
                screen_[cursor_.row][col].attributes = current_attrs_;
            }
            break;
        case 2:  // Erase entire screen
            clearScreen();
            break;
    }
}

inline void TerminalEmulator::eraseInLine(int mode) {
    switch (mode) {
        case 0:  // Erase from cursor to end of line
            for (int col = cursor_.col; col < config_.cols; col++) {
                screen_[cursor_.row][col].character = ' ';
                screen_[cursor_.row][col].attributes = current_attrs_;
            }
            break;
        case 1:  // Erase from start of line to cursor
            for (int col = 0; col <= cursor_.col; col++) {
                screen_[cursor_.row][col].character = ' ';
                screen_[cursor_.row][col].attributes = current_attrs_;
            }
            break;
        case 2:  // Erase entire line
            clearLine();
            break;
    }
}

inline void TerminalEmulator::insertLines(int n) {
    // Scroll lines down from cursor position
    for (int i = 0; i < n; i++) {
        screen_.insert(screen_.begin() + cursor_.row, std::vector<TerminalCell>(config_.cols));
        screen_.pop_back();
    }
}

inline void TerminalEmulator::deleteLines(int n) {
    // Scroll lines up from cursor position
    for (int i = 0; i < n; i++) {
        screen_.erase(screen_.begin() + cursor_.row);
        screen_.push_back(std::vector<TerminalCell>(config_.cols));
    }
}

inline void TerminalEmulator::scrollUp(int n) {
    for (int i = 0; i < n; i++) {
        // Move top line to scrollback
        scrollback_.lines.push_back(screen_[0]);
        if (scrollback_.lines.size() > scrollback_.max_lines) {
            scrollback_.lines.pop_front();
        }
        
        // Scroll screen up
        screen_.erase(screen_.begin());
        screen_.push_back(std::vector<TerminalCell>(config_.cols));
        
        // Initialize new line
        for (auto& cell : screen_.back()) {
            cell.character = ' ';
            cell.attributes = current_attrs_;
        }
    }
}

inline void TerminalEmulator::scrollDown(int n) {
    for (int i = 0; i < n; i++) {
        // Scroll screen down
        screen_.insert(screen_.begin(), std::vector<TerminalCell>(config_.cols));
        screen_.pop_back();
        
        // Initialize new line
        for (auto& cell : screen_.front()) {
            cell.character = ' ';
            cell.attributes = current_attrs_;
        }
    }
}

inline void TerminalEmulator::putChar(char32_t c) {
    if (cursor_.col >= config_.cols) {
        newline();
        cursor_.col = 0;
    }
    
    screen_[cursor_.row][cursor_.col].character = c;
    screen_[cursor_.row][cursor_.col].attributes = current_attrs_;
    cursor_.col++;
}

inline void TerminalEmulator::newline() {
    cursor_.row++;
    
    if (cursor_.row >= config_.rows) {
        scrollUp(1);
        cursor_.row = config_.rows - 1;
    }
}

inline void TerminalEmulator::carriageReturn() {
    cursor_.col = 0;
}

inline void TerminalEmulator::tab() {
    // Find next tab stop
    for (int stop : tab_stops_) {
        if (stop > cursor_.col) {
            cursor_.col = stop;
            return;
        }
    }
    cursor_.col = config_.cols - 1;
}

inline void TerminalEmulator::backspace() {
    if (cursor_.col > 0) {
        cursor_.col--;
    }
}

inline void TerminalEmulator::bell() {
    TerminalEvent event;
    event.type = TerminalEventType::Bell;
    emitEvent(event);
}

inline void TerminalEmulator::clearScreen() {
    for (auto& row : screen_) {
        for (auto& cell : row) {
            cell.character = ' ';
            cell.attributes = current_attrs_;
        }
    }
}

inline void TerminalEmulator::clearLine() {
    for (auto& cell : screen_[cursor_.row]) {
        cell.character = ' ';
        cell.attributes = current_attrs_;
    }
}

inline void TerminalEmulator::reset() {
    clearScreen();
    cursor_.row = 0;
    cursor_.col = 0;
    resetAttributes();
}

inline void TerminalEmulator::resetAttributes() {
    current_attrs_ = config_.default_attrs;
}

inline void TerminalEmulator::setAttribute(int attr) {
    // Handle various attributes (bold, underline, etc.)
    // Simplified implementation
    if (attr == 1) {  // Bold
        current_attrs_ |= 0x08;
    } else if (attr == 4) {  // Underline
        current_attrs_ |= 0x80;
    } else if (attr == 7) {  // Reverse
        current_attrs_ = ((current_attrs_ & 0x0F) << 4) | ((current_attrs_ & 0xF0) >> 4);
    }
}

inline void TerminalEmulator::setForegroundColor(int color) {
    current_attrs_ = (current_attrs_ & 0xF0) | (color & 0x0F);
}

inline void TerminalEmulator::setBackgroundColor(int color) {
    current_attrs_ = (current_attrs_ & 0x0F) | ((color & 0x0F) << 4);
}

inline void TerminalEmulator::onEvent(std::function<void(const TerminalEvent&)> callback) {
    event_callbacks_.push_back(callback);
}

inline void TerminalEmulator::emitEvent(const TerminalEvent& event) {
    for (const auto& callback : event_callbacks_) {
        callback(event);
    }
}

inline TerminalSize TerminalEmulator::getSize() const {
    return {config_.rows, config_.cols, 0, 0};
}

inline TerminalCursor TerminalEmulator::getCursor() const {
    return cursor_;
}

inline TerminalScrollback TerminalEmulator::getScrollback() const {
    return scrollback_;
}

inline TerminalSelection TerminalEmulator::getSelection() const {
    return selection_;
}

inline void TerminalEmulator::setSelection(int start_row, int start_col, int end_row, int end_col) {
    selection_.start_row = start_row;
    selection_.start_col = start_col;
    selection_.end_row = end_row;
    selection_.end_col = end_col;
    selection_.active = true;
    
    TerminalEvent event;
    event.type = TerminalEventType::SelectionChanged;
    emitEvent(event);
}

inline void TerminalEmulator::clearSelection() {
    selection_.active = false;
    
    TerminalEvent event;
    event.type = TerminalEventType::SelectionChanged;
    emitEvent(event);
}

inline std::string TerminalEmulator::getSelectedText() {
    if (!selection_.active) return "";
    
    std::string result;
    
    for (int row = selection_.start_row; row <= selection_.end_row; row++) {
        int start_col = (row == selection_.start_row) ? selection_.start_col : 0;
        int end_col = (row == selection_.end_row) ? selection_.end_col : config_.cols - 1;
        
        for (int col = start_col; col <= end_col; col++) {
            result += (char)screen_[row][col].character;
        }
        
        if (row < selection_.end_row) {
            result += '\n';
        }
    }
    
    return result;
}

inline std::vector<std::vector<TerminalCell>> TerminalEmulator::getScreenBuffer() {
    return screen_;
}

inline void TerminalEmulator::setScreenBuffer(const std::vector<std::vector<TerminalCell>>& buffer) {
    screen_ = buffer;
}

} // namespace rawrxd

#endif // PTY_TERMINAL_HPP
