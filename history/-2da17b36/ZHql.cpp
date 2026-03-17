#include "interactive_shell.h"
#include <iostream>
#include <sstream>
#include <algorithm>

std::unique_ptr<InteractiveShell> g_shell;

InteractiveShell::InteractiveShell(const ShellConfig& config) 
    : running_(false), agent_(nullptr), memory_(nullptr), vsix_loader_(nullptr), 
      react_generator_(nullptr), config_(config), history_index_(0) {}

InteractiveShell::~InteractiveShell() {
    Stop();
}

void InteractiveShell::Start(AgenticEngine* agent, MemoryManager* memory, VSIXLoader* vsix,
                            RawrXD::ReactServerGenerator* react, 
                            std::function<void(const std::string&)> output_cb,
                            std::function<void(const std::string&)> error_cb) {
    agent_ = agent;
    memory_ = memory;
    vsix_loader_ = vsix;
    react_generator_ = react;
    output_callback_ = output_cb;
    running_ = true;
    
    if (output_callback_) {
        output_callback_("Interactive Shell Started\n");
    }
}

void InteractiveShell::Stop() {
    running_ = false;
}

void InteractiveShell::ProcessCommand(const std::string& input) {
    std::string trimmed = input;
    if (trimmed.empty()) return;
    
    if (trimmed == "/help") {
        if (output_callback_) {
            output_callback_("Commands: /help, /exit, /model <path>, /infer <text>\n");
        }
        return;
    }
    
    if (trimmed == "/exit") {
        running_ = false;
        return;
    }
    
    if (output_callback_) {
        output_callback_(">> " + trimmed + "\n");
    }
}

std::string InteractiveShell::Trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::string InteractiveShell::GetHelp() const {
    return "RawrXD Interactive Shell\n/help, /exit, /model, /infer\n";
}

void InteractiveShell::DisplayPrompt() {
    if (output_callback_) output_callback_(">> ");
}

void InteractiveShell::SaveHistory() {}
void InteractiveShell::LoadHistory() {}
