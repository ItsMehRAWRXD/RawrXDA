#pragma once


#include <vector>
#include <cstdint>


struct TerminalInfo {
    void* output_widget;
    void* input_widget;
    void** process;
};

class TerminalPool : public void {

public:
    explicit TerminalPool(uint32_t pool_size, void* parent = nullptr);
    void initialize();
    
public:
    void createNewTerminal();
    void executeCommand(int terminal_index);
    void readProcessOutput(int terminal_index);
    void readProcessError(int terminal_index);
    void closeTerminal(int tab_index);


    void commandExecuted(const std::string& command);
    
private:
    uint32_t pool_size_;
    void* tab_widget_;
    std::vector<TerminalInfo> terminals_;
};

