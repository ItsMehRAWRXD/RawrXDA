// ============================================================================
// GUI IPC Server - Production Implementation
// ============================================================================
// Listens for CLI commands via IPC and processes them through command handlers
// ============================================================================

#pragma once

#include "cli_bridge.hpp"
#include "command_handlers.hpp"
#include "command_registry.hpp"
#include <atomic>
#include <thread>
#include <memory>
#include <functional>

namespace RawrXD {
namespace GUI {

// ============================================================================
// GUI IPC SERVER CLASS
// ============================================================================

/**
 * @class GUIIPCServer
 * @brief IPC server that listens for CLI commands and processes them
 * 
 * Features:
 * - Accepts connections from CLI processes
 * - Receives and parses command messages
 * - Executes commands via GUICommandHandlers
 * - Sends responses back to CLI
 * - Supports both synchronous and asynchronous operation
 */
class GUIIPCServer {
public:
    GUIIPCServer();
    ~GUIIPCServer();
    
    // Non-copyable
    GUIIPCServer(const GUIIPCServer&) = delete;
    GUIIPCServer& operator=(const GUIIPCServer&) = delete;
    
    // Server lifecycle
    bool start(const std::string& server_name = "rawrxd-gui");
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Configuration
    void setVerbose(bool verbose);
    void setAutoFocus(bool auto_focus);
    void setMainWindow(RawrXDMainWindow* main_window);
    
    // Callbacks
    void setCommandReceivedCallback(std::function<void(const std::string&)> callback);
    void setCommandExecutedCallback(std::function<void(const std::string&, bool)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
    // Command processing
    bool processCommand(const std::string& command_line);
    bool processCommand(const std::string& command, const std::vector<std::string>& args);
    
    // Status and statistics
    size_t getCommandCount() const { return command_count_.load(); }
    size_t getSuccessCount() const { return success_count_.load(); }
    size_t getErrorCount() const { return error_count_.load(); }
    std::string getLastError() const { return last_error_; }
    
    // Server information
    std::string getServerName() const { return server_name_; }
    std::string getServerPath() const { return server_path_; }
    
private:
    // Server components
    std::unique_ptr<IPCServer> ipc_server_;
    std::unique_ptr<GUICommandHandlers> command_handlers_;
    
    // State
    std::atomic<bool> running_;
    std::atomic<size_t> command_count_;
    std::atomic<size_t> success_count_;
    std::atomic<size_t> error_count_;
    std::string last_error_;
    std::string server_name_;
    std::string server_path_;
    
    // Configuration
    bool verbose_;
    bool auto_focus_;
    RawrXDMainWindow* main_window_;
    
    // Callbacks
    std::function<void(const std::string&)> command_received_callback_;
    std::function<void(const std::string&, bool)> command_executed_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    // Message processing
    void onMessageReceived(const IPCMessage& message);
    void onConnectionAccepted(const IPCConnection& connection);
    void onConnectionClosed(const IPCConnection& connection);
    void onError(const std::string& error);
    
    // Command execution
    bool executeCommand(const std::string& command_line);
    void reportSuccess(const std::string& command);
    void reportError(const std::string& command, const std::string& error);
    void reportStatus(const std::string& status);
    
    // Response sending
    bool sendResponse(const IPCConnection& connection, const std::string& response);
    bool sendError(const IPCConnection& connection, const std::string& error);
    bool sendStatus(const IPCConnection& connection, const std::string& status);
    
    // Utility methods
    std::string buildResponse(const std::string& command, bool success, 
                             const std::string& message = "");
    std::string buildErrorResponse(const std::string& command, const std::string& error);
    std::string buildStatusResponse(const std::string& status);
    
    // Connection management
    void handleConnection(std::shared_ptr<IPCConnection> connection);
    void processConnectionMessages(IPCConnection* connection);
};

// ============================================================================
// GUI APPLICATION CLASS
// ============================================================================

/**
 * @class GUIApplication
 * @brief Main GUI application that manages the IPC server and GUI components
 * 
 * Features:
 * - Initializes the GUI application
 * - Sets up the IPC server
 * - Manages the main window
 * - Processes events and commands
 * - Handles cleanup on shutdown
 */
class GUIApplication {
public:
    GUIApplication(int argc, char* argv[]);
    ~GUIApplication();
    
    // Non-copyable
    GUIApplication(const GUIApplication&) = delete;
    GUIApplication& operator=(const GUIApplication&) = delete;
    
    // Application lifecycle
    int run();
    void exit(int exit_code = 0);
    
    // Configuration
    void setVerbose(bool verbose);
    void setAutoFocus(bool auto_focus);
    void setServerName(const std::string& server_name);
    
    // Component access
    GUIIPCServer* getIPCServer() { return ipc_server_.get(); }
    RawrXDMainWindow* getMainWindow() { return main_window_; }
    GUICommandHandlers* getCommandHandlers();
    
    // Command processing
    bool processCommand(const std::string& command_line);
    bool processCommand(const std::string& command, const std::vector<std::string>& args);
    
    // Status and information
    bool isRunning() const { return running_.load(); }
    std::string getApplicationName() const { return app_name_; }
    std::string getApplicationVersion() const { return app_version_; }
    
private:
    // Application components
    std::unique_ptr<GUIIPCServer> ipc_server_;
    RawrXDMainWindow* main_window_;
    
    // State
    std::atomic<bool> running_;
    std::string app_name_;
    std::string app_version_;
    int argc_;
    char** argv_;
    
    // Configuration
    bool verbose_;
    bool auto_focus_;
    std::string server_name_;
    
    // Initialization
    bool initialize();
    bool initializeIPCServer();
    bool initializeMainWindow();
    bool initializeCommandHandlers();
    
    // Cleanup
    void cleanup();
    void cleanupIPCServer();
    void cleanupMainWindow();
    
    // Event processing
    void processEvents();
    void processPendingCommands();
    
    // Command queue
    std::vector<std::string> pending_commands_;
    std::mutex command_mutex_;
    
    // Callbacks
    void onCommandReceived(const std::string& command);
    void onCommandExecuted(const std::string& command, bool success);
    void onError(const std::string& error);
};

// ============================================================================
// GLOBAL APPLICATION INSTANCE
// ============================================================================

/**
 * @brief Get the global GUI application instance
 * @return Pointer to the GUI application instance
 */
GUIApplication* getGUIApplication();

/**
 * @brief Initialize the global GUI application
 * @param argc Argument count
 * @param argv Argument values
 * @return true if initialization was successful, false otherwise
 */
bool initializeGUIApplication(int argc, char* argv[]);

/**
 * @brief Shutdown the global GUI application
 */
void shutdownGUIApplication();

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

/**
 * @brief Main entry point for the GUI application
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int mainGUI(int argc, char* argv[]);

} // namespace GUI
} // namespace RawrXD