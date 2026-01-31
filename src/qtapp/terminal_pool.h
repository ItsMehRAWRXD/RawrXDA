#pragma once


#include <vector>
#include <cstdint>


struct TerminalInfo {
    QTextEdit* output_widget;
    QLineEdit* input_widget;
    QProcess* process;
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
    QTabWidget* tab_widget_;
    std::vector<TerminalInfo> terminals_;
};

