#include "cli_streaming_enhancements.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace CLI {

// ============================================================================
// StreamingManager Implementation
// ============================================================================

StreamingManager::StreamingManager() = default;
StreamingManager::~StreamingManager() {
    StopStream();
}

void StreamingManager::StartStream(const std::string& prompt, TokenCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_prompt_ = prompt;
    current_callback_ = callback;
    streaming_ = true;
}

void StreamingManager::StopStream() {
    streaming_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    current_callback_ = nullptr;
    current_prompt_.clear();
}

std::string StreamingManager::GetCurrentPrompt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_prompt_;
}

void StreamingManager::ProcessChunk(const std::string& chunk) {
    if (!streaming_.load()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_callback_) {
        current_callback_(chunk, false);
    }
}

// ============================================================================
// EnhancedCommandExecutor Implementation
// ============================================================================

EnhancedCommandExecutor::EnhancedCommandExecutor(AppState& state)
    : state_(state), streaming_manager_(std::make_unique<StreamingManager>()) {}

bool EnhancedCommandExecutor::ExecuteWithStreaming(const std::string& command, TokenCallback callback) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.push_back(command);
    
    // Check if command supports streaming
    if (command.find("stream") != std::string::npos || 
        command.find("infer") != std::string::npos ||
        command.find("chat") != std::string::npos) {
        streaming_manager_->StartStream(command, callback);
        return true;
    }
    
    return false;
}

void EnhancedCommandExecutor::ExecuteBatch(const std::vector<std::string>& commands) {
    for (const auto& cmd : commands) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.push_back(cmd);
        // Execute each command
        // Actual execution delegated to CommandHandler
    }
}

void EnhancedCommandExecutor::ClearHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
}

// ============================================================================
// AutoCompleter Implementation
// ============================================================================

AutoCompleter::AutoCompleter() {
    // Register default commands
    commands_ = {
        "help", "quit", "exit", "models", "load", "unload", "modelinfo",
        "infer", "stream", "chat", "temp", "topp", "maxtokens",
        "plan", "execute", "status", "selfcorrect", "analyze", "generate", "refactor",
        "autonomous", "goal", "hotreload", "analyzefile", "analyzeproject",
        "serverinfo", "listmodels", "benchmark", "config", "overclock", "telemetry",
        "mode", "shell", "ps", "history", "clear"
    };
}

std::vector<std::string> AutoCompleter::GetCompletions(const std::string& partial) const {
    if (partial.empty()) return {};
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> completions;
    
    for (const auto& cmd : commands_) {
        if (cmd.find(partial) == 0) {
            completions.push_back(cmd);
        }
    }
    
    return completions;
}

void AutoCompleter::AddCompletion(const std::string& completion) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(commands_.begin(), commands_.end(), completion) == commands_.end()) {
        commands_.push_back(completion);
    }
}

void AutoCompleter::RegisterCommand(const std::string& command, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    AddCompletion(command);
    command_params_[command] = params;
}

// ============================================================================
// HistoryManager Implementation
// ============================================================================

HistoryManager::HistoryManager(const std::string& history_file)
    : history_file_(history_file) {
    Load();
}

HistoryManager::~HistoryManager() {
    Save();
}

void HistoryManager::Add(const std::string& command) {
    if (command.empty()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Avoid duplicates of last command
    if (!history_.empty() && history_.back() == command) {
        return;
    }
    
    history_.push_back(command);
    
    // Limit history size
    if (history_.size() > max_history_size_) {
        history_.erase(history_.begin());
    }
}

std::vector<std::string> HistoryManager::GetAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

std::vector<std::string> HistoryManager::Search(const std::string& pattern) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    for (const auto& cmd : history_) {
        if (cmd.find(pattern) != std::string::npos) {
            results.push_back(cmd);
        }
    }
    
    return results;
}

void HistoryManager::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

void HistoryManager::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(history_file_);
    if (!file.is_open()) return;
    
    for (const auto& cmd : history_) {
        file << cmd << "\n";
    }
}

void HistoryManager::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(history_file_);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            history_.push_back(line);
        }
    }
}

// ============================================================================
// ProgressIndicator Implementation
// ============================================================================

ProgressIndicator::ProgressIndicator() = default;

void ProgressIndicator::Start(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_message_ = message;
    current_percent_ = 0;
    active_ = true;
    
    std::cout << "[" << message << "] ";
    std::cout.flush();
}

void ProgressIndicator::Update(int percent) {
    if (!active_.load()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    current_percent_ = std::clamp(percent, 0, 100);
    
    // Simple progress bar
    int bar_width = 50;
    int pos = (bar_width * current_percent_) / 100;
    
    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << current_percent_ << "% ";
    std::cout.flush();
}

void ProgressIndicator::UpdateMessage(const std::string& message) {
    if (!active_.load()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    current_message_ = message;
    std::cout << "\n[" << message << "] ";
    std::cout.flush();
}

void ProgressIndicator::Stop() {
    if (!active_.load()) return;
    
    active_ = false;
    std::cout << "\n";
    std::cout.flush();
}

} // namespace CLI
