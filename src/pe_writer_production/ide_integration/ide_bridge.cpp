// ============================================================================
// IDE Bridge Implementation
// Universal IDE integration with VS Code, LSP, and REST API support
// ============================================================================

#include "ide_bridge.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace pewriter {

// ============================================================================
// IDEBridge Implementation
// ============================================================================

IDEBridge::IDEBridge()
    : peWriter_(std::make_unique<PEWriter>()),
      validator_(std::make_unique<PEValidator>()),
      lspRunning_(false),
      restRunning_(false),
      restPort_(8080),
      lspRequestId_(1) {
}

IDEBridge::~IDEBridge() {
    shutdown();
}

bool IDEBridge::initialize() {
    // Initialize PE writer with default config
    PEConfig config;
    return peWriter_->configure(config);
}

void IDEBridge::shutdown() {
    stopLSP();
    stopRESTServer();

    if (lspThread_.joinable()) {
        lspThread_.join();
    }
    if (restThread_.joinable()) {
        restThread_.join();
    }
}

bool IDEBridge::registerVSCodeCommands() {
    // Register VS Code commands
    // This would integrate with VS Code extension API
    fireEvent("vscode:commands:registered", "createPE,validatePE,getPEInfo");
    return true;
}

bool IDEBridge::handleVSCodeCommand(const std::string& command,
                                  const std::vector<std::string>& args,
                                  std::string& result) {
    if (command == "createPE") {
        return handleCreatePE(args, result);
    } else if (command == "validatePE") {
        return handleValidatePE(args, result);
    } else if (command == "getPEInfo") {
        return handleGetPEInfo(args, result);
    }

    result = "Unknown command: " + command;
    return false;
}

bool IDEBridge::startLSP() {
    if (lspRunning_) return true;

    lspRunning_ = true;
    lspThread_ = std::thread([this]() {
        // LSP server loop would go here
        // For now, just keep thread alive
        while (lspRunning_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    fireEvent("lsp:started", "Language server started");
    return true;
}

void IDEBridge::stopLSP() {
    lspRunning_ = false;
    if (lspThread_.joinable()) {
        lspThread_.join();
    }
    fireEvent("lsp:stopped", "Language server stopped");
}

bool IDEBridge::processLSPMessage(const std::string& message, std::string& response) {
    // Parse LSP message (simplified JSON-RPC 2.0)
    // This is a basic implementation

    if (message.find("initialize") != std::string::npos) {
        response = createLSPResponse(lspRequestId_++, R"(
        {
            "capabilities": {
                "textDocumentSync": 1,
                "hoverProvider": true,
                "completionProvider": {
                    "resolveProvider": false,
                    "triggerCharacters": ["."]
                }
            }
        })");
        return true;
    }

    if (message.find("textDocument/hover") != std::string::npos) {
        response = createLSPResponse(lspRequestId_++, R"(
        {
            "contents": {
                "kind": "markdown",
                "value": "# PE Writer\nAdvanced PE file generation for IDEs"
            }
        })");
        return true;
    }

    if (message.find("textDocument/completion") != std::string::npos) {
        response = createLSPResponse(lspRequestId_++, R"(
        {
            "items": [
                {
                    "label": "createPE",
                    "kind": 2,
                    "detail": "Create a PE executable",
                    "documentation": "Generates a PE32+ executable from configuration"
                }
            ]
        })");
        return true;
    }

    response = createLSPError(lspRequestId_++, -32601, "Method not found");
    return false;
}

bool IDEBridge::startRESTServer(uint16_t port) {
    if (restRunning_) return true;

    restPort_ = port;
    restRunning_ = true;

    restThread_ = std::thread([this]() {
        // REST server loop would go here
        // For now, just keep thread alive
        while (restRunning_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    fireEvent("rest:started", "REST server started on port " + std::to_string(port));
    return true;
}

void IDEBridge::stopRESTServer() {
    restRunning_ = false;
    if (restThread_.joinable()) {
        restThread_.join();
    }
    fireEvent("rest:stopped", "REST server stopped");
}

bool IDEBridge::processRESTRequest(const std::string& method, const std::string& path,
                                 const std::string& body, std::string& response) {
    if (method == "POST" && path == "/api/pe/create") {
        std::string outputPath;
        if (createPEFromConfig(body, outputPath)) {
            response = R"({"success": true, "output": ")" + outputPath + R"("})";
            return true;
        } else {
            response = R"({"success": false, "error": "Failed to create PE"})";
            return false;
        }
    }

    if (method == "POST" && path == "/api/pe/validate") {
        std::string validationResult;
        if (validatePEFile(body, validationResult)) {
            response = R"({"success": true, "result": )" + validationResult + "}";
            return true;
        } else {
            response = R"({"success": false, "error": "Validation failed"})";
            return false;
        }
    }

    response = R"({"success": false, "error": "Unknown endpoint"})";
    return false;
}

bool IDEBridge::createPEFromConfig(const std::string& configJson, std::string& outputPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // Parse JSON config
        PEConfig config;
        if (!peWriter_->loadConfigFromJSON(configJson)) {
            return false;
        }

        // Generate unique output filename
        outputPath = "generated_pe_" + std::to_string(std::time(nullptr)) + ".exe";

        // Build and write PE
        if (!peWriter_->build()) {
            return false;
        }

        if (!peWriter_->writeToFile(outputPath)) {
            return false;
        }

        fireEvent("pe:created", outputPath);
        return true;
    } catch (const std::exception& e) {
        fireEvent("pe:error", std::string("Creation failed: ") + e.what());
        return false;
    }
}

bool IDEBridge::validatePEFile(const std::string& filePath, std::string& validationResult) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // Read file
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            validationResult = "Cannot open file";
            return false;
        }

        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

        // Validate
        auto result = validator_->validateImage(data);

        // Format result as JSON
        std::stringstream ss;
        ss << "{";
        ss << "\"valid\": " << (result.isValid ? "true" : "false") << ",";
        ss << "\"message\": \"" << result.message << "\",";

        if (!result.errors.empty()) {
            ss << "\"errors\": [";
            for (size_t i = 0; i < result.errors.size(); ++i) {
                ss << "\"" << result.errors[i] << "\"";
                if (i < result.errors.size() - 1) ss << ",";
            }
            ss << "],";
        }

        if (!result.warnings.empty()) {
            ss << "\"warnings\": [";
            for (size_t i = 0; i < result.warnings.size(); ++i) {
                ss << "\"" << result.warnings[i] << "\"";
                if (i < result.warnings.size() - 1) ss << ",";
            }
            ss << "]";
        }

        ss << "}";
        validationResult = ss.str();

        return result.isValid;
    } catch (const std::exception& e) {
        validationResult = std::string("Validation error: ") + e.what();
        return false;
    }
}

bool IDEBridge::getPEInfo(const std::string& filePath, std::string& infoJson) {
    // Basic PE info extraction
    std::stringstream ss;
    ss << "{";
    ss << "\"file\": \"" << filePath << "\",";
    ss << "\"type\": \"PE32+\",";
    ss << "\"architecture\": \"x64\"";
    ss << "}";
    infoJson = ss.str();
    return true;
}

void IDEBridge::setEventCallback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = callback;
}

void IDEBridge::lock() {
    mutex_.lock();
}

void IDEBridge::unlock() {
    mutex_.unlock();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool IDEBridge::handleCreatePE(const std::vector<std::string>& args, std::string& result) {
    if (args.size() < 1) {
        result = "Usage: createPE <config_file>";
        return false;
    }

    std::ifstream configFile(args[0]);
    if (!configFile) {
        result = "Cannot open config file: " + args[0];
        return false;
    }

    std::stringstream buffer;
    buffer << configFile.rfile();
    std::string configJson = buffer.str();

    std::string outputPath;
    if (createPEFromConfig(configJson, outputPath)) {
        result = "PE created successfully: " + outputPath;
        return true;
    } else {
        result = "Failed to create PE: " + peWriter_->getErrorMessage();
        return false;
    }
}

bool IDEBridge::handleValidatePE(const std::vector<std::string>& args, std::string& result) {
    if (args.size() < 1) {
        result = "Usage: validatePE <pe_file>";
        return false;
    }

    std::string validationResult;
    if (validatePEFile(args[0], validationResult)) {
        result = "PE validation passed: " + validationResult;
        return true;
    } else {
        result = "PE validation failed: " + validationResult;
        return false;
    }
}

bool IDEBridge::handleGetPEInfo(const std::vector<std::string>& args, std::string& result) {
    if (args.size() < 1) {
        result = "Usage: getPEInfo <pe_file>";
        return false;
    }

    std::string infoJson;
    if (getPEInfo(args[0], infoJson)) {
        result = "PE info: " + infoJson;
        return true;
    } else {
        result = "Failed to get PE info";
        return false;
    }
}

std::string IDEBridge::createLSPResponse(int id, const std::string& result) {
    std::stringstream ss;
    ss << "Content-Length: " << (result.length() + 50) << "\r\n\r\n";
    ss << "{\"jsonrpc\": \"2.0\", \"id\": " << id << ", \"result\": " << result << "}";
    return ss.str();
}

std::string IDEBridge::createLSPError(int id, int code, const std::string& message) {
    std::stringstream ss;
    ss << "Content-Length: " << (message.length() + 100) << "\r\n\r\n";
    ss << "{\"jsonrpc\": \"2.0\", \"id\": " << id << ", \"error\": {\"code\": " << code << ", \"message\": \"" << message << "\"}}";
    return ss.str();
}

void IDEBridge::fireEvent(const std::string& event, const std::string& data) {
    if (eventCallback_) {
        eventCallback_(event, data);
    }
}

} // namespace pewriter