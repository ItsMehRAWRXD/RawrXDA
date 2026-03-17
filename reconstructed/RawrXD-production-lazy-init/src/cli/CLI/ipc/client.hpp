// ============================================================================
// CLI IPC Client - Production Implementation
// ============================================================================
// Connects to GUI IPC server and sends commands
// ============================================================================

#pragma once

#include "../gui/cli_bridge.hpp"
#include "../gui/command_handlers.hpp"
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace RawrXD {
namespace CLI {

// ============================================================================
// CLI IPC CLIENT CLASS
// ============================================================================

/**
 * @class CLIIPCClient
 * @brief IPC client that connects to GUI server and sends commands
 * 
 * Features:
 * - Connects to GUI IPC server
 * - Sends commands to GUI
 * - Receives responses from GUI
 * - Handles connection errors and reconnections
 * - Supports both synchronous and asynchronous operation
 */
class CLIIPCClient {
public:
    CLIIPCClient();
    ~CLIIPCClient();
    
    // Non-copyable
    CLIIPCClient(const CLIIPCClient&) = delete;
    CLIIPCClient& operator=(const CLIIPCClient&) = delete;
    
    // Client lifecycle
    bool connect(const std::string& server_name = "rawrxd-gui");
    void disconnect();
    bool isConnected() const { return connected_.load(); }
    
    // Configuration
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setTimeout(int timeout_ms) { timeout_ms_ = timeout_ms; }
    void setAutoReconnect(bool auto_reconnect) { auto_reconnect_ = auto_reconnect; }
    void setMaxReconnectAttempts(int max_attempts) { max_reconnect_attempts_ = max_attempts; }
    
    // Command sending
    bool sendCommand(const std::string& command_line);
    bool sendCommand(const std::string& command, const std::vector<std::string>& args);
    
    // Response handling
    std::string getLastResponse() const { return last_response_; }
    std::string getLastError() const { return last_error_; }
    bool getLastSuccess() const { return last_success_; }
    
    // Status and statistics
    size_t getCommandCount() const { return command_count_.load(); }
    size_t getSuccessCount() const { return success_count_.load(); }
    size_t getErrorCount() const { return error_count_.load(); }
    size_t getReconnectCount() const { return reconnect_count_.load(); }
    
    // Callbacks
    void setCommandSentCallback(std::function<void(const std::string&)> callback);
    void setResponseReceivedCallback(std::function<void(const std::string&, bool)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setConnectionStatusCallback(std::function<void(bool)> callback);
    
    // Server information
    std::string getServerName() const { return server_name_; }
    std::string getServerPath() const { return server_path_; }
    std::string getClientId() const { return client_id_; }
    
private:
    // Client components
    std::unique_ptr<IPCClient> ipc_client_;
    
    // State
    std::atomic<bool> connected_;
    std::atomic<bool> connecting_;
    std::atomic<size_t> command_count_;
    std::atomic<size_t> success_count_;
    std::atomic<size_t> error_count_;
    std::atomic<size_t> reconnect_count_;
    std::atomic<int> reconnect_attempts_;
    std::string last_response_;
    std::string last_error_;
    bool last_success_;
    std::string server_name_;
    std::string server_path_;
    std::string client_id_;
    
    // Configuration
    bool verbose_;
    int timeout_ms_;
    bool auto_reconnect_;
    int max_reconnect_attempts_;
    
    // Callbacks
    std::function<void(const std::string&)> command_sent_callback_;
    std::function<void(const std::string&, bool)> response_received_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::function<void(bool)> connection_status_callback_;
    
    // Message processing
    void onMessageReceived(const IPCMessage& message);
    void onConnected();
    void onDisconnected();
    void onError(const std::string& error);
    
    // Command execution
    bool executeCommand(const std::string& command_line);
    void handleResponse(const std::string& response);
    void handleError(const std::string& error);
    void handleStatus(const std::string& status);
    
    // Reconnection
    void attemptReconnect();
    bool shouldReconnect() const;
    
    // Utility methods
    std::string buildCommandMessage(const std::string& command, const std::vector<std::string>& args);
    std::string generateClientId();
    void updateConnectionStatus(bool connected);
    void resetStatistics();
};

// ============================================================================
// COMMAND SENDER CLASS
// ============================================================================

/**
 * @class CommandSender
 * @brief Formats and sends commands to GUI via IPC client
 * 
 * Features:
 * - Command formatting and validation
 * - Batch command sending
 * - Progress reporting
 * - Error handling and recovery
 */
class CommandSender {
public:
    CommandSender(CLIIPCClient* client);
    ~CommandSender() = default;
    
    // Non-copyable
    CommandSender(const CommandSender&) = delete;
    CommandSender& operator=(const CommandSender&) = delete;
    
    // Command sending
    bool sendLoadModel(const std::string& model_path);
    bool sendUnloadModel();
    bool sendFocusPane(const std::string& pane_name);
    bool sendShowChat();
    bool sendHideChat();
    bool sendGetStatus();
    bool sendExecuteCommand(const std::string& command);
    
    // Batch operations
    bool sendLoadModels(const std::vector<std::string>& model_paths);
    bool sendFocusPanes(const std::vector<std::string>& pane_names);
    
    // Custom commands
    bool sendCommand(const std::string& command, const std::vector<std::string>& args);
    bool sendCommandLine(const std::string& command_line);
    
    // Configuration
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setShowProgress(bool show_progress) { show_progress_ = show_progress; }
    void setBatchSize(size_t batch_size) { batch_size_ = batch_size; }
    
    // Status and results
    std::string getLastResponse() const { return last_response_; }
    std::string getLastError() const { return last_error_; }
    bool getLastSuccess() const { return last_success_; }
    
    // Batch operation results
    size_t getBatchSuccessCount() const { return batch_success_count_; }
    size_t getBatchErrorCount() const { return batch_error_count_; }
    std::vector<std::string> getBatchErrors() const { return batch_errors_; }
    
private:
    CLIIPCClient* client_;
    bool verbose_;
    bool show_progress_;
    size_t batch_size_;
    
    // Last operation results
    std::string last_response_;
    std::string last_error_;
    bool last_success_;
    
    // Batch operation results
    size_t batch_success_count_;
    size_t batch_error_count_;
    std::vector<std::string> batch_errors_;
    
    // Utility methods
    bool validateModelPath(const std::string& path);
    bool validatePaneName(const std::string& pane_name);
    void updateLastResults(bool success, const std::string& response, const std::string& error);
    void resetBatchResults();
    void addBatchError(const std::string& error);
    void showProgress(const std::string& message);
};

// ============================================================================
// RESPONSE HANDLER CLASS
// ============================================================================

/**
 * @class ResponseHandler
 * @brief Processes responses from GUI and provides user feedback
 * 
 * Features:
 * - Response parsing and validation
 * - User-friendly output formatting
 * - Error message processing
 * - Progress and status reporting
 */
class ResponseHandler {
public:
    ResponseHandler();
    ~ResponseHandler() = default;
    
    // Response processing
    void handleResponse(const std::string& response);
    void handleError(const std::string& error);
    void handleStatus(const std::string& status);
    void handleSuccess(const std::string& message);
    
    // Output formatting
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setUseColors(bool use_colors) { use_colors_ = use_colors; }
    void setShowTimestamps(bool show_timestamps) { show_timestamps_ = show_timestamps; }
    
    // Output streams
    void setOutputStream(std::ostream* output) { output_stream_ = output; }
    void setErrorStream(std::ostream* error) { error_stream_ = error; }
    
    // Status and results
    std::string getLastResponse() const { return last_response_; }
    std::string getLastError() const { return last_error_; }
    bool getLastSuccess() const { return last_success_; }
    
    // Statistics
    size_t getResponseCount() const { return response_count_; }
    size_t getErrorCount() const { return error_count_; }
    size_t getSuccessCount() const { return success_count_; }
    
private:
    // Configuration
    bool verbose_;
    bool use_colors_;
    bool show_timestamps_;
    
    // Output streams
    std::ostream* output_stream_;
    std::ostream* error_stream_;
    
    // Last processed data
    std::string last_response_;
    std::string last_error_;
    bool last_success_;
    
    // Statistics
    size_t response_count_;
    size_t error_count_;
    size_t success_count_;
    
    // Utility methods
    std::string formatResponse(const std::string& response);
    std::string formatError(const std::string& error);
    std::string formatStatus(const std::string& status);
    std::string formatSuccess(const std::string& message);
    std::string getTimestamp();
    std::string colorize(const std::string& text, const std::string& color);
    void writeOutput(const std::string& message);
    void writeError(const std::string& message);
    void resetLastData();
};

// ============================================================================
// CLI COMMAND INTEGRATION
// ============================================================================

/**
 * @class CLICommandIntegration
 * @brief Integrates CLI commands with GUI IPC communication
 * 
 * Features:
 * - Unified interface for CLI→GUI communication
 * - Automatic connection management
 * - Error handling and recovery
 * - Progress reporting
 */
class CLICommandIntegration {
public:
    CLICommandIntegration();
    ~CLICommandIntegration();
    
    // Non-copyable
    CLICommandIntegration(const CLICommandIntegration&) = delete;
    CLICommandIntegration& operator=(const CLICommandIntegration&) = delete;
    
    // Initialization
    bool initialize(const std::string& server_name = "rawrxd-gui");
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // Command execution
    bool executeCommand(const std::string& command_line);
    bool executeCommand(const std::string& command, const std::vector<std::string>& args);
    
    // High-level operations
    bool loadModel(const std::string& model_path);
    bool unloadModel();
    bool focusPane(const std::string& pane_name);
    bool showChat();
    bool hideChat();
    bool getStatus();
    bool executeSystemCommand(const std::string& command);
    
    // Batch operations
    bool loadModels(const std::vector<std::string>& model_paths);
    bool focusPanes(const std::vector<std::string>& pane_names);
    
    // Configuration
    void setVerbose(bool verbose);
    void setShowProgress(bool show_progress);
    void setAutoReconnect(bool auto_reconnect);
    void setTimeout(int timeout_ms);
    
    // Status and results
    std::string getLastResponse() const;
    std::string getLastError() const;
    bool getLastSuccess() const;
    
    // Statistics
    size_t getCommandCount() const;
    size_t getSuccessCount() const;
    size_t getErrorCount() const;
    
    // Component access
    CLIIPCClient* getIPCClient() { return ipc_client_.get(); }
    CommandSender* getCommandSender() { return command_sender_.get(); }
    ResponseHandler* getResponseHandler() { return response_handler_.get(); }
    
private:
    // Components
    std::unique_ptr<CLIIPCClient> ipc_client_;
    std::unique_ptr<CommandSender> command_sender_;
    std::unique_ptr<ResponseHandler> response_handler_;
    
    // State
    bool initialized_;
    std::string last_response_;
    std::string last_error_;
    bool last_success_;
    
    // Configuration
    bool verbose_;
    bool show_progress_;
    bool auto_reconnect_;
    int timeout_ms_;
    
    // Initialization
    bool initializeComponents();
    void setupCallbacks();
    void cleanupComponents();
    
    // Command execution
    bool sendCommand(const std::string& command, const std::vector<std::string>& args);
    void handleResponse(const std::string& response, bool success);
    void handleError(const std::string& error);
    
    // Utility methods
    void updateLastResults(const std::string& response, const std::string& error, bool success);
    void showProgress(const std::string& message);
    void showError(const std::string& message);
};

// ============================================================================
// GLOBAL CLI INTEGRATION INSTANCE
// ============================================================================

/**
 * @brief Get the global CLI integration instance
 * @return Pointer to the CLI integration instance
 */
CLICommandIntegration* getCLICommandIntegration();

/**
 * @brief Initialize the global CLI integration
 * @param server_name The GUI server name to connect to
 * @return true if initialization was successful, false otherwise
 */
bool initializeCLICommandIntegration(const std::string& server_name = "rawrxd-gui");

/**
 * @brief Shutdown the global CLI integration
 */
void shutdownCLICommandIntegration();

// ============================================================================
// MAIN CLI ENTRY POINT
// ============================================================================

/**
 * @brief Main entry point for CLI→GUI communication
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int mainCLI(int argc, char* argv[]);

} // namespace CLI
} // namespace RawrXD