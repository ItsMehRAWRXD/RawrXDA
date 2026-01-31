// Terminal Pool - Multiple terminal management
#include "terminal_pool.h"


TerminalPool::TerminalPool(uint32_t pool_size, void* parent) 
    : void(parent), pool_size_(pool_size), tab_widget_(nullptr) {
    // Lightweight constructor - defer Qt widget and process creation
}

void TerminalPool::initialize() {
    if (tab_widget_) return;  // Already initialized
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QHBoxLayout* control_layout = new QHBoxLayout();
    QPushButton* new_terminal_btn = new QPushButton("+ Terminal", this);
// Qt connect removed
    control_layout->addWidget(new_terminal_btn);
    control_layout->addStretch();
    layout->addLayout(control_layout);
    
    tab_widget_ = new QTabWidget(this);
    layout->addWidget(tab_widget_);
    
    for (uint32_t i = 0; i < pool_size_; ++i) {
        createNewTerminal();
    }
}

void TerminalPool::createNewTerminal() {
    void* terminal_container = new void(this);
    QVBoxLayout* terminal_layout = new QVBoxLayout(terminal_container);
    
    QTextEdit* terminal_output = new QTextEdit(this);
    terminal_output->setReadOnly(true);
    terminal_output->setStyleSheet(
        "background-color: #000000; color: #00ff00; font-family: 'Consolas';");
    
    QLineEdit* terminal_input = new QLineEdit(this);
    terminal_input->setStyleSheet(
        "background-color: #000000; color: #00ff00; font-family: 'Consolas';");
    terminal_input->setPlaceholderText("Enter command...");
    
    terminal_layout->addWidget(terminal_output);
    terminal_layout->addWidget(terminal_input);
    
    // Create process for this terminal (PowerShell)
    QProcess* process = new QProcess(this);
    process->setProgram("pwsh.exe"); // PowerShell Core
    process->setArguments({"-NoExit", "-Command", "$host.ui.RawUI.WindowTitle='RawrXD Terminal'"}); 
    process->start();
    
    // Store terminal components
    TerminalInfo info;
    info.output_widget = terminal_output;
    info.input_widget = terminal_input;
    info.process = process;
    terminals_.push_back(info);
    
    std::string label = "Terminal " + std::string::number(terminals_.size());
    int index = tab_widget_->addTab(terminal_container, label);
    tab_widget_->setTabsClosable(true); // Enable tab closing
    
    // Connect tab close signal
// Qt connect removed
    // Connect input to command execution
// Qt connect removed
            this, [this, index]() { executeCommand(index); });
    
    // Connect process output
// Qt connect removed
            this, [this, index]() { readProcessOutput(index); });
// Qt connect removed
            this, [this, index]() { readProcessError(index); });
}

void TerminalPool::executeCommand(int terminal_index) {
    if (terminal_index < 0 || terminal_index >= static_cast<int>(terminals_.size())) {
        return;
    }
    
    TerminalInfo& info = terminals_[terminal_index];
    std::string command = info.input_widget->text();
    info.input_widget->clear();
    
    if (!command.isEmpty()) {
        // Send command to process (don't echo locally - cmd.exe will display it)
        info.process->write((command + "\n").toLocal8Bit());
        
        // signal for other components
        commandExecuted(command);
    }
}

void TerminalPool::readProcessOutput(int terminal_index) {
    if (terminal_index < 0 || terminal_index >= static_cast<int>(terminals_.size())) {
        return;
    }
    
    TerminalInfo& info = terminals_[terminal_index];
    std::vector<uint8_t> output = info.process->readAllStandardOutput();
    std::string output_str = std::string::fromLocal8Bit(output);
    info.output_widget->insertPlainText(output_str);
    
    // Scroll to bottom
    QScrollBar* scroll = info.output_widget->verticalScrollBar();
    scroll->setValue(scroll->maximum());
}

void TerminalPool::readProcessError(int terminal_index) {
    if (terminal_index < 0 || terminal_index >= static_cast<int>(terminals_.size())) {
        return;
    }
    
    TerminalInfo& info = terminals_[terminal_index];
    std::vector<uint8_t> error = info.process->readAllStandardError();
    std::string error_str = std::string::fromLocal8Bit(error);
    info.output_widget->insertPlainText(error_str);
    
    // Scroll to bottom
    QScrollBar* scroll = info.output_widget->verticalScrollBar();
    scroll->setValue(scroll->maximum());
}

void TerminalPool::closeTerminal(int tab_index) {
    if (tab_index < 0 || tab_index >= static_cast<int>(terminals_.size())) {
        return;
    }
    
    TerminalInfo& info = terminals_[tab_index];
    
    // Terminate the process
    if (info.process) {
        info.process->terminate();
        if (!info.process->waitForFinished(3000)) {
            info.process->kill();
        }
    }
    
    // Remove the tab
    tab_widget_->removeTab(tab_index);
    
    // Clean up
    if (info.output_widget) {
        info.output_widget->deleteLater();
    }
    if (info.input_widget) {
        info.input_widget->deleteLater();
    }
    if (info.process) {
        info.process->deleteLater();
    }
    
    // Remove from terminals list and reindex
    terminals_.erase(terminals_.begin() + tab_index);
}

