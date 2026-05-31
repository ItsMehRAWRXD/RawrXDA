// UEC-X Protocol Layer - JSON-RPC 2.0 Implementation
// Handles communication between IDE adapters and microkernel

#pragma once

#include "../core/microkernel/include/uec_core.h"
#include <nlohmann/json.hpp>
#include <string>

namespace uec {
namespace protocol {

// =============================================================================
// JSON-RPC Types
// =============================================================================

using json = nlohmann::json;

enum class JSONRPCError : int32_t {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerError = -32000,
    
    // UEC-specific errors
    ExtensionNotFound = -32001,
    CommandNotFound = -32002,
    CapabilityDenied = -32003,
    Timeout = -32004,
    SandboxViolation = -32005
};

struct JSONRPCRequest {
    std::string jsonrpc = "2.0";
    std::variant<std::string, int64_t, std::nullptr_t> id;
    std::string method;
    json params;
    
    bool IsNotification() const {
        return std::holds_alternative<std::nullptr_t>(id);
    }
};

struct JSONRPCResponse {
    std::string jsonrpc = "2.0";
    std::variant<std::string, int64_t, std::nullptr_t> id;
    json result;
    json error;
    
    bool IsError() const {
        return !error.is_null();
    }
};

struct JSONRPCErrorObject {
    int32_t code;
    std::string message;
    json data;
};

// =============================================================================
// Protocol Handler
// =============================================================================

class UEC_API ProtocolHandler {
public:
    ProtocolHandler();
    ~ProtocolHandler();

    // Non-copyable, non-movable
    ProtocolHandler(const ProtocolHandler&) = delete;
    ProtocolHandler& operator=(const ProtocolHandler&) = delete;
    ProtocolHandler(ProtocolHandler&&) = delete;
    ProtocolHandler& operator=(ProtocolHandler&&) = delete;

    // Lifecycle
    Result<void> Initialize(Microkernel* kernel);
    Result<void> Shutdown();
    bool IsInitialized() const;

    // Request handling
    Result<std::string> HandleRequest(const std::string& requestJson);
    Result<JSONRPCResponse> HandleRequest(const JSONRPCRequest& request);
    
    // Notification handling (no response)
    Result<void> HandleNotification(const JSONRPCRequest& notification);

    // Response generation
    JSONRPCResponse CreateSuccessResponse(const JSONRPCRequest& request, const json& result);
    JSONRPCResponse CreateErrorResponse(const JSONRPCRequest& request, 
                                        JSONRPCError code, 
                                        const std::string& message,
                                        const json& data = nullptr);

    // Batch processing
    Result<std::string> HandleBatchRequest(const std::string& requestJson);
    Result<std::vector<JSONRPCResponse>> HandleBatchRequest(
        const std::vector<JSONRPCRequest>& requests);

private:
    // Method handlers
    using MethodHandler = std::function<Result<json>(const json& params)>;
    
    Result<json> HandleInitialize(const json& params);
    Result<json> HandleShutdown(const json& params);
    Result<json> HandleRegisterCommand(const json& params);
    Result<json> HandleExecuteCommand(const json& params);
    Result<json> HandleEmitEvent(const json& params);
    Result<json> HandleSubscribeEvent(const json& params);
    Result<json> HandleLoadExtension(const json& params);
    Result<json> HandleUnloadExtension(const json& params);
    Result<json> HandleGetExtensionStatus(const json& params);
    Result<json> HandleKVSet(const json& params);
    Result<json> HandleKVGet(const json& params);
    Result<json> HandleKVDelete(const json& params);
    Result<json> HandleGetStats(const json& params);
    Result<json> HandleGetHealth(const json& params);
    Result<json> HandlePing(const json& params);

    // Utility
    Result<JSONRPCRequest> ParseRequest(const std::string& jsonStr);
    std::string SerializeResponse(const JSONRPCResponse& response);
    std::string SerializeBatchResponse(const std::vector<JSONRPCResponse>& responses);
    
    ErrorCode JSONRPCToUECError(JSONRPCError code) const;
    JSONRPCError UECToJSONRPCError(ErrorCode code) const;

    Microkernel* m_kernel = nullptr;
    std::unordered_map<std::string, MethodHandler> m_handlers;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shuttingDown{false};
};

// =============================================================================
// Transport Interface
// =============================================================================

class UEC_API Transport {
public:
    virtual ~Transport() = default;
    
    virtual Result<void> Initialize() = 0;
    virtual Result<void> Shutdown() = 0;
    virtual bool IsConnected() const = 0;
    
    virtual Result<std::string> Receive() = 0;
    virtual Result<void> Send(const std::string& message) = 0;
    
    virtual void SetMessageHandler(std::function<void(const std::string&)> handler) = 0;
};

// =============================================================================
// Named Pipe Transport (Windows)
// =============================================================================

#ifdef UEC_PLATFORM_WINDOWS

class UEC_API NamedPipeTransport : public Transport {
public:
    NamedPipeTransport(const std::string& pipeName);
    ~NamedPipeTransport() override;

    Result<void> Initialize() override;
    Result<void> Shutdown() override;
    bool IsConnected() const override;
    
    Result<std::string> Receive() override;
    Result<void> Send(const std::string& message) override;
    
    void SetMessageHandler(std::function<void(const std::string&)> handler) override;

private:
    void ReceiveLoop();
    
    std::string m_pipeName;
    HANDLE m_pipeHandle = INVALID_HANDLE_VALUE;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    std::thread m_receiveThread;
    std::function<void(const std::string&)> m_messageHandler;
    mutable std::mutex m_mutex;
};

#endif

// =============================================================================
// Protocol Server
// =============================================================================

class UEC_API ProtocolServer {
public:
    ProtocolServer();
    ~ProtocolServer();

    // Non-copyable, non-movable
    ProtocolServer(const ProtocolServer&) = delete;
    ProtocolServer& operator=(const ProtocolServer&) = delete;
    ProtocolServer(ProtocolServer&&) = delete;
    ProtocolServer& operator=(ProtocolServer&&) = delete;

    // Lifecycle
    Result<void> Initialize(Microkernel* kernel, std::unique_ptr<Transport> transport);
    Result<void> Shutdown();
    bool IsRunning() const;

    // Server control
    Result<void> Start();
    Result<void> Stop();

    // Connection management
    size_t GetConnectionCount() const;
    void CloseAllConnections();

private:
    void ProcessMessage(const std::string& message);
    void SendResponse(const std::string& response);

    std::unique_ptr<ProtocolHandler> m_handler;
    std::unique_ptr<Transport> m_transport;
    std::atomic<bool> m_running{false};
    std::atomic<size_t> m_connectionCount{0};
};

} // namespace protocol
} // namespace uec
