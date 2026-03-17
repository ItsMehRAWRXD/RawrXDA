// ============================================================================
// CLI IPC Client - Production Implementation
// ============================================================================
// Connects to GUI IPC server and sends commands
// ============================================================================

#include "cli_ipc_client.hpp"
#include "../gui/command_handlers.hpp"
#include "../gui/command_registry.hpp"
#include "../gui/cli_bridge.hpp"
#include "cli_local_model.hpp"
#include "../core/local_gguf_loader.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace RawrXD {
namespace CLI {

// ============================================================================
// CLI IPC CLIENT IMPLEMENTATION
// ============================================================================

CLIIPCClient::CLIIPCClient()
    : connected_(false)
    , connecting_(false)
    , command_count_(0)
    , success_count_(0)
    , error_count_(0)
    , reconnect_count_(0)
    , reconnect_attempts_(0)
    , last_response_("")
    , last_error_("")
    , last_success_(false)
    , server_name_("")
    , server_path_("")
    , client_id_("")
    , verbose_(false)
    , timeout_ms_(5000)
    , auto_reconnect_(true)
    , max_reconnect_attempts_(3) {
    
    // Generate client ID
    client_id_ = generateClientId();
}

CLIIPCClient::~CLIIPCClient() {
    disconnect();
}

// ============================================================================
// CLIENT LIFECYCLE
// ============================================================================

bool CLIIPCClient::connect(const std::string& server_name) {
    if (connected_.load() || connecting_.load()) {
        if (verbose_) {
            std::cout << "[CLIIPCClient] Already connected or connecting" << std::endl;
        }
        return false;
    }
    
    server_name_ = server_name;
    
    // Determine server path based on platform
#ifdef _WIN32
    server_path_ = "\\\\.\\pipe\\" + server_name;
#else
    server_path_ = "/tmp/" + server_name;
#endif
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Connecting to server: " << server_path_ << std::endl;
    }
    
    connecting_.store(true);
    
    // Create IPC client
    ipc_client_ = std::make_unique<IPCClient>();
    
    // Set up callbacks
    ipc_client_->setMessageCallback([this](const IPCMessage& message) {
        onMessageReceived(message);
    });
    
    ipc_client_->setConnectionCallback([this]() {
        onConnected();
    });
    
    ipc_client_->setDisconnectionCallback([this]() {
        onDisconnected();
    });
    
    ipc_client_->setErrorCallback([this](const std::string& error) {
        onError(error);
    });
    
    // Connect to server
    if (!ipc_client_->connect(server_path_, timeout_ms_)) {
        last_error_ = "Failed to connect to server: " + server_path_;
        connecting_.store(false);
        
        if (verbose_) {
            std::cerr << "[CLIIPCClient] " << last_error_ << std::endl;
        }
        
        return false;
    }
    
    // Wait for connection to be established
    int attempts = 0;
    while (connecting_.load() && attempts < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }
    
    if (!connected_.load()) {
        last_error_ = "Connection timeout";
        connecting_.store(false);
        
        if (verbose_) {
            std::cerr << "[CLIIPCClient] " << last_error_ << std::endl;
        }
        
        return false;
    }
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Connected successfully" << std::endl;
    }
    
    return true;
}

void CLIIPCClient::disconnect() {
    if (!connected_.load() && !connecting_.load()) {
        return;
    }
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Disconnecting..." << std::endl;
    }
    
    connected_.store(false);
    connecting_.store(false);
    
    // Disconnect from server
    if (ipc_client_) {
        ipc_client_->disconnect();
        ipc_client_.reset();
    }
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Disconnected" << std::endl;
    }
}

// ============================================================================
// COMMAND SENDING
// ============================================================================

bool CLIIPCClient::sendCommand(const std::string& command_line) {
    if (!connected_.load()) {
        last_error_ = "Not connected to server";
        
        if (verbose_) {
            std::cerr << "[CLIIPCClient] " << last_error_ << std::endl;
        }
        
        // Attempt reconnection if enabled
        if (auto_reconnect_ && shouldReconnect()) {
            attemptReconnect();
        }
        
        return false;
    }
    
    // Parse command line
    auto [command, args] = parseCommandLine(command_line);
    if (command.empty()) {
        last_error_ = "Empty command";
        return false;
    }
    
    return sendCommand(command, args);
}

bool CLIIPCClient::sendCommand(const std::string& command, const std::vector<std::string>& args) {
    if (!connected_.load()) {
        last_error_ = "Not connected to server";
        
        if (verbose_) {
            std::cerr << "[CLIIPCClient] " << last_error_ << std::endl;
        }
        
        // Attempt reconnection if enabled
        if (auto_reconnect_ && shouldReconnect()) {
            attemptReconnect();
        }
        
        return false;
    }
    
    // Build command message
    std::string command_message = buildCommandMessage(command, args);
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Sending command: " << command_message << std::endl;
    }
    
    // Execute command
    bool success = executeCommand(command_message);
    
    // Update statistics
    command_count_++;
    if (success) {
        success_count_++;
    } else {
        error_count_++;
    }
    
    return success;
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

void CLIIPCClient::onMessageReceived(const IPCMessage& message) {
    if (verbose_) {
        std::cout << "[CLIIPCClient] Message received: type=" << static_cast<int>(message.type)
                  << ", size=" << message.payload.size() << std::endl;
    }
    
    // Process message based on type
    switch (message.type) {
        case IPCMessageType::RESPONSE:
            // Parse response from message payload
            if (message.payload.size() > 0) {
                std::string response(reinterpret_cast<const char*>(message.payload.data()),
                                   message.payload.size());
                handleResponse(response);
            }
            break;
            
        case IPCMessageType::ERROR:
            // Parse error from message payload
            if (message.payload.size() > 0) {
                std::string error(reinterpret_cast<const char*>(message.payload.data()),
                                message.payload.size());
                handleError(error);
            }
            break;
            
        case IPCMessageType::STATUS:
            // Parse status from message payload
            if (message.payload.size() > 0) {
                std::string status(reinterpret_cast<const char*>(message.payload.data()),
                                 message.payload.size());
                handleStatus(status);
            }
            break;
            
        default:
            onError("Unknown message type: " + std::to_string(static_cast<int>(message.type)));
            break;
    }
}

void CLIIPCClient::onConnected() {
    if (verbose_) {
        std::cout << "[CLIIPCClient] Connected to server" << std::endl;
    }
    
    connected_.store(true);
    connecting_.store(false);
    reconnect_attempts_.store(0);
    
    // Notify connection status callback
    if (connection_status_callback_) {
        connection_status_callback_(true);
    }
    
    // Reset statistics on new connection
    resetStatistics();
}

void CLIIPCClient::onDisconnected() {
    if (verbose_) {
        std::cout << "[CLIIPCClient] Disconnected from server" << std::endl;
    }
    
    connected_.store(false);
    connecting_.store(false);
    
    // Notify connection status callback
    if (connection_status_callback_) {
        connection_status_callback_(false);
    }
    
    // Attempt reconnection if enabled
    if (auto_reconnect_ && shouldReconnect()) {
        attemptReconnect();
    }
}

void CLIIPCClient::onError(const std::string& error) {
    last_error_ = error;
    error_count_++;
    
    if (verbose_) {
        std::cerr << "[CLIIPCClient] Error: " << error << std::endl;
    }
    
    // Notify error callback
    if (error_callback_) {
        error_callback_(error);
    }
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool CLIIPCClient::executeCommand(const std::string& command_line) {
    if (!ipc_client_ || !connected_.load()) {
        last_error_ = "Not connected to server";
        return false;
    }
    
    // Create command message
    IPCMessage message;
    message.type = IPCMessageType::COMMAND;
    message.payload.assign(command_line.begin(), command_line.end());
    
    // Send message
    if (!ipc_client_->sendMessage(message)) {
        last_error_ = "Failed to send command";
        return false;
    }
    
    // Notify command sent callback
    if (command_sent_callback_) {
        command_sent_callback_(command_line);
    }
    
    // Wait for response (with timeout)
    int attempts = 0;
    int max_attempts = timeout_ms_ / 100; // 100ms per attempt
    
    while (connected_.load() && attempts < max_attempts) {
        // Check if we have a response
        if (!last_response_.empty() || !last_error_.empty()) {
            last_success_ = last_error_.empty();
            
            // Notify response received callback
            if (response_received_callback_) {
                response_received_callback_(last_response_, last_success_);
            }
            
            return last_success_;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }
    
    last_error_ = "Command timeout";
    return false;
}

void CLIIPCClient::handleResponse(const std::string& response) {
    last_response_ = response;
    last_error_.clear();
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Response: " << response << std::endl;
    }
}

void CLIIPCClient::handleError(const std::string& error) {
    last_error_ = error;
    last_response_.clear();
    
    if (verbose_) {
        std::cerr << "[CLIIPCClient] Error: " << error << std::endl;
    }
}

void CLIIPCClient::handleStatus(const std::string& status) {
    if (verbose_) {
        std::cout << "[CLIIPCClient] Status: " << status << std::endl;
    }
}

// ============================================================================
// RECONNECTION
// ============================================================================

void CLIIPCClient::attemptReconnect() {
    if (!auto_reconnect_ || reconnect_attempts_.load() >= max_reconnect_attempts_) {
        return;
    }
    
    reconnect_attempts_++;
    reconnect_count_++;
    
    if (verbose_) {
        std::cout << "[CLIIPCClient] Attempting reconnection (attempt " 
                  << reconnect_attempts_.load() << "/" << max_reconnect_attempts_ << ")" << std::endl;
    }
    
    // Disconnect first
    disconnect();
    
    // Wait before reconnecting
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Attempt to reconnect
    if (connect(server_name_)) {
        reconnect_attempts_.store(0);
        
        if (verbose_) {
            std::cout << "[CLIIPCClient] Reconnected successfully" << std::endl;
        }
    }
}

bool CLIIPCClient::shouldReconnect() const {
    return auto_reconnect_ && 
           reconnect_attempts_.load() < max_reconnect_attempts_ &&
           !connected_.load();
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

std::string CLIIPCClient::buildCommandMessage(const std::string& command, const std::vector<std::string>& args) {
    std::stringstream ss;
    ss << command;
    
    for (const auto& arg : args) {
        ss << " " << arg;
    }
    
    return ss.str();
}

std::string CLIIPCClient::generateClientId() {
    // Generate a unique client ID based on process ID and timestamp
    std::stringstream ss;
    ss << "cli-" << std::this_thread::get_id() << "-" << std::chrono::system_clock::now().time_since_epoch().count();
    return ss.str();
}

void CLIIPCClient::updateConnectionStatus(bool connected) {
    connected_.store(connected);
    
    // Notify connection status callback
    if (connection_status_callback_) {
        connection_status_callback_(connected);
    }
}

void CLIIPCClient::resetStatistics() {
    command_count_.store(0);
    success_count_.store(0);
    error_count_.store(0);
    last_response_.clear();
    last_error_.clear();
    last_success_ = false;
}

// ============================================================================
// CALLBACK SETTERS
// ============================================================================

void CLIIPCClient::setCommandSentCallback(std::function<void(const std::string&)> callback) {
    command_sent_callback_ = callback;
}

void CLIIPCClient::setResponseReceivedCallback(std::function<void(const std::string&, bool)> callback) {
    response_received_callback_ = callback;
}

void CLIIPCClient::setErrorCallback(std::function<void(const std::string&)> callback) {
    error_callback_ = callback;
}

void CLIIPCClient::setConnectionStatusCallback(std::function<void(bool)> callback) {
    connection_status_callback_ = callback;
}

} // namespace CLI
} // namespace RawrXD