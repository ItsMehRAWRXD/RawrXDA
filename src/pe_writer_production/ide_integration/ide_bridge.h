// ============================================================================
// IDE Bridge - Universal IDE Integration Component
// Provides C++ API, VS Code extension interface, LSP, and REST API
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>

namespace pewriter {

// ============================================================================
// IDE BRIDGE CLASS
// ============================================================================

class IDEBridge {
public:
    IDEBridge();
    ~IDEBridge();

    // Initialization
    bool initialize();
    void shutdown();

    // VS Code Extension Interface
    bool registerVSCodeCommands();
    bool handleVSCodeCommand(const std::string& command,
                           const std::vector<std::string>& args,
                           std::string& result);

    // Language Server Protocol
    bool startLSP();
    void stopLSP();
    bool processLSPMessage(const std::string& message, std::string& response);

    // REST API
    bool startRESTServer(uint16_t port = 8080);
    void stopRESTServer();
    bool processRESTRequest(const std::string& method, const std::string& path,
                          const std::string& body, std::string& response);

    // C++ API Wrappers
    bool createPEFromConfig(const std::string& configJson, std::string& outputPath);
    bool validatePEFile(const std::string& filePath, std::string& validationResult);
    bool getPEInfo(const std::string& filePath, std::string& infoJson);

    // Event callbacks
    using EventCallback = std::function<void(const std::string& event, const std::string& data)>;
    void setEventCallback(EventCallback callback);

    // Thread safety
    void lock();
    void unlock();

private:
    // Internal components
    std::unique_ptr<PEWriter> peWriter_;
    std::unique_ptr<PEValidator> validator_;

    // Server components
    std::thread lspThread_;
    std::thread restThread_;
    bool lspRunning_;
    bool restRunning_;
    uint16_t restPort_;

    // Callbacks
    EventCallback eventCallback_;

    // Thread safety
    std::mutex mutex_;

    // LSP state
    std::unordered_map<std::string, std::string> lspDocuments_;
    int lspRequestId_;

    // REST state
    // (REST server implementation would go here)

    // Helper methods
    bool handleCreatePE(const std::vector<std::string>& args, std::string& result);
    bool handleValidatePE(const std::vector<std::string>& args, std::string& result);
    bool handleGetPEInfo(const std::vector<std::string>& args, std::string& result);

    std::string createLSPResponse(int id, const std::string& result);
    std::string createLSPError(int id, int code, const std::string& message);

    void fireEvent(const std::string& event, const std::string& data);
};

} // namespace pewriter