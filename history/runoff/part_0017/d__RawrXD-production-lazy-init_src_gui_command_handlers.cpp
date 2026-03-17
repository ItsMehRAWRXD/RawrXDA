// ============================================================================
// GUI Command Handlers - Production Implementation
// ============================================================================
// Processes incoming CLI commands and executes corresponding GUI actions
// ============================================================================

#include "command_handlers.hpp"
#include "../cli/cli_local_model.hpp"
#include "../core/local_gguf_loader.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// Forward declarations for GUI components
class RawrXDMainWindow {
public:
    void showStatusMessage(const std::string& message);
    void showErrorMessage(const std::string& message);
    void focusPane(const std::string& pane_name);
    void showChat();
    void hideChat();
    void executeCommand(const std::string& command);
    
    class ModelLoaderWidget* getModelLoaderWidget();
    class AgentChatPane* getChatPane();
    class TerminalManager* getTerminalManager();
};

class ModelLoaderWidget {
public:
    bool loadModel(const std::string& model_path);
    bool unloadModel();
    std::string getCurrentModelPath() const;
    bool isModelLoaded() const;
};

class AgentChatPane {
public:
    void show();
    void hide();
    bool isVisible() const;
    void focus();
};

class TerminalManager {
public:
    void executeCommand(const std::string& command);
    std::string getLastOutput() const;
};

namespace RawrXD {
namespace GUI {

// ============================================================================
// GUI COMMAND HANDLERS IMPLEMENTATION
// ============================================================================

GUICommandHandlers::GUICommandHandlers(RawrXDMainWindow* main_window)
    : main_window_(main_window)
    , model_loader_widget_(nullptr)
    , chat_pane_(nullptr)
    , terminal_manager_(nullptr)
    , model_loaded_(false)
    , current_model_path_("")
    , last_error_("")
    , verbose_(false)
    , auto_focus_(true) {
    
    if (main_window_) {
        model_loader_widget_ = main_window_->getModelLoaderWidget();
        chat_pane_ = main_window_->getChatPane();
        terminal_manager_ = main_window_->getTerminalManager();
    }
}

GUICommandHandlers::~GUICommandHandlers() {
    // Cleanup
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

bool GUICommandHandlers::handleMessage(const IPCMessage& message) {
    if (message.type != IPCMessageType::COMMAND) {
        reportError("Invalid message type: expected COMMAND");
        return false;
    }
    
    // Parse command from message payload
    std::string command_line(reinterpret_cast<const char*>(message.payload.data()));
    
    if (verbose_) {
        std::cout << "[GUI] Received command: " << command_line << std::endl;
    }
    
    return handleCommand(command_line, {});
}

bool GUICommandHandlers::handleCommand(const std::string& command, 
                                     const std::vector<std::string>& args) {
    // Get command info from registry
    const CommandInfo* cmd_info = CommandRegistry::instance().getCommand(command);
    
    if (!cmd_info) {
        reportError("Unknown command: " + command);
        return false;
    }
    
    // Validate argument count
    if (cmd_info->requires_args && args.size() < static_cast<size_t>(cmd_info->min_args)) {
        reportError("Insufficient arguments for command: " + command);
        reportError("Usage: " + cmd_info->usage);
        return false;
    }
    
    if (cmd_info->max_args >= 0 && args.size() > static_cast<size_t>(cmd_info->max_args)) {
        reportError("Too many arguments for command: " + command);
        reportError("Usage: " + cmd_info->usage);
        return false;
    }
    
    // Execute command
    bool success = cmd_info->handler(this, args);
    
    if (success) {
        reportSuccess("Command executed successfully: " + command);
    } else {
        reportError("Command failed: " + command);
    }
    
    return success;
}

// ============================================================================
// INDIVIDUAL COMMAND HANDLERS
// ============================================================================

bool GUICommandHandlers::handleLoadModel(const std::string& model_path) {
    if (verbose_) {
        std::cout << "[GUI] Loading model: " << model_path << std::endl;
    }
    
    // Validate model path
    if (!validateModelPath(model_path)) {
        reportError("Invalid model path: " + model_path);
        return false;
    }
    
    // Load model in GUI
    if (!loadModelInGUI(model_path)) {
        reportError("Failed to load model in GUI: " + model_path);
        return false;
    }
    
    // Update state
    model_loaded_ = true;
    current_model_path_ = model_path;
    
    // Trigger callback
    if (model_loaded_callback_) {
        model_loaded_callback_(model_path);
    }
    
    // Report success
    reportSuccess("Model loaded successfully: " + model_path);
    
    return true;
}

bool GUICommandHandlers::handleUnloadModel() {
    if (verbose_) {
        std::cout << "[GUI] Unloading current model" << std::endl;
    }
    
    // Check if model is loaded
    if (!model_loaded_) {
        reportError("No model is currently loaded");
        return false;
    }
    
    // Unload model from GUI
    if (!unloadModelFromGUI()) {
        reportError("Failed to unload model from GUI");
        return false;
    }
    
    // Update state
    model_loaded_ = false;
    current_model_path_ = "";
    
    // Trigger callback
    if (model_unloaded_callback_) {
        model_unloaded_callback_();
    }
    
    // Report success
    reportSuccess("Model unloaded successfully");
    
    return true;
}

bool GUICommandHandlers::handleFocusPane(const std::string& pane_name) {
    if (verbose_) {
        std::cout << "[GUI] Focusing pane: " << pane_name << std::endl;
    }
    
    // Validate pane name
    if (!validatePaneName(pane_name)) {
        reportError("Invalid pane name: " + pane_name);
        return false;
    }
    
    // Focus pane in GUI
    if (!focusPaneInGUI(pane_name)) {
        reportError("Failed to focus pane: " + pane_name);
        return false;
    }
    
    // Trigger callback
    if (pane_focused_callback_) {
        pane_focused_callback_(pane_name);
    }
    
    // Report success
    reportSuccess("Pane focused successfully: " + pane_name);
    
    return true;
}

bool GUICommandHandlers::handleShowChat() {
    if (verbose_) {
        std::cout << "[GUI] Showing chat pane" << std::endl;
    }
    
    // Show chat in GUI
    if (!showChatInGUI()) {
        reportError("Failed to show chat pane");
        return false;
    }
    
    // Report success
    reportSuccess("Chat pane shown successfully");
    
    return true;
}

bool GUICommandHandlers::handleHideChat() {
    if (verbose_) {
        std::cout << "[GUI] Hiding chat pane" << std::endl;
    }
    
    // Hide chat in GUI
    if (!hideChatInGUI()) {
        reportError("Failed to hide chat pane");
        return false;
    }
    
    // Report success
    reportSuccess("Chat pane hidden successfully");
    
    return true;
}

bool GUICommandHandlers::handleGetStatus() {
    if (verbose_) {
        std::cout << "[GUI] Getting status" << std::endl;
    }
    
    // Build status report
    std::stringstream status;
    status << "RawrXD GUI Status:\n";
    status << "  Model loaded: " << (model_loaded_ ? "Yes" : "No") << "\n";
    if (model_loaded_) {
        status << "  Current model: " << current_model_path_ << "\n";
    }
    status << "  Chat visible: " << (chat_pane_ && chat_pane_->isVisible() ? "Yes" : "No") << "\n";
    status << "  Last error: " << (last_error_.empty() ? "None" : last_error_) << "\n";
    
    // Report status
    reportStatus(status.str());
    
    return true;
}

bool GUICommandHandlers::handleExecuteCommand(const std::string& command) {
    if (verbose_) {
        std::cout << "[GUI] Executing command: " << command << std::endl;
    }
    
    // Execute command in GUI
    if (!executeCommandInGUI(command)) {
        reportError("Failed to execute command: " + command);
        return false;
    }
    
    // Report success
    reportSuccess("Command executed successfully: " + command);
    
    return true;
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

bool GUICommandHandlers::handleLoadModels(const std::vector<std::string>& model_paths) {
    if (verbose_) {
        std::cout << "[GUI] Loading " << model_paths.size() << " models" << std::endl;
    }
    
    bool all_success = true;
    std::vector<std::string> failed_models;
    
    for (const auto& model_path : model_paths) {
        if (!handleLoadModel(model_path)) {
            all_success = false;
            failed_models.push_back(model_path);
        }
    }
    
    if (all_success) {
        reportSuccess("All models loaded successfully");
    } else {
        std::stringstream error;
        error << "Failed to load " << failed_models.size() << " model(s): ";
        for (size_t i = 0; i < failed_models.size(); ++i) {
            if (i > 0) error << ", ";
            error << failed_models[i];
        }
        reportError(error.str());
    }
    
    return all_success;
}

bool GUICommandHandlers::handleFocusPanes(const std::vector<std::string>& pane_names) {
    if (verbose_) {
        std::cout << "[GUI] Focusing " << pane_names.size() << " panes" << std::endl;
    }
    
    bool all_success = true;
    std::vector<std::string> failed_panes;
    
    for (const auto& pane_name : pane_names) {
        if (!handleFocusPane(pane_name)) {
            all_success = false;
            failed_panes.push_back(pane_name);
        }
    }
    
    if (all_success) {
        reportSuccess("All panes focused successfully");
    } else {
        std::stringstream error;
        error << "Failed to focus " << failed_panes.size() << " pane(s): ";
        for (size_t i = 0; i < failed_panes.size(); ++i) {
            if (i > 0) error << ", ";
            error << failed_panes[i];
        }
        reportError(error.str());
    }
    
    return all_success;
}

// ============================================================================
// GUI ACTIONS
// ============================================================================

bool GUICommandHandlers::loadModelInGUI(const std::string& model_path) {
    if (!model_loader_widget_) {
        last_error_ = "Model loader widget not available";
        return false;
    }
    
    // Load model using widget
    if (!model_loader_widget_->loadModel(model_path)) {
        last_error_ = "Model loader widget failed to load model";
        return false;
    }
    
    // Show status message
    if (main_window_) {
        main_window_->showStatusMessage("Model loaded: " + model_path);
    }
    
    // Auto-focus if enabled
    if (auto_focus_ && chat_pane_) {
        chat_pane_->focus();
    }
    
    return true;
}

bool GUICommandHandlers::unloadModelFromGUI() {
    if (!model_loader_widget_) {
        last_error_ = "Model loader widget not available";
        return false;
    }
    
    // Unload model using widget
    if (!model_loader_widget_->unloadModel()) {
        last_error_ = "Model loader widget failed to unload model";
        return false;
    }
    
    // Show status message
    if (main_window_) {
        main_window_->showStatusMessage("Model unloaded");
    }
    
    return true;
}

bool GUICommandHandlers::focusPaneInGUI(const std::string& pane_name) {
    if (!main_window_) {
        last_error_ = "Main window not available";
        return false;
    }
    
    // Focus pane using main window
    main_window_->focusPane(pane_name);
    
    return true;
}

bool GUICommandHandlers::showChatInGUI() {
    if (!chat_pane_) {
        last_error_ = "Chat pane not available";
        return false;
    }
    
    // Show chat pane
    chat_pane_->show();
    chat_pane_->focus();
    
    return true;
}

bool GUICommandHandlers::hideChatInGUI() {
    if (!chat_pane_) {
        last_error_ = "Chat pane not available";
        return false;
    }
    
    // Hide chat pane
    chat_pane_->hide();
    
    return true;
}

bool GUICommandHandlers::executeCommandInGUI(const std::string& command) {
    if (!terminal_manager_) {
        last_error_ = "Terminal manager not available";
        return false;
    }
    
    // Execute command using terminal manager
    terminal_manager_->executeCommand(command);
    
    return true;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool GUICommandHandlers::validateModelPath(const std::string& path) {
    // Check if path is empty
    if (path.empty()) {
        last_error_ = "Model path cannot be empty";
        return false;
    }
    
    // Check if path ends with .gguf
    if (path.length() < 5 || path.substr(path.length() - 5) != ".gguf") {
        // Allow non-.gguf paths for now (could be directory or other format)
        if (verbose_) {
            std::cout << "[GUI] Warning: Model path does not end with .gguf: " << path << std::endl;
        }
    }
    
    // Additional validation could be added here (file existence, permissions, etc.)
    
    return true;
}

bool GUICommandHandlers::validatePaneName(const std::string& pane_name) {
    // Define valid pane names
    static const std::vector<std::string> valid_panes = {
        "chat", "model_loader", "terminal", "settings", "about"
    };
    
    // Check if pane name is valid
    auto it = std::find(valid_panes.begin(), valid_panes.end(), pane_name);
    if (it == valid_panes.end()) {
        last_error_ = "Invalid pane name: " + pane_name;
        return false;
    }
    
    return true;
}

// ============================================================================
// STATUS AND ERROR REPORTING
// ============================================================================

void GUICommandHandlers::reportStatus(const std::string& status) {
    if (verbose_) {
        std::cout << "[GUI] Status: " << status << std::endl;
    }
    
    // Update CLI status
    updateCLIStatus(status);
    
    // Show status in GUI
    if (main_window_) {
        main_window_->showStatusMessage(status);
    }
}

void GUICommandHandlers::reportError(const std::string& error) {
    last_error_ = error;
    
    if (verbose_) {
        std::cerr << "[GUI] Error: " << error << std::endl;
    }
    
    // Show error in GUI
    if (main_window_) {
        main_window_->showErrorMessage(error);
    }
}

void GUICommandHandlers::reportSuccess(const std::string& message) {
    if (verbose_) {
        std::cout << "[GUI] Success: " << message << std::endl;
    }
    
    // Show success in GUI
    if (main_window_) {
        main_window_->showStatusMessage("✓ " + message);
    }
}

void GUICommandHandlers::updateCLIStatus(const std::string& status) {
    // This would typically send a message back to CLI
    // For now, we'll just log it
    if (verbose_) {
        std::cout << "[CLI Update] " << status << std::endl;
    }
}

void GUICommandHandlers::reportCommandResult(const std::string& command, bool success,
                                           const std::string& message) {
    std::stringstream result;
    result << "Command: " << command << " - " << (success ? "SUCCESS" : "FAILED");
    if (!message.empty()) {
        result << " - " << message;
    }
    
    if (success) {
        reportSuccess(result.str());
    } else {
        reportError(result.str());
    }
}

// ============================================================================
// COMMAND REGISTRY IMPLEMENTATION
// ============================================================================

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

void CommandRegistry::registerCommand(const CommandInfo& cmd) {
    commands_[cmd.name] = cmd;
    
    // Register aliases
    for (const auto& alias : cmd.aliases) {
        commands_[alias] = cmd;
    }
}

const CommandInfo* CommandRegistry::getCommand(const std::string& name) const {
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> CommandRegistry::getAllCommandNames() const {
    std::vector<std::string> names;
    for (const auto& pair : commands_) {
        // Only return primary names, not aliases
        if (pair.second.name == pair.first) {
            names.push_back(pair.first);
        }
    }
    return names;
}

std::vector<CommandInfo> CommandRegistry::getAllCommands() const {
    std::vector<CommandInfo> commands;
    for (const auto& pair : commands_) {
        // Only return primary commands, not aliases
        if (pair.second.name == pair.first) {
            commands.push_back(pair.second);
        }
    }
    return commands;
}

// ============================================================================
// COMMAND LINE INTERFACE IMPLEMENTATION
// ============================================================================

GUICommandLineInterface::GUICommandLineInterface(GUICommandHandlers* handlers)
    : handlers_(handlers) {
    // Initialize argument parser
    parser_ = std::make_unique<CLIArgumentParser>();
}

bool GUICommandLineInterface::processCommandLine(int argc, char* argv[]) {
    if (!handlers_) {
        std::cerr << "Error: Command handlers not initialized" << std::endl;
        return false;
    }
    
    // Parse arguments
    auto result = parser_->parse(argc, argv);
    if (!result.success) {
        std::cerr << "Error: " << result.error_message << std::endl;
        return false;
    }
    
    // Process commands
    for (const auto& cmd : result.commands) {
        if (!handlers_->handleCommand(cmd.name, cmd.args)) {
            return false;
        }
    }
    
    return true;
}

bool GUICommandLineInterface::processCommand(const std::string& command_line) {
    if (!handlers_) {
        std::cerr << "Error: Command handlers not initialized" << std::endl;
        return false;
    }
    
    // Parse command line
    auto result = parser_->parse(command_line);
    if (!result.success) {
        std::cerr << "Error: " << result.error_message << std::endl;
        return false;
    }
    
    // Process commands
    for (const auto& cmd : result.commands) {
        if (!handlers_->handleCommand(cmd.name, cmd.args)) {
            return false;
        }
    }
    
    return true;
}

void GUICommandLineInterface::showHelp() const {
    std::cout << "RawrXD GUI Command Line Interface" << std::endl;
    std::cout << "Usage: rawrxd-gui [options] <command> [args...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -v, --verbose  Enable verbose output" << std::endl;
    std::cout << "  --version      Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    
    auto commands = CommandRegistry::instance().getAllCommands();
    for (const auto& cmd : commands) {
        std::cout << "  " << std::left << std::setw(20) << cmd.name << cmd.description << std::endl;
        if (!cmd.aliases.empty()) {
            std::cout << "    Aliases: ";
            for (size_t i = 0; i < cmd.aliases.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << cmd.aliases[i];
            }
            std::cout << std::endl;
        }
        std::cout << "    Usage: " << cmd.usage << std::endl;
    }
}

void GUICommandLineInterface::showCommandHelp(const std::string& command) const {
    const CommandInfo* cmd_info = CommandRegistry::instance().getCommand(command);
    
    if (!cmd_info) {
        std::cerr << "Unknown command: " << command << std::endl;
        return;
    }
    
    std::cout << "Command: " << cmd_info->name << std::endl;
    std::cout << "Description: " << cmd_info->description << std::endl;
    std::cout << "Usage: " << cmd_info->usage << std::endl;
    
    if (!cmd_info->aliases.empty()) {
        std::cout << "Aliases: ";
        for (size_t i = 0; i < cmd_info->aliases.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << cmd_info->aliases[i];
        }
        std::cout << std::endl;
    }
}

void GUICommandLineInterface::showVersion() const {
    std::cout << "RawrXD GUI Command Line Interface v1.0.0" << std::endl;
    std::cout << "Built with production-ready IPC layer" << std::endl;
}

} // namespace GUI
} // namespace RawrXD