#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <future>
#include "nlohmann/json.hpp"

namespace RawrXD {

struct LSPConfig {
    std::string languageId;
    std::string command;
    std::vector<std::string> args;
    std::string rootPath;
};

// Abstract transport interface
class JsonRpcTransport {
public:
    virtual ~JsonRpcTransport() = default;
    virtual bool connect(const std::string& cmd, const std::vector<std::string>& args) = 0;
    virtual void send(const nlohmann::json& msg) = 0;
    virtual nlohmann::json receive() = 0;
    virtual bool isConnected() const = 0;
};

class LSPClient {
public:
    LSPClient(const LSPConfig& config);
    ~LSPClient();
    
    bool start();
    void stop();
    
    // Core LSP methods
    std::future<nlohmann::json> initialize();
    void didOpen(const std::string& uri, const std::string& text);
    void didChange(const std::string& uri, const std::string& text);
    std::future<nlohmann::json> completion(const std::string& uri, int line, int character);
    std::future<nlohmann::json> definition(const std::string& uri, int line, int character);
    
private:
    LSPConfig m_config;
    std::unique_ptr<JsonRpcTransport> m_transport;
    std::atomic<bool> m_initialized{false};
    std::atomic<int> m_requestId{0};
    std::mutex m_mutex;
    
    nlohmann::json createRequest(const std::string& method, const nlohmann::json& params);
    nlohmann::json createNotification(const std::string& method, const nlohmann::json& params);
};

} // namespace RawrXD

#pragma once
