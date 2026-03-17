#pragma once

// C++20 / Win32. Terminal pool; no Qt widgets. Use HWND/void* in impl.

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct TerminalInfo {
    void* output_handle = nullptr;  // HWND or custom control
    void* input_handle = nullptr;
    void* process_handle = nullptr; // HANDLE or process impl
};

class TerminalPool
{
public:
    using CommandExecutedFn = std::function<void(const std::string& command)>;

    explicit TerminalPool(uint32_t pool_size);
    void setOnCommandExecuted(CommandExecutedFn f) { m_onCommandExecuted = std::move(f); }
    void initialize();

    void createNewTerminal();
    void executeCommand(int terminal_index);
    void readProcessOutput(int terminal_index);
    void readProcessError(int terminal_index);
    void closeTerminal(int tab_index);

    void* getContainerHandle() const { return m_container; }  // For Win32: parent HWND

private:
    uint32_t pool_size_ = 0;
    void* m_container = nullptr;
    std::vector<TerminalInfo> terminals_;
    CommandExecutedFn m_onCommandExecuted;
};
