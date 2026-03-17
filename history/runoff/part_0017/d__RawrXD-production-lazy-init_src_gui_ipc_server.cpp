// ============================================================================
// GUI IPC Server - Production Implementation
// ============================================================================
// Listens for CLI commands via IPC and processes them through command handlers
// ============================================================================

#include "ipc_server.hpp"
#include "command_handlers.hpp"
#include "command_registry.hpp"
#include "cli_bridge.hpp"
#include "../cli/cli_local_model.hpp"
#include "../core/local_gguf_loader.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

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
// GUI IPC SERVER IMPLEMENTATION
// ============================================================================

GUIIPCServer::GUIIPCServer()
    : running_(false)
    , command_count_(0)
    , success_count_(0)
    , error_count_(0)
    , last_error_("")
    , server_name_("")
    , server_path_("")
    , verbose_(false)
    , auto_focus_(true)
    , main_window_(nullptr) {
    
    // Initialize command handlers
    command_handlers_ = std::make_unique<GUICommandHandlers>(nullptr);
}

GUIIPCServer::~GUIIPCServer() {
    stop();
}

// ============================================================================
// SERVER LIFECYCLE
// ============================================================================

bool GUIIPCServer::start(const std::string& server_name) {
    if (running_.load()) {
        if (verbose_) {
            std::cout << "[GUIIPCServer] Server already running" << std::endl;
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
        std::cout << "[GUIIPCServer] Starting server: " << server_path_ << std::endl;
    }
    
    // Initialize command handlers with main window
    if (main_window_) {
        command_handlers_ = std::make_unique<GUICommandHandlers>(main_window_);
        command_handlers_->setVerbose(verbose_);
        command_handlers_->setAutoFocus(auto_focus_);
    }
    
    // Create IPC server
    ipc_server_ = std::make_unique<IPCServer>();
    
    // Set up callbacks
    ipc_server_->setMessageCallback([this](const IPCMessage& message) {
        onMessageReceived(message);
    });
    
    ipc_server_->setConnectionCallback([this](const IPCConnection& connection) {
        onConnectionAccepted(connection);
    });
    
    ipc_server_->setDisconnectionCallback([this](const IPCConnection& connection) {
        onConnectionClosed(connection);
    });
    
    ipc_server_->setErrorCallback([this](const std::string& error) {
        onError(error);
    });
    
    // Start server
    if (!ipc_server_->start(server_path_)) {
        last_error_ = "Failed to start IPC server: " + server_path_;
        if (verbose_) {
            std::cerr << "[GUIIPCServer] " << last_error_ << std::endl;
        }
        return false;
    }
    
    running_.store(true);
    
    if (verbose_) {
        std::cout << "[GUIIPCServer] Server started successfully" << std::endl;
    }
    
    return true;
}

void GUIIPCServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    if (verbose_) {
        std::cout << "[GUIIPCServer] Stopping server" << std::endl;
    }
    
    running_.store(false);
    
    // Stop IPC server
    if (ipc_server_) {
        ipc_server_->stop();
        ipc_server_.reset();
    }
    
    // Reset command handlers
    if (command_handlers_) {
        command_handlers_.reset();
    }
    
    if (verbose_) {
        std::cout << "[GUIIPCServer] Server stopped" << std::endl;
    }
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void GUIIPCServer::setVerbose(bool verbose) {
    verbose_ = verbose;
    if (command_handlers_) {
        command_handlers_->setVerbose(verbose);
    }
}

void GUIIPCServer::setAutoFocus(bool auto_focus) {
    auto_focus_ = auto_focus;
    if (command_handlers_) {
        command_handlers_->setAutoFocus(auto_focus);
    }
}

void GUIIPCServer::setMainWindow(RawrXDMainWindow* main_window) {
    main_window_ = main_window;
    if (command_handlers_) {
        command_handlers_ = std::make_unique<GUICommandHandlers>(main_window_);
        command_handlers_->setVerbose(verbose_);
        command_handlers_->setAutoFocus(auto_focus_);
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void GUIIPCServer::setCommandReceivedCallback(std::function<void(const std::string&)> callback) {
    command_received_callback_ = callback;
}

void GUIIPCServer::setCommandExecutedCallback(std::function<void(const std::string&, bool)> callback) {
    command_executed_callback_ = callback;
}

void GUIIPCServer::setErrorCallback(std::function<void(const std::string&)> callback) {
    error_callback_ = callback;
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

bool GUIIPCServer::processCommand(const std::string& command_line) {
    if (!running_.load()) {
        last_error_ = "Server is not running";
        return false;
    }
    
    if (!command_handlers_) {
        last_error_ = "Command handlers not initialized";
        return false;
    }
    
    // Parse command line
    auto [command, args] = parseCommandLine(command_line);
    if (command.empty()) {
        last_error_ = "Empty command";
        return false;
    }
    
    return processCommand(command, args);
}

bool GUIIPCServer::processCommand(const std::string& command, const std::vector<std::string>& args) {
    if (!running_.load()) {
        last_error_ = "Server is not running";
        return false;
    }
    
    if (!command_handlers_) {
        last_error_ = "Command handlers not initialized";
        return false;
    }
    
    // Increment command count
    command_count_++;
    
    // Notify command received
    if (command_received_callback_) {
        command_received_callback_(command);
    }
    
    // Execute command
    bool success = command_handlers_->handleCommand(command, args);
    
    // Update statistics
    if (success) {
        success_count_++;
        reportSuccess(command);
    } else {
        error_count_++;
        reportError(command, command_handlers_->getLastError());
    }
    
    // Notify command executed
    if (command_executed_callback_) {
        command_executed_callback_(command, success);
    }
    
    return success;
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

void GUIIPCServer::onMessageReceived(const IPCMessage& message) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Message received: type=" << static_cast<int>(message.type) 
                  << ", size=" << message.payload.size() << std::endl;
    }
    
    // Process message based on type
    switch (message.type) {
        case IPCMessageType::COMMAND:
            // Parse command from message payload
            if (message.payload.size() > 0) {
                std::string command_line(reinterpret_cast<const char*>(message.payload.data()), 
                                       message.payload.size());
                executeCommand(command_line);
            }
            break;
            
        case IPCMessageType::RESPONSE:
            // Handle response (if needed)
            break;
            
        case IPCMessageType::ERROR:
            // Handle error message
            if (message.payload.size() > 0) {
                std::string error(reinterpret_cast<const char*>(message.payload.data()), 
                                message.payload.size());
                onError(error);
            }
            break;
            
        case IPCMessageType::STATUS:
            // Handle status message
            if (message.payload.size() > 0) {
                std::string status(reinterpret_cast<const char*>(message.payload.data()), 
                                 message.payload.size());
                reportStatus(status);
            }
            break;
            
        default:
            onError("Unknown message type: " + std::to_string(static_cast<int>(message.type)));
            break;
    }
}

void GUIIPCServer::onConnectionAccepted(const IPCConnection& connection) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Connection accepted: " << connection.id << std::endl;
    }
    
    // Send welcome message
    std::string welcome = "RawrXD GUI IPC Server v1.0.0";
    sendResponse(connection, welcome);
}

void GUIIPCServer::onConnectionClosed(const IPCConnection& connection) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Connection closed: " << connection.id << std::endl;
    }
}

void GUIIPCServer::onError(const std::string& error) {
    last_error_ = error;
    error_count_++;
    
    if (verbose_) {
        std::cerr << "[GUIIPCServer] Error: " << error << std::endl;
    }
    
    // Notify error callback
    if (error_callback_) {
        error_callback_(error);
    }
    
    // Report error to all connections
    if (ipc_server_) {
        auto connections = ipc_server_->getConnections();
        for (const auto& connection : connections) {
            sendError(*connection, error);
        }
    }
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool GUIIPCServer::executeCommand(const std::string& command_line) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Executing command: " << command_line << std::endl;
    }
    
    return processCommand(command_line);
}

void GUIIPCServer::reportSuccess(const std::string& command) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Command succeeded: " << command << std::endl;
    }
    
    // Build success response
    std::string response = buildResponse(command, true, "Command executed successfully");
    
    // Send to all connections
    if (ipc_server_) {
        auto connections = ipc_server_->getConnections();
        for (const auto& connection : connections) {
            sendResponse(*connection, response);
        }
    }
}

void GUIIPCServer::reportError(const std::string& command, const std::string& error) {
    if (verbose_) {
        std::cerr << "[GUIIPCServer] Command failed: " << command << " - " << error << std::endl;
    }
    
    // Build error response
    std::string response = buildErrorResponse(command, error);
    
    // Send to all connections
    if (ipc_server_) {
        auto connections = ipc_server_->getConnections();
        for (const auto& connection : connections) {
            sendError(*connection, response);
        }
    }
}

void GUIIPCServer::reportStatus(const std::string& status) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Status: " << status << std::endl;
    }
    
    // Build status response
    std::string response = buildStatusResponse(status);
    
    // Send to all connections
    if (ipc_server_) {
        auto connections = ipc_server_->getConnections();
        for (const auto& connection : connections) {
            sendStatus(*connection, response);
        }
    }
}

// ============================================================================
// RESPONSE SENDING
// ============================================================================

bool GUIIPCServer::sendResponse(const IPCConnection& connection, const std::string& response) {
    if (!ipc_server_) {
        return false;
    }
    
    // Create response message
    IPCMessage message;
    message.type = IPCMessageType::RESPONSE;
    message.payload.assign(response.begin(), response.end());
    
    // Send message
    return ipc_server_->sendMessage(connection, message);
}

bool GUIIPCServer::sendError(const IPCConnection& connection, const std::string& error) {
    if (!ipc_server_) {
        return false;
    }
    
    // Create error message
    IPCMessage message;
    message.type = IPCMessageType::ERROR;
    message.payload.assign(error.begin(), error.end());
    
    // Send message
    return ipc_server_->sendMessage(connection, message);
}

bool GUIIPCServer::sendStatus(const IPCConnection& connection, const std::string& status) {
    if (!ipc_server_) {
        return false;
    }
    
    // Create status message
    IPCMessage message;
    message.type = IPCMessageType::STATUS;
    message.payload.assign(status.begin(), status.end());
    
    // Send message
    return ipc_server_->sendMessage(connection, message);
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

std::string GUIIPCServer::buildResponse(const std::string& command, bool success, 
                                       const std::string& message) {
    std::stringstream ss;
    ss << "Command: " << command << " - " << (success ? "SUCCESS" : "FAILED");
    if (!message.empty()) {
        ss << " - " << message;
    }
    return ss.str();
}

std::string GUIIPCServer::buildErrorResponse(const std::string& command, const std::string& error) {
    std::stringstream ss;
    ss << "Command: " << command << " - ERROR - " << error;
    return ss.str();
}

std::string GUIIPCServer::buildStatusResponse(const std::string& status) {
    std::stringstream ss;
    ss << "Status: " << status;
    return ss.str();
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

void GUIIPCServer::handleConnection(std::shared_ptr<IPCConnection> connection) {
    if (verbose_) {
        std::cout << "[GUIIPCServer] Handling connection: " << connection->id << std::endl;
    }
    
    // Process messages from this connection
    processConnectionMessages(connection.get());
}

void GUIIPCServer::processConnectionMessages(IPCConnection* connection) {
    while (running_.load() && connection->isConnected()) {
        // Receive message with timeout
        IPCMessage message;
        if (ipc_server_->receiveMessage(*connection, message, 100)) {
            onMessageReceived(message);
        }
        
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ============================================================================
// GUI APPLICATION IMPLEMENTATION
// ============================================================================

GUIApplication::GUIApplication(int argc, char* argv[])
    : argc_(argc)
    , argv_(argv)
    , running_(false)
    , app_name_("RawrXD GUI")
    , app_version_("1.0.0")
    , verbose_(false)
    , auto_focus_(true)
    , server_name_("rawrxd-gui")
    , main_window_(nullptr) {
    
    // Initialize command registry
    initializeCommandRegistry();
}

GUIApplication::~GUIApplication() {
    cleanup();
}

// ============================================================================
// APPLICATION LIFECYCLE
// ============================================================================

int GUIApplication::run() {
    if (running_.load()) {
        std::cerr << "[GUIApplication] Application already running" << std::endl;
        return -1;
    }
    
    if (verbose_) {
        std::cout << "[GUIApplication] Starting application..." << std::endl;
    }
    
    // Initialize application
    if (!initialize()) {
        std::cerr << "[GUIApplication] Failed to initialize application" << std::endl;
        return -1;
    }
    
    running_.store(true);
    
    if (verbose_) {
        std::cout << "[GUIApplication] Application started successfully" << std::endl;
    }
    
    // Main event loop
    while (running_.load()) {
        processEvents();
        
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (verbose_) {
        std::cout << "[GUIApplication] Application stopped" << std::endl;
    }
    
    return 0;
}

void GUIApplication::exit(int exit_code) {
    if (verbose_) {
        std::cout << "[GUIApplication] Exiting with code: " << exit_code << std::endl;
    }
    
    running_.store(false);
    cleanup();
    
    ::exit(exit_code);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void GUIApplication::setVerbose(bool verbose) {
    verbose_ = verbose;
    if (ipc_server_) {
        ipc_server_->setVerbose(verbose);
    }
}

void GUIApplication::setAutoFocus(bool auto_focus) {
    auto_focus_ = auto_focus;
    if (ipc_server_) {
        ipc_server_->setAutoFocus(auto_focus);
    }
}

void GUIApplication::setServerName(const std::string& server_name) {
    server_name_ = server_name;
}

// ============================================================================
// COMPONENT ACCESS
// ============================================================================

GUICommandHandlers* GUIApplication::getCommandHandlers() {
    if (ipc_server_) {
        return ipc_server_->getCommandHandlers();
    }
    return nullptr;
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

bool GUIApplication::processCommand(const std::string& command_line) {
    if (!ipc_server_) {
        std::cerr << "[GUIApplication] IPC server not initialized" << std::endl;
        return false;
    }
    
    return ipc_server_->processCommand(command_line);
}

bool GUIApplication::processCommand(const std::string& command, const std::vector<std::string>& args) {
    if (!ipc_server_) {
        std::cerr << "[GUIApplication] IPC server not initialized" << std::endl;
        return false;
    }
    
    return ipc_server_->processCommand(command, args);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool GUIApplication::initialize() {
    if (verbose_) {
        std::cout << "[GUIApplication] Initializing..." << std::endl;
    }
    
    // Initialize IPC server
    if (!initializeIPCServer()) {
        std::cerr << "[GUIApplication] Failed to initialize IPC server" << std::endl;
        return false;
    }
    
    // Initialize main window
    if (!initializeMainWindow()) {
        std::cerr << "[GUIApplication] Failed to initialize main window" << std::endl;
        return false;
    }
    
    // Initialize command handlers
    if (!initializeCommandHandlers()) {
        std::cerr << "[GUIApplication] Failed to initialize command handlers" << std::endl;
        return false;
    }
    
    if (verbose_) {
        std::cout << "[GUIApplication] Initialization complete" << std::endl;
    }
    
    return true;
}

bool GUIApplication::initializeIPCServer() {
    if (verbose_) {
        std::cout << "[GUIApplication] Initializing IPC server..." << std::endl;
    }
    
    // Create IPC server
    ipc_server_ = std::make_unique<GUIIPCServer>();
    ipc_server_->setVerbose(verbose_);
    ipc_server_->setAutoFocus(auto_focus_);
    ipc_server_->setMainWindow(main_window_);
    
    // Set up callbacks
    ipc_server_->setCommandReceivedCallback([this](const std::string& command) {
        onCommandReceived(command);
    });
    
    ipc_server_->setCommandExecutedCallback([this](const std::string& command, bool success) {
        onCommandExecuted(command, success);
    });
    
    ipc_server_->setErrorCallback([this](const std::string& error) {
        onError(error);
    });
    
    // Start server
    if (!ipc_server_->start(server_name_)) {
        std::cerr << "[GUIApplication] Failed to start IPC server" << std::endl;
        return false;
    }
    
    if (verbose_) {
        std::cout << "[GUIApplication] IPC server initialized" << std::endl;
    }
    
    return true;
}

bool GUIApplication::initializeMainWindow() {
    if (verbose_) {
        std::cout << "[GUIApplication] Initializing main window..." << std::endl;
    }
    
    // TODO: Initialize actual main window
    // For now, we'll use a placeholder
    main_window_ = nullptr; // Will be set when actual GUI is implemented
    
    if (verbose_) {
        std::cout << "[GUIApplication] Main window initialized" << std::endl;
    }
    
    return true;
}

bool GUIApplication::initializeCommandHandlers() {
    if (verbose_) {
        std::cout << "[GUIApplication] Initializing command handlers..." << std::endl;
    }
    
    // Command handlers are initialized with IPC server
    if (!ipc_server_) {
        std::cerr << "[GUIApplication] IPC server not initialized" << std::endl;
        return false;
    }
    
    if (verbose_) {
        std::cout << "[GUIApplication] Command handlers initialized" << std::endl;
    }
    
    return true;
}

// ============================================================================
// CLEANUP
// ============================================================================

void GUIApplication::cleanup() {
    if (verbose_) {
        std::cout << "[GUIApplication] Cleaning up..." << std::endl;
    }
    
    cleanupIPCServer();
    cleanupMainWindow();
    
    if (verbose_) {
        std::cout << "[GUIApplication] Cleanup complete" << std::endl;
    }
}

void GUIApplication::cleanupIPCServer() {
    if (ipc_server_) {
        if (verbose_) {
            std::cout << "[GUIApplication] Cleaning up IPC server..." << std::endl;
        }
        ipc_server_.reset();
    }
}

void GUIApplication::cleanupMainWindow() {
    if (verbose_) {
        std::cout << "[GUIApplication] Cleaning up main window..." << std::endl;
    }
    // TODO: Cleanup actual main window when implemented
    main_window_ = nullptr;
}

// ============================================================================
// EVENT PROCESSING
// ============================================================================

void GUIApplication::processEvents() {
    // Process pending commands
    processPendingCommands();
    
    // TODO: Process GUI events when actual GUI is implemented
}

void GUIApplication::processPendingCommands() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    for (const auto& command_line : pending_commands_) {
        processCommand(command_line);
    }
    
    pending_commands_.clear();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void GUIApplication::onCommandReceived(const std::string& command) {
    if (verbose_) {
        std::cout << "[GUIApplication] Command received: " << command << std::endl;
    }
}

void GUIApplication::onCommandExecuted(const std::string& command, bool success) {
    if (verbose_) {
        std::cout << "[GUIApplication] Command executed: " << command 
                  << " - " << (success ? "SUCCESS" : "FAILED") << std::endl;
    }
}

void GUIApplication::onError(const std::string& error) {
    std::cerr << "[GUIApplication] Error: " << error << std::endl;
}

// ============================================================================
// GLOBAL APPLICATION INSTANCE
// ============================================================================

static GUIApplication* global_app_instance = nullptr;

GUIApplication* getGUIApplication() {
    return global_app_instance;
}

bool initializeGUIApplication(int argc, char* argv[]) {
    if (global_app_instance) {
        return false; // Already initialized
    }
    
    global_app_instance = new GUIApplication(argc, argv);
    return true;
}

void shutdownGUIApplication() {
    if (global_app_instance) {
        delete global_app_instance;
        global_app_instance = nullptr;
    }
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int mainGUI(int argc, char* argv[]) {
    // Initialize GUI application
    if (!initializeGUIApplication(argc, argv)) {
        std::cerr << "Failed to initialize GUI application" << std::endl;
        return -1;
    }
    
    // Get application instance
    GUIApplication* app = getGUIApplication();
    if (!app) {
        std::cerr << "Failed to get GUI application instance" << std::endl;
        return -1;
    }
    
    // Parse command line arguments
    bool verbose = false;
    bool auto_focus = true;
    std::string server_name = "rawrxd-gui";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--no-focus") {
            auto_focus = false;
        } else if (arg == "--server-name" && i + 1 < argc) {
            server_name = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "RawrXD GUI Application" << std::endl;
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -v, --verbose          Enable verbose output" << std::endl;
            std::cout << "  --no-focus            Disable auto-focus" << std::endl;
            std::cout << "  --server-name <name>  Set server name (default: rawrxd-gui)" << std::endl;
            std::cout << "  -h, --help            Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Configure application
    app->setVerbose(verbose);
    app->setAutoFocus(auto_focus);
    app->setServerName(server_name);
    
    // Run application
    int exit_code = app->run();
    
    // Shutdown
    shutdownGUIApplication();
    
    return exit_code;
}

} // namespace GUI
} // namespace RawrXD