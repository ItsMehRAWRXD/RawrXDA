#include "terminal_pool.h"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace RawrXD {

TerminalPool::TerminalPool() : next_terminal_number_(1) {}

TerminalPool::~TerminalPool() {
    CloseAllTerminals();
}

std::string TerminalPool::CreateTerminal(const std::string& title) {
    auto term = std::make_shared<TerminalInfo>();
    term->terminal_id = GenerateTerminalId();
    term->title = title.empty() ? ("Terminal " + std::to_string(next_terminal_number_++)) : title;
    term->state = TerminalState::IDLE;
    term->is_background = false;
    term->exit_code = 0;
    term->process_handle = NULL;
    term->stdin_write = NULL;
    term->stdout_read = NULL;
    term->stderr_read = NULL;
    term->process_id = 0;

    if (!CreatePowerShellProcess(term)) {
        return "";
    }

    terminals_[term->terminal_id] = term;
    return term->terminal_id;
}

bool TerminalPool::CreatePowerShellProcess(std::shared_ptr<TerminalInfo> term) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes for stdin
    HANDLE stdin_read, stdin_write;
    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        return false;
    }
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    // Create pipes for stdout
    HANDLE stdout_read, stdout_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return false;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    // Create pipes for stderr
    HANDLE stderr_read, stderr_write;
    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return false;
    }
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // Create PowerShell process
    STARTUPINFOW si = {0};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    
    wchar_t cmd[] = L"powershell.exe -NoLogo -NoExit -Command -";
    
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);
        return false;
    }

    // Close handles we don't need
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    // Store handles
    term->process_handle = pi.hProcess;
    term->stdin_write = stdin_write;
    term->stdout_read = stdout_read;
    term->stderr_read = stderr_read;
    term->process_id = pi.dwProcessId;

    CloseHandle(pi.hThread);

    return true;
}

bool TerminalPool::CloseTerminal(const std::string& terminal_id) {
    auto it = terminals_.find(terminal_id);
    if (it != terminals_.end()) {
        CleanupTerminal(it->second);
        terminals_.erase(it);
        return true;
    }
    return false;
}

void TerminalPool::CloseAllTerminals() {
    for (auto& [id, term] : terminals_) {
        CleanupTerminal(term);
    }
    terminals_.clear();
}

void TerminalPool::CloseTerminalsForChat(const std::string& chat_id) {
    std::vector<std::string> to_close;
    for (const auto& [id, term] : terminals_) {
        if (term->chat_id == chat_id) {
            to_close.push_back(id);
        }
    }
    
    for (const auto& id : to_close) {
        CloseTerminal(id);
    }
}

void TerminalPool::CleanupTerminal(std::shared_ptr<TerminalInfo> term) {
    if (term->process_handle) {
        TerminateProcess(term->process_handle, 0);
        CloseHandle(term->process_handle);
        term->process_handle = NULL;
    }
    if (term->stdin_write) {
        CloseHandle(term->stdin_write);
        term->stdin_write = NULL;
    }
    if (term->stdout_read) {
        CloseHandle(term->stdout_read);
        term->stdout_read = NULL;
    }
    if (term->stderr_read) {
        CloseHandle(term->stderr_read);
        term->stderr_read = NULL;
    }
    term->state = TerminalState::CLOSED;
}

bool TerminalPool::ExecuteCommand(const std::string& terminal_id, const std::string& command, bool background) {
    auto term = GetTerminal(terminal_id);
    if (!term || term->state == TerminalState::CLOSED) {
        return false;
    }

    std::string cmd = command + "\n";
    if (!WriteToPipe(term->stdin_write, cmd)) {
        return false;
    }

    term->current_command = command;
    term->state = background ? TerminalState::HANGING : TerminalState::RUNNING;
    term->is_background = background;

    return true;
}

std::string TerminalPool::ReadOutput(const std::string& terminal_id, size_t max_bytes) {
    auto term = GetTerminal(terminal_id);
    if (!term) return "";

    std::string output;
    ReadFromPipe(term->stdout_read, output, max_bytes);
    
    std::string err_output;
    ReadFromPipe(term->stderr_read, err_output, max_bytes);
    
    if (!err_output.empty()) {
        output += err_output;
    }

    term->output_buffer += output;

    // Check if process finished
    if (!term->is_background && term->state == TerminalState::RUNNING) {
        DWORD exit_code;
        if (GetExitCodeProcess(term->process_handle, &exit_code)) {
            if (exit_code != STILL_ACTIVE) {
                term->state = TerminalState::IDLE;
                term->exit_code = exit_code;
                term->current_command.clear();
            }
        }
    }

    return output;
}

bool TerminalPool::IsCommandRunning(const std::string& terminal_id) {
    auto term = GetTerminal(terminal_id);
    if (!term) return false;

    return term->state == TerminalState::RUNNING || term->state == TerminalState::HANGING;
}

bool TerminalPool::KillCurrentCommand(const std::string& terminal_id) {
    auto term = GetTerminal(terminal_id);
    if (!term) return false;

    // Send Ctrl+C to PowerShell
    if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, term->process_id)) {
        // If that fails, terminate the process
        TerminateProcess(term->process_handle, 1);
    }

    term->state = TerminalState::IDLE;
    term->current_command.clear();
    return true;
}

std::shared_ptr<TerminalInfo> TerminalPool::GetTerminal(const std::string& terminal_id) {
    auto it = terminals_.find(terminal_id);
    if (it != terminals_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> TerminalPool::GetAllTerminalIds() {
    std::vector<std::string> ids;
    for (const auto& [id, term] : terminals_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<std::string> TerminalPool::GetIdleTerminals() {
    std::vector<std::string> idle;
    for (const auto& [id, term] : terminals_) {
        if (term->state == TerminalState::IDLE) {
            idle.push_back(id);
        }
    }
    return idle;
}

std::vector<std::string> TerminalPool::GetTerminalsForChat(const std::string& chat_id) {
    std::vector<std::string> chat_terms;
    for (const auto& [id, term] : terminals_) {
        if (term->chat_id == chat_id) {
            chat_terms.push_back(id);
        }
    }
    return chat_terms;
}

std::string TerminalPool::GetOrCreateIdleTerminal(const std::string& chat_id) {
    // Find idle terminal for this chat
    auto chat_terms = GetTerminalsForChat(chat_id);
    for (const auto& id : chat_terms) {
        auto term = GetTerminal(id);
        if (term && term->state == TerminalState::IDLE) {
            return id;
        }
    }

    // Create new terminal
    std::string term_id = CreateTerminal("Terminal for " + chat_id);
    if (!term_id.empty()) {
        AssociateWithChat(term_id, chat_id);
    }
    return term_id;
}

std::string TerminalPool::GetOrCreateTerminalForChat(const std::string& chat_id) {
    auto chat_terms = GetTerminalsForChat(chat_id);
    if (!chat_terms.empty()) {
        return chat_terms[0];
    }

    std::string term_id = CreateTerminal("Terminal for " + chat_id);
    if (!term_id.empty()) {
        AssociateWithChat(term_id, chat_id);
    }
    return term_id;
}

TerminalState TerminalPool::GetTerminalState(const std::string& terminal_id) {
    auto term = GetTerminal(terminal_id);
    return term ? term->state : TerminalState::CLOSED;
}

void TerminalPool::SetTerminalState(const std::string& terminal_id, TerminalState state) {
    auto term = GetTerminal(terminal_id);
    if (term) {
        term->state = state;
    }
}

void TerminalPool::AssociateWithChat(const std::string& terminal_id, const std::string& chat_id) {
    auto term = GetTerminal(terminal_id);
    if (term) {
        term->chat_id = chat_id;
    }
}

void TerminalPool::ClearOutput(const std::string& terminal_id) {
    auto term = GetTerminal(terminal_id);
    if (term) {
        term->output_buffer.clear();
    }
}

std::string TerminalPool::GetFullOutput(const std::string& terminal_id) {
    auto term = GetTerminal(terminal_id);
    return term ? term->output_buffer : "";
}

size_t TerminalPool::GetOutputSize(const std::string& terminal_id) {
    auto term = GetTerminal(terminal_id);
    return term ? term->output_buffer.size() : 0;
}

size_t TerminalPool::GetActiveCount() const {
    size_t count = 0;
    for (const auto& [id, term] : terminals_) {
        if (term->state != TerminalState::IDLE && term->state != TerminalState::CLOSED) {
            ++count;
        }
    }
    return count;
}

std::string TerminalPool::GenerateTerminalId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return "term_" + std::to_string(ms.count());
}

bool TerminalPool::ReadFromPipe(HANDLE pipe, std::string& output, size_t max_bytes) {
    DWORD bytes_avail = 0;
    if (!PeekNamedPipe(pipe, NULL, 0, NULL, &bytes_avail, NULL)) {
        return false;
    }

    if (bytes_avail == 0) return true;

    DWORD to_read = (std::min)(bytes_avail, (DWORD)max_bytes);
    std::vector<char> buffer(to_read);
    
    DWORD bytes_read = 0;
    if (ReadFile(pipe, buffer.data(), to_read, &bytes_read, NULL)) {
        output.append(buffer.data(), bytes_read);
        return true;
    }

    return false;
}

bool TerminalPool::WriteToPipe(HANDLE pipe, const std::string& data) {
    DWORD bytes_written = 0;
    return WriteFile(pipe, data.c_str(), data.length(), &bytes_written, NULL) != 0;
}

} // namespace RawrXD
