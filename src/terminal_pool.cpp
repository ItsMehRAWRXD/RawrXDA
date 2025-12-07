// Terminal Pool - Multiple terminal management
#include "terminal_pool.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QScrollBar>
#include <QDir>

TerminalPool::TerminalPool(uint32_t pool_size, QWidget* parent) 
    : QWidget(parent), pool_size_(pool_size) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QHBoxLayout* control_layout = new QHBoxLayout();
    QPushButton* new_terminal_btn = new QPushButton("+ Terminal", this);
    connect(new_terminal_btn, &QPushButton::clicked, this, &TerminalPool::createNewTerminal);
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
    QWidget* terminal_container = new QWidget(this);
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
    
    QString label = "Terminal " + QString::number(terminals_.size());
    int index = tab_widget_->addTab(terminal_container, label);
    tab_widget_->setTabsClosable(true); // Enable tab closing
    
    // Connect tab close signal
    connect(tab_widget_, QOverload<int>::of(&QTabWidget::tabCloseRequested),
            this, &TerminalPool::closeTerminal);
    
    // Connect input to command execution
    connect(terminal_input, &QLineEdit::returnPressed, 
            this, [this, index]() { executeCommand(index); });
    
    // Connect process output
    connect(process, &QProcess::readyReadStandardOutput, 
            this, [this, index]() { readProcessOutput(index); });
    connect(process, &QProcess::readyReadStandardError, 
            this, [this, index]() { readProcessError(index); });
}

void TerminalPool::executeCommand(int terminal_index) {
    if (terminal_index < 0 || terminal_index >= static_cast<int>(terminals_.size())) {
        return;
    }
    
    TerminalInfo& info = terminals_[terminal_index];
    QString command = info.input_widget->text();
    info.input_widget->clear();
    
    if (!command.isEmpty()) {
        // Send command to process (don't echo locally - cmd.exe will display it)
        info.process->write((command + "\n").toLocal8Bit());
        
        // Emit signal for other components
        emit commandExecuted(command);
    }
}

void TerminalPool::readProcessOutput(int terminal_index) {
    if (terminal_index < 0 || terminal_index >= static_cast<int>(terminals_.size())) {
        return;
    }
    
    TerminalInfo& info = terminals_[terminal_index];
    QByteArray output = info.process->readAllStandardOutput();
    QString output_str = QString::fromLocal8Bit(output);
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
    QByteArray error = info.process->readAllStandardError();
    QString error_str = QString::fromLocal8Bit(error);
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
