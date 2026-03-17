// ============================================================================
// GUI Main Entry Point - Production Implementation
// ============================================================================
// Main entry point for GUI application with IPC server
// ============================================================================

#include "../gui/ipc_server.hpp"
#include "../gui/command_handlers.hpp"
#include "../gui/command_registry.hpp"
#include "../gui/cli_bridge.hpp"
#include "../cli/cli_local_model.hpp"
#include "../core/local_gguf_loader.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <csignal>

namespace RawrXD {
namespace GUI {

// ============================================================================
// SIGNAL HANDLING
// ============================================================================

static GUIIPCServer* global_server = nullptr;
static bool shutdown_requested = false;

void signalHandler(int signal) {
    std::cout << "\n[GUI] Signal received: " << signal << std::endl;
    
    if (global_server) {
        std::cout << "[GUI] Shutting down server..." << std::endl;
        global_server->stop();
    }
    
    shutdown_requested = true;
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifdef _WIN32
    std::signal(SIGBREAK, signalHandler);
#else
    std::signal(SIGQUIT, signalHandler);
#endif
}

// ============================================================================
// COMMAND LINE PARSER
// ============================================================================

class CommandLineParser {
public:
    struct Options {
        bool verbose = false;
        bool auto_focus = true;
        std::string server_name = "rawrxd-gui";
        bool show_help = false;
        bool show_version = false;
        bool daemon_mode = false;
    };
    
    CommandLineParser(int argc, char* argv[]) : argc_(argc), argv_(argv) {}
    
    bool parse(Options& options) {
        for (int i = 1; i < argc_; ++i) {
            std::string arg = argv_[i];
            
            if (arg == "-h" || arg == "--help") {
                options.show_help = true;
                return true;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "--no-focus") {
                options.auto_focus = false;
            } else if (arg == "--server-name" && i + 1 < argc_) {
                options.server_name = argv_[++i];
            } else if (arg == "--daemon") {
                options.daemon_mode = true;
            } else if (arg == "--version") {
                options.show_version = true;
                return true;
            }
        }
        
        return true;
    }
    
    void showHelp() const {
        std::cout << "RawrXD GUI Application with IPC Server" << std::endl;
        std::cout << "Usage: " << argv_[0] << " [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -h, --help            Show this help message" << std::endl;
        std::cout << "  -v, --verbose         Enable verbose output" << std::endl;
        std::cout << "  --no-focus            Disable auto-focus on commands" << std::endl;
        std::cout << "  --server-name <name>  Set server name (default: rawrxd-gui)" << std::endl;
        std::cout << "  --daemon              Run in daemon mode (no GUI)" << std::endl;
        std::cout << "  --version             Show version information" << std::endl;
        std::cout << std::endl;
        std::cout << "Description:" << std::endl;
        std::cout << "  Starts the RawrXD GUI application with an IPC server that" << std::endl;
        std::cout << "  listens for commands from CLI processes. The server runs" << std::endl;
        std::cout << "  continuously until stopped with SIGINT (Ctrl+C) or SIGTERM." << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv_[0] << " --verbose" << std::endl;
        std::cout << "  " << argv_[0] << " --server-name my-gui" << std::endl;
        std::cout << "  " << argv_[0] << " --daemon --verbose" << std::endl;
        std::cout << std::endl;
        std::cout << "CLI Usage (from another terminal):" << std::endl;
        std::cout << "  rawrxd-cli load-model /path/to/model.gguf" << std::endl;
        std::cout << "  rawrxd-cli focus-pane chat" << std::endl;
        std::cout << "  rawrxd-cli get-status" << std::endl;
    }
    
    void showVersion() const {
        std::cout << "RawrXD GUI Application v1.0.0" << std::endl;
        std::cout << "Built with production-ready IPC layer" << std::endl;
        std::cout << "Supports CLI→GUI command communication" << std::endl;
    }
    
private:
    int argc_;
    char** argv_;
};

// ============================================================================
// MAIN GUI ENTRY POINT
// ============================================================================

int mainGUI(int argc, char* argv[]) {
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
    
    std::cout << "RawrXD GUI Application Starting..." << std::endl;
    
    // Setup signal handlers
    setupSignalHandlers();
    
    // Initialize command registry
    if (!initializeCommandRegistry()) {
        std::cerr << "Failed to initialize command registry" << std::endl;
        return 1;
    }
    
    // Create IPC server
    GUIIPCServer server;
    server.setVerbose(options.verbose);
    server.setAutoFocus(options.auto_focus);
    
    // Start server
    if (!server.start(options.server_name)) {
        std::cerr << "Failed to start IPC server: " << server.getLastError() << std::endl;
        return 1;
    }
    
    // Store global server reference for signal handling
    global_server = &server;
    
    std::cout << "RawrXD GUI Application Started Successfully" << std::endl;
    std::cout << "Server: " << server.getServerPath() << std::endl;
    std::cout << "Mode: " << (options.daemon_mode ? "Daemon" : "GUI") << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    if (options.daemon_mode) {
        // Daemon mode: just run the server
        std::cout << "Running in daemon mode (no GUI)" << std::endl;
        
        while (!shutdown_requested && server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        // GUI mode: would initialize actual GUI here
        std::cout << "GUI mode selected (GUI components not yet implemented)" << std::endl;
        std::cout << "Running server only..." << std::endl;
        
        while (!shutdown_requested && server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // Stop server
    std::cout << "Stopping server..." << std::endl;
    server.stop();
    
    std::cout << "RawrXD GUI Application Stopped" << std::endl;
    
    return 0;
}

} // namespace GUI
} // namespace RawrXD

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char* argv[]) {
    return RawrXD::GUI::mainGUI(argc, argv);
}