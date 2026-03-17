// ============================================================================
// CLI Main Entry Point - Production Implementation
// ============================================================================
// Main entry point for CLI→GUI communication
// ============================================================================

#include "cli_ipc_client.hpp"
#include "cli_command_integration.hpp"
#include "cli_local_model.hpp"
#include "../gui/command_handlers.hpp"
#include "../gui/command_registry.hpp"
#include "../gui/cli_bridge.hpp"
#include "../core/local_gguf_loader.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace RawrXD {
namespace CLI {

// ============================================================================
// GLOBAL CLI INTEGRATION INSTANCE
// ============================================================================

static CLICommandIntegration* global_cli_integration = nullptr;

CLICommandIntegration* getCLICommandIntegration() {
    return global_cli_integration;
}

bool initializeCLICommandIntegration(const std::string& server_name) {
    if (global_cli_integration) {
        return false; // Already initialized
    }
    
    global_cli_integration = new CLICommandIntegration();
    if (!global_cli_integration->initialize(server_name)) {
        delete global_cli_integration;
        global_cli_integration = nullptr;
        return false;
    }
    
    return true;
}

void shutdownCLICommandIntegration() {
    if (global_cli_integration) {
        global_cli_integration->shutdown();
        delete global_cli_integration;
        global_cli_integration = nullptr;
    }
}

// ============================================================================
// COMMAND LINE PARSER
// ============================================================================

class CommandLineParser {
public:
    struct Options {
        bool verbose = false;
        bool show_progress = true;
        bool auto_reconnect = true;
        int timeout_ms = 5000;
        std::string server_name = "rawrxd-gui";
        std::string command;
        std::vector<std::string> args;
        bool show_help = false;
        bool show_version = false;
    };
    
    CommandLineParser(int argc, char* argv[]) : argc_(argc), argv_(argv) {}
    
    bool parse(Options& options) {
        if (argc_ < 2) {
            options.show_help = true;
            return true;
        }
        
        for (int i = 1; i < argc_; ++i) {
            std::string arg = argv_[i];
            
            if (arg == "-h" || arg == "--help") {
                options.show_help = true;
                return true;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "--no-progress") {
                options.show_progress = false;
            } else if (arg == "--no-reconnect") {
                options.auto_reconnect = false;
            } else if (arg == "--timeout" && i + 1 < argc_) {
                options.timeout_ms = std::atoi(argv_[++i]);
            } else if (arg == "--server" && i + 1 < argc_) {
                options.server_name = argv_[++i];
            } else if (arg == "--version") {
                options.show_version = true;
                return true;
            } else if (options.command.empty()) {
                options.command = arg;
            } else {
                options.args.push_back(arg);
            }
        }
        
        return true;
    }
    
    void showHelp() const {
        std::cout << "RawrXD CLI→GUI Command Interface" << std::endl;
        std::cout << "Usage: " << argv_[0] << " [options] <command> [args...]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -h, --help            Show this help message" << std::endl;
        std::cout << "  -v, --verbose         Enable verbose output" << std::endl;
        std::cout << "  --no-progress         Disable progress reporting" << std::endl;
        std::cout << "  --no-reconnect        Disable auto-reconnect" << std::endl;
        std::cout << "  --timeout <ms>        Set timeout in milliseconds (default: 5000)" << std::endl;
        std::cout << "  --server <name>       Set GUI server name (default: rawrxd-gui)" << std::endl;
        std::cout << "  --version             Show version information" << std::endl;
        std::cout << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  load-model <path>     Load a model in GUI" << std::endl;
        std::cout << "  unload-model          Unload current model" << std::endl;
        std::cout << "  focus-pane <name>     Focus a GUI pane" << std::endl;
        std::cout << "  show-chat             Show chat pane" << std::endl;
        std::cout << "  hide-chat             Hide chat pane" << std::endl;
        std::cout << "  get-status            Get GUI status" << std::endl;
        std::cout << "  execute <command>     Execute system command" << std::endl;
        std::cout << "  load-models <paths>   Load multiple models" << std::endl;
        std::cout << "  focus-panes <names>   Focus multiple panes" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv_[0] << " load-model /path/to/model.gguf" << std::endl;
        std::cout << "  " << argv_[0] << " focus-pane chat" << std::endl;
        std::cout << "  " << argv_[0] << " get-status" << std::endl;
        std::cout << "  " << argv_[0] << " -v load-models model1.gguf model2.gguf" << std::endl;
    }
    
    void showVersion() const {
        std::cout << "RawrXD CLI→GUI Command Interface v1.0.0" << std::endl;
        std::cout << "Built with production-ready IPC layer" << std::endl;
    }
    
private:
    int argc_;
    char** argv_;
};

// ============================================================================
// COMMAND EXECUTOR
// ============================================================================

class CommandExecutor {
public:
    CommandExecutor(CLICommandIntegration* integration) : integration_(integration) {}
    
    bool execute(const std::string& command, const std::vector<std::string>& args) {
        if (command == "load-model") {
            return executeLoadModel(args);
        } else if (command == "unload-model") {
            return executeUnloadModel(args);
        } else if (command == "focus-pane") {
            return executeFocusPane(args);
        } else if (command == "show-chat") {
            return executeShowChat(args);
        } else if (command == "hide-chat") {
            return executeHideChat(args);
        } else if (command == "get-status") {
            return executeGetStatus(args);
        } else if (command == "execute") {
            return executeCommand(args);
        } else if (command == "load-models") {
            return executeLoadModels(args);
        } else if (command == "focus-panes") {
            return executeFocusPanes(args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            return false;
        }
    }
    
private:
    bool executeLoadModel(const std::vector<std::string>& args) {
        if (args.size() != 1) {
            std::cerr << "Usage: load-model <path>" << std::endl;
            return false;
        }
        return integration_->loadModel(args[0]);
    }
    
    bool executeUnloadModel(const std::vector<std::string>& args) {
        if (!args.empty()) {
            std::cerr << "Usage: unload-model" << std::endl;
            return false;
        }
        return integration_->unloadModel();
    }
    
    bool executeFocusPane(const std::vector<std::string>& args) {
        if (args.size() != 1) {
            std::cerr << "Usage: focus-pane <name>" << std::endl;
            return false;
        }
        return integration_->focusPane(args[0]);
    }
    
    bool executeShowChat(const std::vector<std::string>& args) {
        if (!args.empty()) {
            std::cerr << "Usage: show-chat" << std::endl;
            return false;
        }
        return integration_->showChat();
    }
    
    bool executeHideChat(const std::vector<std::string>& args) {
        if (!args.empty()) {
            std::cerr << "Usage: hide-chat" << std::endl;
            return false;
        }
        return integration_->hideChat();
    }
    
    bool executeGetStatus(const std::vector<std::string>& args) {
        if (!args.empty()) {
            std::cerr << "Usage: get-status" << std::endl;
            return false;
        }
        return integration_->getStatus();
    }
    
    bool executeCommand(const std::vector<std::string>& args) {
        if (args.empty()) {
            std::cerr << "Usage: execute <command>" << std::endl;
            return false;
        }
        std::string command = args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            command += " " + args[i];
        }
        return integration_->executeSystemCommand(command);
    }
    
    bool executeLoadModels(const std::vector<std::string>& args) {
        if (args.empty()) {
            std::cerr << "Usage: load-models <path1> [path2...]" << std::endl;
            return false;
        }
        return integration_->loadModels(args);
    }
    
    bool executeFocusPanes(const std::vector<std::string>& args) {
        if (args.empty()) {
            std::cerr << "Usage: focus-panes <name1> [name2...]" << std::endl;
            return false;
        }
        return integration_->focusPanes(args);
    }
    
    CLICommandIntegration* integration_;
};

// ============================================================================
// MAIN CLI ENTRY POINT
// ============================================================================

int mainCLI(int argc, char* argv[]) {
    CommandLineParser parser(argc, argv);
    CommandLineParser::Options options;
    
    if (!parser.parse(options)) {
        std::cerr << "Failed to parse command line arguments" << std::endl;
        return 1;
    }
    
    if (options.show_help) {
        parser.showHelp();
        return 0;
    }
    
    if (options.show_version) {
        parser.showVersion();
        return 0;
    }
    
    if (options.command.empty()) {
        std::cerr << "No command specified" << std::endl;
        parser.showHelp();
        return 1;
    }
    
    // Initialize CLI integration
    if (!initializeCLICommandIntegration(options.server_name)) {
        std::cerr << "Failed to initialize CLI integration" << std::endl;
        return 1;
    }
    
    CLICommandIntegration* integration = getCLICommandIntegration();
    if (!integration) {
        std::cerr << "Failed to get CLI integration instance" << std::endl;
        shutdownCLICommandIntegration();
        return 1;
    }
    
    // Configure integration
    integration->setVerbose(options.verbose);
    integration->setShowProgress(options.show_progress);
    integration->setAutoReconnect(options.auto_reconnect);
    integration->setTimeout(options.timeout_ms);
    
    // Execute command
    CommandExecutor executor(integration);
    bool success = executor.execute(options.command, options.args);
    
    if (!success) {
        std::cerr << "Command failed: " << integration->getLastError() << std::endl;
    }
    
    // Shutdown
    shutdownCLICommandIntegration();
    
    return success ? 0 : 1;
}

} // namespace CLI
} // namespace RawrXD

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char* argv[]) {
    return RawrXD::CLI::mainCLI(argc, argv);
}