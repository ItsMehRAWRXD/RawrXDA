#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <windows.h>

// SCALAR-ONLY: Multi-terminal management with PowerShell support

namespace RawrXD {

enum class TerminalState {
    IDLE,           // Ready for new commands
    RUNNING,        // Executing command
    HANGING,        // Command running but not responding (e.g., server)
    CLOSED          // Terminal closed
};

struct TerminalInfo {
    std::string terminal_id;
    std::string title;
    TerminalState state;
    std::string current_command;
    std::string output_buffer;
    HANDLE process_handle;
    HANDLE stdin_write;
    HANDLE stdout_read;
    HANDLE stderr_read;
    DWORD process_id;
    int exit_code;
    bool is_background;
    std::string chat_id;  // Associated chat (if any)
};

class TerminalPool {
public:
    TerminalPool();
    ~TerminalPool();

    // Terminal lifecycle (scalar)
    std::string CreateTerminal(const std::string& title = "PowerShell");
    bool CloseTerminal(const std::string& terminal_id);
    void CloseAllTerminals();
    void CloseTerminalsForChat(const std::string& chat_id);

    // Command execution (scalar)
    bool ExecuteCommand(const std::string& terminal_id, const std::string& command, bool background = false);
    std::string ReadOutput(const std::string& terminal_id, size_t max_bytes = 4096);
    bool IsCommandRunning(const std::string& terminal_id);
    bool KillCurrentCommand(const std::string& terminal_id);

    // Terminal management
    std::shared_ptr<TerminalInfo> GetTerminal(const std::string& terminal_id);
    std::vector<std::string> GetAllTerminalIds();
    std::vector<std::string> GetIdleTerminals();
    std::vector<std::string> GetTerminalsForChat(const std::string& chat_id);

    // Find or create terminal
    std::string GetOrCreateIdleTerminal(const std::string& chat_id);
    std::string GetOrCreateTerminalForChat(const std::string& chat_id);

    // Terminal state
    TerminalState GetTerminalState(const std::string& terminal_id);
    void SetTerminalState(const std::string& terminal_id, TerminalState state);
    void AssociateWithChat(const std::string& terminal_id, const std::string& chat_id);

    // Output management
    void ClearOutput(const std::string& terminal_id);
    std::string GetFullOutput(const std::string& terminal_id);
    size_t GetOutputSize(const std::string& terminal_id);

    // Stats
    size_t GetTerminalCount() const { return terminals_.size(); }
    size_t GetActiveCount() const;

private:
    std::map<std::string, std::shared_ptr<TerminalInfo>> terminals_;
    int next_terminal_number_;

    std::string GenerateTerminalId();
    bool CreatePowerShellProcess(std::shared_ptr<TerminalInfo> term);
    bool ReadFromPipe(HANDLE pipe, std::string& output, size_t max_bytes);
    bool WriteToPipe(HANDLE pipe, const std::string& data);
    void CleanupTerminal(std::shared_ptr<TerminalInfo> term);
};

} // namespace RawrXD
