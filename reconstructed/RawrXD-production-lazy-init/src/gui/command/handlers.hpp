// ============================================================================
// GUI Command Handlers - Production Implementation
// ============================================================================
// Processes incoming CLI commands and executes corresponding GUI actions
// ============================================================================

#pragma once

#include "../gui/cli_bridge.hpp"
#include <string>
#include <functional>
#include <memory>

// Forward declarations
class RawrXDMainWindow;
class ModelLoaderWidget;
class AgentChatPane;
class TerminalManager;

namespace RawrXD {
namespace GUI {

// ============================================================================
// GUI COMMAND HANDLERS CLASS
// ============================================================================

/**
 * @class GUICommandHandlers
 * @brief Processes incoming CLI commands and executes GUI actions
 * 
 * Features:
 * - Command parsing and validation
 * - GUI action execution
 * - Error handling and reporting
 * - Status updates to CLI
 * - Integration with main window components
 */
class GUICommandHandlers {
public:
    GUICommandHandlers(RawrXDMainWindow* main_window);
    ~GUICommandHandlers();
    
    // Non-copyable
    GUICommandHandlers(const GUICommandHandlers&) = delete;
    GUICommandHandlers& operator=(const GUICommandHandlers&) = delete;
    
    // Command processing
    bool handleMessage(const IPCMessage& message);
    bool handleCommand(const std::string& command, const std::vector<std::string>& args);
    
    // Individual command handlers
    bool handleLoadModel(const std::string& model_path);
    bool handleUnloadModel();
    bool handleFocusPane(const std::string& pane_name);
    bool handleShowChat();
    bool handleHideChat();
    bool handleGetStatus();
    bool handleExecuteCommand(const std::string& command);
    
    // Batch operations
    bool handleLoadModels(const std::vector<std::string>& model_paths);
    bool handleFocusPanes(const std::vector<std::string>& pane_names);
    
    // Status and error reporting
    void reportStatus(const std::string& status);
    void reportError(const std::string& error);
    void reportSuccess(const std::string& message);
    
    // Configuration
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setAutoFocus(bool auto_focus) { auto_focus_ = auto_focus; }
    
    // Callbacks for GUI events
    void setModelLoadedCallback(std::function<void(const std::string&)> callback) {
        model_loaded_callback_ = callback;
    }
    
    void setModelUnloadedCallback(std::function<void()> callback) {
        model_unloaded_callback_ = callback;
    }
    
    void setPaneFocusedCallback(std::function<void(const std::string&)> callback) {
        pane_focused_callback_ = callback;
    }
    
    // Getters
    bool isModelLoaded() const { return model_loaded_; }
    std::string getCurrentModelPath() const { return current_model_path_; }
    std::string getLastError() const { return last_error_; }
    
private:
    RawrXDMainWindow* main_window_;
    ModelLoaderWidget* model_loader_widget_;
    AgentChatPane* chat_pane_;
    TerminalManager* terminal_manager_;
    
    // State
    bool model_loaded_;
    std::string current_model_path_;
    std::string last_error_;
    bool verbose_;
    bool auto_focus_;
    
    // Callbacks
    std::function<void(const std::string&)> model_loaded_callback_;
    std::function<void()> model_unloaded_callback_;
    std::function<void(const std::string&)> pane_focused_callback_;
    
    // Command parsing
    std::vector<std::string> tokenizeCommand(const std::string& command);
    bool validateModelPath(const std::string& path);
    bool validatePaneName(const std::string& pane_name);
    
    // GUI actions
    bool loadModelInGUI(const std::string& model_path);
    bool unloadModelFromGUI();
    bool focusPaneInGUI(const std::string& pane_name);
    bool showChatInGUI();
    bool hideChatInGUI();
    bool executeCommandInGUI(const std::string& command);
    
    // Status reporting
    void updateCLIStatus(const std::string& status);
    void reportCommandResult(const std::string& command, bool success, 
                            const std::string& message = "");
};

// ============================================================================
// COMMAND REGISTRY
// ============================================================================

struct CommandInfo {
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
    std::function<bool(GUICommandHandlers*, const std::vector<std::string>&)> handler;
    std::string usage;
    bool requires_args;
    int min_args;
    int max_args;
};

class CommandRegistry {
public:
    static CommandRegistry& instance();
    
    void registerCommand(const CommandInfo& cmd);
    const CommandInfo* getCommand(const std::string& name) const;
    std::vector<std::string> getAllCommandNames() const;
    std::vector<CommandInfo> getAllCommands() const;
    
private:
    CommandRegistry() = default;
    std::map<std::string, CommandInfo> commands_;
};

// ============================================================================
// COMMAND LINE INTEGRATION
// ============================================================================

/**
 * @class GUICommandLineInterface
 * @brief Integrates command handlers with CLI argument parser
 */
class GUICommandLineInterface {
public:
    GUICommandLineInterface(GUICommandHandlers* handlers);
    
    bool processCommandLine(int argc, char* argv[]);
    bool processCommand(const std::string& command_line);
    
    void showHelp() const;
    void showCommandHelp(const std::string& command) const;
    void showVersion() const;
    
private:
    GUICommandHandlers* handlers_;
    std::unique_ptr<CLIArgumentParser> parser_;
};

} // namespace GUI
} // namespace RawrXD