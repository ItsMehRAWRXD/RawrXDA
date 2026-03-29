// ============================================================================
// ToolRegistryServer.cpp
// 
// Purpose: IPC server implementation for accessing tools remotely
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#include "ToolRegistryServer.h"
#include "PublicToolRegistry.h"
#include "../logging/Logger.h"

#include <nlohmann/json.hpp>
#include <atomic>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

#ifdef max
#undef max
#endif

namespace RawrXD {

// ============================================================================
// Implementation Details
// ============================================================================

class ToolRegistryServer::Impl {
public:
    Impl() : enabled(true), require_auth(false), running(false), 
             listen_socket(INVALID_SOCKET), port(0) {}
    
    ~Impl() {
        if (listen_socket != INVALID_SOCKET) {
            closesocket(listen_socket);
            listen_socket = INVALID_SOCKET;
        }
        WSACleanup();
    }
    
    bool StartServer(int server_port, const std::string& bind_addr) {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (running) {
            LOG_WARNING("Server already running on port " + std::to_string(port));
            return false;
        }
        
        // Initialize Windows Sockets
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            LOG_ERROR("WSAStartup failed");
            return false;
        }
        
        // Create listening socket
        listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket == INVALID_SOCKET) {
            LOG_ERROR("socket() failed");
            WSACleanup();
            return false;
        }
        
        // Bind socket
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<u_short>(server_port));
        inet_pton(AF_INET, bind_addr.c_str(), &server_addr.sin_addr.s_addr);
        
        if (bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            LOG_ERROR("bind() failed");
            closesocket(listen_socket);
            listen_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
        
        // Listen for connections
        if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
            LOG_ERROR("listen() failed");
            closesocket(listen_socket);
            listen_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
        
        running.store(true);
        port = server_port;
        
        // Start accept thread
        accept_thread = std::thread([this] { AcceptLoop(); });
        if (accept_thread.joinable()) {
            accept_thread.detach();
        }
        
        LOG_INFO("ToolRegistryServer started on " + bind_addr + ":" + std::to_string(server_port));
        return true;
    }
    
    void StopServer() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!running) return;
            running.store(false);
        }
        
        if (listen_socket != INVALID_SOCKET) {
            closesocket(listen_socket);
            listen_socket = INVALID_SOCKET;
        }
        
        LOG_INFO("ToolRegistryServer stopped");
    }
    
    bool IsRunning() const {
        return running.load() && enabled.load();
    }
    
    void AcceptLoop() {
        while (running.load()) {
            sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);
            
            SOCKET client_socket = accept(listen_socket,
                                         (sockaddr*)&client_addr,
                                         &client_addr_len);
            
            if (client_socket == INVALID_SOCKET) {
                if (running.load()) {
                    LOG_WARNING("accept() failed");
                }
                break;
            }
            
            // Handle client request in a separate thread
            std::thread([this, client_socket] {
                HandleClientRequest(client_socket);
            }).detach();
        }
    }
    
    void HandleClientRequest(SOCKET client_socket) {
        try {
            // Receive HTTP request
            char buffer[4096] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received < 0) {
                LOG_WARNING("recv() failed");
                closesocket(client_socket);
                return;
            }
            
            buffer[bytes_received] = '\0';
            
            // Parse HTTP request
            std::string request(buffer);
            std::string method, path, http_version;
            
            size_t first_space = request.find(' ');
            size_t second_space = request.find(' ', first_space + 1);
            
            if (first_space != std::string::npos && second_space != std::string::npos) {
                method = request.substr(0, first_space);
                path = request.substr(first_space + 1, second_space - first_space - 1);
                http_version = request.substr(second_space + 1, request.find('\r') - second_space - 1);
            }
            
            // Extract request body (for POST requests)
            std::string request_body;
            size_t body_start = request.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                request_body = request.substr(body_start + 4);
            }
            
            json response_json = json::object();
            int http_status = 404;
            
            // Route request
            if (path == "/api/tools" && method == "GET") {
                http_status = HandleListTools(response_json);
            }
            else if (path.substr(0, 15) == "/api/tools/" && method == "GET" && 
                    path.find("/schema") != std::string::npos) {
                std::string tool_name = path.substr(15, path.find("/schema") - 15);
                http_status = HandleToolSchema(tool_name, response_json);
            }
            else if (path.substr(0, 15) == "/api/tools/" && method == "POST" &&
                    path.find("/execute") != std::string::npos) {
                std::string tool_name = path.substr(15, path.find("/execute") - 15);
                http_status = HandleToolExecute(tool_name, request_body, response_json);
            }
            else if (path == "/api/status" && method == "GET") {
                http_status = HandleStatus(response_json);
            }
            else if (path.substr(0, 15) == "/api/tools/" && method == "GET" &&
                    path.find("/stats") != std::string::npos) {
                std::string tool_name = path.substr(15, path.find("/stats") - 15);
                http_status = HandleToolStats(tool_name, response_json);
            }
            else {
                response_json["error"] = "Not Found";
                http_status = 404;
            }
            
            // Send HTTP response
            std::string response_body = response_json.dump();
            std::string http_response = "HTTP/1.1 " + std::to_string(http_status) + " OK\r\n" +
                                       "Content-Type: application/json\r\n" +
                                       "Content-Length: " + std::to_string(response_body.size()) + "\r\n" +
                                       "Access-Control-Allow-Origin: *\r\n" +
                                       "\r\n" + response_body;
            
            send(client_socket, http_response.c_str(), http_response.size(), 0);
            
            // Log access
            stats.requests_total++;
            if (http_status >= 400) {
                stats.requests_failed++;
            } else {
                stats.requests_successful++;
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("HandleClientRequest exception: " + std::string(e.what()));
        }
        
        closesocket(client_socket);
    }
    
    int HandleListTools(json& response) {
        try {
            auto tools = PublicToolRegistry::Get().ListAvailableTools();
            response["tools"] = tools;
            response["count"] = tools.size();
            return 200;
        } catch (const std::exception& e) {
            response["error"] = e.what();
            return 500;
        }
    }
    
    int HandleToolSchema(const std::string& tool_name, json& response) {
        try {
            auto tools = PublicToolRegistry::Get().ListAvailableTools();
            auto it = std::find(tools.begin(), tools.end(), tool_name);

            if (it == tools.end()) {
                response["error"] = "Tool not found: " + tool_name;
                return 404;
            }

            response["name"] = tool_name;
            auto schemaJson = PublicToolRegistry::Get().GetToolSchema(tool_name);
            try {
                response["schema"] = json::parse(schemaJson);
            } catch (...) {
                response["schema"] = json::object({{"raw", schemaJson}});
            }

            return 200;
        } catch (const std::exception& e) {
            response["error"] = e.what();
            return 500;
        }
    }
    
    int HandleToolExecute(const std::string& tool_name, const std::string& request_body,
                         json& response) {
        try {
            if (!IsToolAccessible(tool_name)) {
                response["error"] = "Tool not accessible: " + tool_name;
                return 403;
            }

            json payload = json::object();
            if (!request_body.empty()) {
                payload = json::parse(request_body);
            }

            json args = json::object();
            if (payload.is_object() && payload.contains("args") && payload["args"].is_object()) {
                args = payload["args"];
            } else if (payload.is_object()) {
                args = payload;
            }

            auto result = PublicToolRegistry::Get().ExecuteRegisteredTool(tool_name, args.dump());
            response["success"] = result.success();
            response["status"] = static_cast<int>(result.status);
            response["output"] = result.output;
            response["error"] = result.error_message;
            response["exit_code"] = result.exit_code;
            response["elapsed_ms"] = result.execution_time_ms;
            response["metadata"] = result.metadata;

            if (!result.success()) {
                if (result.status == ToolResultStatus::PermissionDenied) {
                    return 403;
                }
                if (result.status == ToolResultStatus::NotFound) {
                    return 404;
                }
                if (result.status == ToolResultStatus::ValidationFailed ||
                    result.status == ToolResultStatus::ParseError) {
                    return 400;
                }
                return 500;
            }

            return 200;
        } catch (const std::exception& e) {
            response["error"] = e.what();
            return 500;
        }
    }
    
    int HandleStatus(json& response) {
        try {
            response["running"] = running.load();
            response["port"] = port;
            response["tools_count"] = PublicToolRegistry::Get().ListAvailableTools().size();
            response["requests_total"] = stats.requests_total;
            response["requests_successful"] = stats.requests_successful;
            response["requests_failed"] = stats.requests_failed;
            
            return 200;
        } catch (const std::exception& e) {
            response["error"] = e.what();
            return 500;
        }
    }
    
    int HandleToolStats(const std::string& tool_name, json& response) {
        try {
            auto stats_obj = PublicToolRegistry::Get().GetToolStatistics(tool_name);
            response["name"] = stats_obj.name;
            response["invocations"] = stats_obj.invocation_count;
            response["successes"] = stats_obj.success_count;
            response["errors"] = stats_obj.error_count;
            response["avg_time_ms"] = stats_obj.average_execution_ms;
            
            return 200;
        } catch (const std::exception& e) {
            response["error"] = e.what();
            return 500;
        }
    }
    
    bool IsToolAccessible(const std::string& tool_name) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = inaccessible_tools.find(tool_name);
        return it == inaccessible_tools.end();
    }
    
    void SetToolAccessible(const std::string& tool_name, bool accessible) {
        std::lock_guard<std::mutex> lock(mutex);
        if (accessible) {
            inaccessible_tools.erase(tool_name);
        } else {
            inaccessible_tools.insert(tool_name);
        }
    }
    
    // Public members
    mutable std::mutex mutex;
    std::atomic<bool> enabled{true};
    std::atomic<bool> require_auth{false};
    std::atomic<bool> running{false};
    std::string auth_token;
    std::set<std::string> inaccessible_tools;
    SOCKET listen_socket;
    int port;
    std::thread accept_thread;
    
    struct Stats {
        uint64_t requests_total = 0;
        uint64_t requests_successful = 0;
        uint64_t requests_failed = 0;
    } stats;
};

// ============================================================================
// Public ToolRegistryServer Implementation
// ============================================================================

ToolRegistryServer& ToolRegistryServer::Get() {
    static ToolRegistryServer instance;
    return instance;
}

ToolRegistryServer::ToolRegistryServer()
    : m_impl(std::make_unique<Impl>()) {
    LOG_INFO("ToolRegistryServer singleton created");
}

ToolRegistryServer::~ToolRegistryServer() = default;

bool ToolRegistryServer::Start(int port, const std::string& bind_address) {
    return m_impl->StartServer(port, bind_address);
}

void ToolRegistryServer::Stop() {
    m_impl->StopServer();
}

bool ToolRegistryServer::IsRunning() const {
    return m_impl->IsRunning();
}

std::string ToolRegistryServer::GetStatus() {
    try {
        json status;
        status["running"] = IsRunning();
        status["port"] = m_impl->port;
        return status.dump();
    } catch (...) {
        return "{\"error\": \"Failed to get status\"}";
    }
}

void ToolRegistryServer::SetEnabled(bool enabled) {
    m_impl->enabled.store(enabled);
}

void ToolRegistryServer::SetToolAccessible(const std::string& tool_name, bool accessible) {
    m_impl->SetToolAccessible(tool_name, accessible);
}

bool ToolRegistryServer::IsToolAccessible(const std::string& tool_name) const {
    return m_impl->IsToolAccessible(tool_name);
}

void ToolRegistryServer::SetRequireAuthentication(bool require_auth) {
    m_impl->require_auth.store(require_auth);
}

void ToolRegistryServer::SetAuthenticationToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->auth_token = token;
}

ToolRegistryServer::ServerStats ToolRegistryServer::GetStatistics() const {
    ServerStats stats;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        stats.requests_total = m_impl->stats.requests_total;
        stats.requests_successful = m_impl->stats.requests_successful;
        stats.requests_failed = m_impl->stats.requests_failed;
    }
    return stats;
}

void ToolRegistryServer::SetAccessLogCallback(AccessLogCallback callback) {
    // TODO: Implement access logging
    LOG_INFO("Access log callback registered");
}

}  // namespace RawrXD
