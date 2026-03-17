// ═══════════════════════════════════════════════════════════════════════════════
// MCPServer.hpp - Model Context Protocol Server
// Exposes RawrXD Agent capabilities to external clients (Copilot, Cursor, etc.)
// Pure C++20 - No Qt dependencies
// ═══════════════════════════════════════════════════════════════════════════════

#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <variant>
#include <queue>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD {
namespace MCP {

using String = std::wstring;
template<typename T> using Vector = std::vector<T>;
template<typename T> using Optional = std::optional<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;

// ═══════════════════════════════════════════════════════════════════════════════
// MCP Types (Model Context Protocol specification)
// ═══════════════════════════════════════════════════════════════════════════════

enum class MCPErrorCode {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerNotInitialized = -32002,
    UnknownProtocolVersion = -32001
};

struct MCPTool {
    String name;
    String description;
    String inputSchema;  // JSON schema as string
};

struct MCPResource {
    String uri;
    String name;
    String description;
    String mimeType;
};

struct MCPPrompt {
    String name;
    String description;
    Vector<std::pair<String, String>> arguments; // name, description
};

// ═══════════════════════════════════════════════════════════════════════════════
// Simple JSON utilities (same as LSP)
// ═══════════════════════════════════════════════════════════════════════════════

class JsonBuilder {
public:
    static std::string Object(const std::vector<std::pair<std::string, std::string>>& fields) {
        std::string result = "{";
        for (size_t i = 0; i < fields.size(); i++) {
            if (i > 0) result += ", ";
            result += "\"" + fields[i].first + "\": " + fields[i].second;
        }
        result += "}";
        return result;
    }
    
    static std::string Array(const std::vector<std::string>& items) {
        std::string result = "[";
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0) result += ", ";
            result += items[i];
        }
        result += "]";
        return result;
    }
    
    static std::string String(const std::string& s) {
        std::string result = "\"";
        for (char c : s) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        result += "\"";
        return result;
    }
    
    static std::string Int(int64_t v) { return std::to_string(v); }
    static std::string Bool(bool v) { return v ? "true" : "false"; }
    static std::string Null() { return "null"; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Tool Registry - Stores all available tools
// ═══════════════════════════════════════════════════════════════════════════════

using ToolHandler = std::function<std::string(const std::string& arguments)>;

class ToolRegistry {
public:
    void RegisterTool(const String& name, const String& description, 
                     const String& inputSchema, ToolHandler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        MCPTool tool;
        tool.name = name;
        tool.description = description;
        tool.inputSchema = inputSchema;
        tools_[name] = tool;
        handlers_[name] = handler;
    }
    
    void UnregisterTool(const String& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        tools_.erase(name);
        handlers_.erase(name);
    }
    
    Vector<MCPTool> GetAllTools() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Vector<MCPTool> result;
        for (const auto& [name, tool] : tools_) {
            result.push_back(tool);
        }
        return result;
    }
    
    Optional<MCPTool> GetTool(const String& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::string ExecuteTool(const String& name, const std::string& arguments) {
        ToolHandler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(name);
            if (it == handlers_.end()) {
                return "{\"error\": \"Tool not found\"}";
            }
            handler = it->second;
        }
        return handler(arguments);
    }

private:
    std::map<String, MCPTool> tools_;
    std::map<String, ToolHandler> handlers_;
    mutable std::mutex mutex_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Resource Manager - Manages resources exposed via MCP
// ═══════════════════════════════════════════════════════════════════════════════

using ResourceReader = std::function<std::string(const String& uri)>;

class ResourceManager {
public:
    void RegisterResource(const MCPResource& resource, ResourceReader reader) {
        std::lock_guard<std::mutex> lock(mutex_);
        resources_[resource.uri] = resource;
        readers_[resource.uri] = reader;
    }
    
    void UnregisterResource(const String& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        resources_.erase(uri);
        readers_.erase(uri);
    }
    
    Vector<MCPResource> GetAllResources() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Vector<MCPResource> result;
        for (const auto& [uri, res] : resources_) {
            result.push_back(res);
        }
        return result;
    }
    
    std::string ReadResource(const String& uri) {
        ResourceReader reader;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = readers_.find(uri);
            if (it == readers_.end()) {
                return "";
            }
            reader = it->second;
        }
        return reader(uri);
    }

private:
    std::map<String, MCPResource> resources_;
    std::map<String, ResourceReader> readers_;
    mutable std::mutex mutex_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// MCP Server - Main server implementation
// ═══════════════════════════════════════════════════════════════════════════════

class MCPServer {
public:
    struct Config {
        std::string host{"127.0.0.1"};
        int port{8080};
        String serverName{L"RawrXD Agent"};
        String serverVersion{L"1.0.0"};
        bool authEnabled{false};
        std::string authToken;
    };
    
    using LogCallback = std::function<void(const String& message)>;
    
    explicit MCPServer(const Config& config = {}) : config_(config) {
        toolRegistry_ = std::make_shared<ToolRegistry>();
        resourceManager_ = std::make_shared<ResourceManager>();
    }
    
    ~MCPServer() { Stop(); }
    
    bool Start() {
        if (running_) return true;
        
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            Log(L"WSAStartup failed");
            return false;
        }
        
        // Create socket
        listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket_ == INVALID_SOCKET) {
            Log(L"Socket creation failed");
            WSACleanup();
            return false;
        }
        
        // Allow address reuse
        int opt = 1;
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        // Bind
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(config_.port);
        inet_pton(AF_INET, config_.host.c_str(), &serverAddr.sin_addr);
        
        if (bind(listenSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            Log(L"Bind failed");
            closesocket(listenSocket_);
            WSACleanup();
            return false;
        }
        
        // Listen
        if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
            Log(L"Listen failed");
            closesocket(listenSocket_);
            WSACleanup();
            return false;
        }
        
        running_ = true;
        acceptThread_ = std::thread(&MCPServer::AcceptLoop, this);
        
        Log(L"MCP Server started on port " + std::to_wstring(config_.port));
        return true;
    }
    
    void Stop() {
        if (!running_) return;
        
        running_ = false;
        
        // Close listen socket to unblock accept
        if (listenSocket_ != INVALID_SOCKET) {
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
        }
        
        // Close all client connections
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (auto& client : clients_) {
                if (client.socket != INVALID_SOCKET) {
                    closesocket(client.socket);
                }
                if (client.thread.joinable()) {
                    client.thread.detach();
                }
            }
            clients_.clear();
        }
        
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
        
        WSACleanup();
        Log(L"MCP Server stopped");
    }
    
    bool IsRunning() const { return running_; }
    
    // Accessors
    SharedPtr<ToolRegistry> GetToolRegistry() { return toolRegistry_; }
    SharedPtr<ResourceManager> GetResourceManager() { return resourceManager_; }
    
    // Register built-in tools
    void RegisterBuiltinTools() {
        // File read tool
        toolRegistry_->RegisterTool(
            L"read_file",
            L"Read contents of a file",
            L"{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",
            [](const std::string& args) -> std::string {
                // Parse path from args and read file
                // Simplified - in production would parse JSON properly
                size_t pathStart = args.find("\"path\":\"");
                if (pathStart == std::string::npos) {
                    return "{\"error\": \"Missing path\"}";
                }
                pathStart += 8;
                size_t pathEnd = args.find("\"", pathStart);
                std::string path = args.substr(pathStart, pathEnd - pathStart);
                
                // Read file
                HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile == INVALID_HANDLE_VALUE) {
                    return "{\"error\": \"File not found\"}";
                }
                
                LARGE_INTEGER size;
                GetFileSizeEx(hFile, &size);
                
                std::string content(static_cast<size_t>(size.QuadPart), '\0');
                DWORD bytesRead;
                ReadFile(hFile, content.data(), (DWORD)size.QuadPart, &bytesRead, nullptr);
                CloseHandle(hFile);
                
                return "{\"content\": " + JsonBuilder::String(content) + "}";
            }
        );
        
        // File write tool
        toolRegistry_->RegisterTool(
            L"write_file",
            L"Write content to a file",
            L"{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}",
            [](const std::string& args) -> std::string {
                size_t pathStart = args.find("\"path\":\"");
                if (pathStart == std::string::npos) {
                    return "{\"error\": \"Missing path\"}";
                }
                pathStart += 8;
                size_t pathEnd = args.find("\"", pathStart);
                std::string path = args.substr(pathStart, pathEnd - pathStart);
                
                size_t contentStart = args.find("\"content\":\"");
                if (contentStart == std::string::npos) {
                    return "{\"error\": \"Missing content\"}";
                }
                contentStart += 11;
                size_t contentEnd = args.rfind("\"");
                std::string content = args.substr(contentStart, contentEnd - contentStart);
                
                HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0,
                                          nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile == INVALID_HANDLE_VALUE) {
                    return "{\"error\": \"Cannot create file\"}";
                }
                
                DWORD written;
                WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, nullptr);
                CloseHandle(hFile);
                
                return "{\"success\": true, \"bytes_written\": " + std::to_string(written) + "}";
            }
        );
        
        // Execute command tool
        toolRegistry_->RegisterTool(
            L"execute_command",
            L"Execute a shell command",
            L"{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}",
            [](const std::string& args) -> std::string {
                size_t cmdStart = args.find("\"command\":\"");
                if (cmdStart == std::string::npos) {
                    return "{\"error\": \"Missing command\"}";
                }
                cmdStart += 11;
                size_t cmdEnd = args.rfind("\"");
                std::string command = args.substr(cmdStart, cmdEnd - cmdStart);
                
                // Create pipes
                SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
                HANDLE hReadPipe, hWritePipe;
                CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
                SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
                
                STARTUPINFOA si{sizeof(si)};
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = hWritePipe;
                si.hStdError = hWritePipe;
                
                PROCESS_INFORMATION pi{};
                std::string cmdLine = "cmd /c " + command;
                
                if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(hReadPipe);
                    CloseHandle(hWritePipe);
                    return "{\"error\": \"Failed to execute command\"}";
                }
                
                CloseHandle(hWritePipe);
                
                std::string output;
                char buffer[4096];
                DWORD bytesRead;
                while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }
                
                WaitForSingleObject(pi.hProcess, 30000);
                
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                
                CloseHandle(hReadPipe);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                
                return "{\"exit_code\": " + std::to_string(exitCode) + 
                       ", \"output\": " + JsonBuilder::String(output) + "}";
            }
        );
        
        // List directory tool
        toolRegistry_->RegisterTool(
            L"list_directory",
            L"List contents of a directory",
            L"{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}",
            [](const std::string& args) -> std::string {
                size_t pathStart = args.find("\"path\":\"");
                if (pathStart == std::string::npos) {
                    return "{\"error\": \"Missing path\"}";
                }
                pathStart += 8;
                size_t pathEnd = args.find("\"", pathStart);
                std::string path = args.substr(pathStart, pathEnd - pathStart);
                
                std::string searchPath = path + "\\*";
                WIN32_FIND_DATAA findData;
                HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
                
                if (hFind == INVALID_HANDLE_VALUE) {
                    return "{\"error\": \"Directory not found\"}";
                }
                
                std::vector<std::string> entries;
                do {
                    std::string name = findData.cFileName;
                    if (name != "." && name != "..") {
                        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                        entries.push_back(JsonBuilder::Object({
                            {"name", JsonBuilder::String(name)},
                            {"type", JsonBuilder::String(isDir ? "directory" : "file")}
                        }));
                    }
                } while (FindNextFileA(hFind, &findData));
                
                FindClose(hFind);
                
                return "{\"entries\": " + JsonBuilder::Array(entries) + "}";
            }
        );
    }
    
    void SetLogCallback(LogCallback cb) { logCallback_ = cb; }

private:
    struct ClientConnection {
        SOCKET socket{INVALID_SOCKET};
        std::thread thread;
        bool initialized{false};
    };
    
    Config config_;
    std::atomic<bool> running_{false};
    
    SOCKET listenSocket_{INVALID_SOCKET};
    std::thread acceptThread_;
    
    Vector<ClientConnection> clients_;
    std::mutex clientsMutex_;
    
    SharedPtr<ToolRegistry> toolRegistry_;
    SharedPtr<ResourceManager> resourceManager_;
    
    LogCallback logCallback_;
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Accept Loop
    // ─────────────────────────────────────────────────────────────────────────────
    
    void AcceptLoop() {
        while (running_) {
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(listenSocket_, (sockaddr*)&clientAddr, &addrLen);
            if (clientSocket == INVALID_SOCKET) {
                if (!running_) break;
                continue;
            }
            
            Log(L"Client connected");
            
            std::lock_guard<std::mutex> lock(clientsMutex_);
            ClientConnection conn;
            conn.socket = clientSocket;
            conn.thread = std::thread(&MCPServer::HandleClient, this, clientSocket);
            clients_.push_back(std::move(conn));
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Client Handler
    // ─────────────────────────────────────────────────────────────────────────────
    
    void HandleClient(SOCKET clientSocket) {
        std::string buffer;
        char readBuffer[4096];
        
        while (running_) {
            int bytesReceived = recv(clientSocket, readBuffer, sizeof(readBuffer) - 1, 0);
            if (bytesReceived <= 0) break;
            
            readBuffer[bytesReceived] = '\0';
            buffer += readBuffer;
            
            // Try to parse complete JSON-RPC messages
            // MCP uses newline-delimited JSON
            size_t newlinePos;
            while ((newlinePos = buffer.find('\n')) != std::string::npos) {
                std::string message = buffer.substr(0, newlinePos);
                buffer = buffer.substr(newlinePos + 1);
                
                if (!message.empty()) {
                    std::string response = ProcessMessage(message);
                    response += "\n";
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                }
            }
        }
        
        closesocket(clientSocket);
    }
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Message Processing
    // ─────────────────────────────────────────────────────────────────────────────
    
    std::string ProcessMessage(const std::string& message) {
        // Parse JSON-RPC message
        // Find method
        size_t methodPos = message.find("\"method\":");
        if (methodPos == std::string::npos) {
            return CreateErrorResponse("null", MCPErrorCode::InvalidRequest, "Missing method");
        }
        
        size_t methodStart = message.find("\"", methodPos + 9) + 1;
        size_t methodEnd = message.find("\"", methodStart);
        std::string method = message.substr(methodStart, methodEnd - methodStart);
        
        // Find id
        std::string id = "null";
        size_t idPos = message.find("\"id\":");
        if (idPos != std::string::npos) {
            size_t idStart = idPos + 5;
            while (idStart < message.size() && std::isspace(message[idStart])) idStart++;
            
            if (message[idStart] == '"') {
                idStart++;
                size_t idEnd = message.find("\"", idStart);
                id = "\"" + message.substr(idStart, idEnd - idStart) + "\"";
            } else {
                size_t idEnd = idStart;
                while (idEnd < message.size() && (std::isdigit(message[idEnd]) || message[idEnd] == '-')) idEnd++;
                id = message.substr(idStart, idEnd - idStart);
            }
        }
        
        // Find params
        std::string params = "{}";
        size_t paramsPos = message.find("\"params\":");
        if (paramsPos != std::string::npos) {
            size_t paramsStart = paramsPos + 9;
            while (paramsStart < message.size() && std::isspace(message[paramsStart])) paramsStart++;
            
            // Find matching brace/bracket
            int depth = 0;
            size_t paramsEnd = paramsStart;
            char opener = message[paramsStart];
            char closer = (opener == '{') ? '}' : ']';
            
            for (size_t i = paramsStart; i < message.size(); i++) {
                if (message[i] == opener) depth++;
                else if (message[i] == closer) {
                    depth--;
                    if (depth == 0) {
                        paramsEnd = i + 1;
                        break;
                    }
                }
            }
            
            params = message.substr(paramsStart, paramsEnd - paramsStart);
        }
        
        // Route to handler
        if (method == "initialize") {
            return HandleInitialize(id, params);
        } else if (method == "initialized") {
            return CreateSuccessResponse(id, "null");
        } else if (method == "tools/list") {
            return HandleToolsList(id);
        } else if (method == "tools/call") {
            return HandleToolsCall(id, params);
        } else if (method == "resources/list") {
            return HandleResourcesList(id);
        } else if (method == "resources/read") {
            return HandleResourcesRead(id, params);
        } else if (method == "shutdown") {
            return CreateSuccessResponse(id, "null");
        } else if (method == "exit") {
            return ""; // No response
        }
        
        return CreateErrorResponse(id, MCPErrorCode::MethodNotFound, "Method not found: " + method);
    }
    
    std::string HandleInitialize(const std::string& id, const std::string& params) {
        auto result = JsonBuilder::Object({
            {"protocolVersion", JsonBuilder::String("2024-11-05")},
            {"capabilities", JsonBuilder::Object({
                {"tools", JsonBuilder::Object({{"listChanged", JsonBuilder::Bool(true)}})},
                {"resources", JsonBuilder::Object({{"listChanged", JsonBuilder::Bool(true)}})},
                {"prompts", JsonBuilder::Object({})}
            })},
            {"serverInfo", JsonBuilder::Object({
                {"name", JsonBuilder::String(WideToUtf8(config_.serverName))},
                {"version", JsonBuilder::String(WideToUtf8(config_.serverVersion))}
            })}
        });
        
        return CreateSuccessResponse(id, result);
    }
    
    std::string HandleToolsList(const std::string& id) {
        auto tools = toolRegistry_->GetAllTools();
        
        std::vector<std::string> toolsJson;
        for (const auto& tool : tools) {
            toolsJson.push_back(JsonBuilder::Object({
                {"name", JsonBuilder::String(WideToUtf8(tool.name))},
                {"description", JsonBuilder::String(WideToUtf8(tool.description))},
                {"inputSchema", WideToUtf8(tool.inputSchema)}  // Already JSON
            }));
        }
        
        auto result = JsonBuilder::Object({
            {"tools", JsonBuilder::Array(toolsJson)}
        });
        
        return CreateSuccessResponse(id, result);
    }
    
    std::string HandleToolsCall(const std::string& id, const std::string& params) {
        // Extract tool name
        size_t namePos = params.find("\"name\":");
        if (namePos == std::string::npos) {
            return CreateErrorResponse(id, MCPErrorCode::InvalidParams, "Missing tool name");
        }
        
        size_t nameStart = params.find("\"", namePos + 7) + 1;
        size_t nameEnd = params.find("\"", nameStart);
        String toolName = Utf8ToWide(params.substr(nameStart, nameEnd - nameStart));
        
        // Extract arguments
        std::string arguments = "{}";
        size_t argsPos = params.find("\"arguments\":");
        if (argsPos != std::string::npos) {
            size_t argsStart = params.find("{", argsPos);
            if (argsStart != std::string::npos) {
                int depth = 0;
                size_t argsEnd = argsStart;
                for (size_t i = argsStart; i < params.size(); i++) {
                    if (params[i] == '{') depth++;
                    else if (params[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            argsEnd = i + 1;
                            break;
                        }
                    }
                }
                arguments = params.substr(argsStart, argsEnd - argsStart);
            }
        }
        
        // Execute tool
        std::string toolResult = toolRegistry_->ExecuteTool(toolName, arguments);
        
        auto result = JsonBuilder::Object({
            {"content", JsonBuilder::Array({
                JsonBuilder::Object({
                    {"type", JsonBuilder::String("text")},
                    {"text", JsonBuilder::String(toolResult)}
                })
            })}
        });
        
        return CreateSuccessResponse(id, result);
    }
    
    std::string HandleResourcesList(const std::string& id) {
        auto resources = resourceManager_->GetAllResources();
        
        std::vector<std::string> resourcesJson;
        for (const auto& res : resources) {
            resourcesJson.push_back(JsonBuilder::Object({
                {"uri", JsonBuilder::String(WideToUtf8(res.uri))},
                {"name", JsonBuilder::String(WideToUtf8(res.name))},
                {"description", JsonBuilder::String(WideToUtf8(res.description))},
                {"mimeType", JsonBuilder::String(WideToUtf8(res.mimeType))}
            }));
        }
        
        auto result = JsonBuilder::Object({
            {"resources", JsonBuilder::Array(resourcesJson)}
        });
        
        return CreateSuccessResponse(id, result);
    }
    
    std::string HandleResourcesRead(const std::string& id, const std::string& params) {
        size_t uriPos = params.find("\"uri\":");
        if (uriPos == std::string::npos) {
            return CreateErrorResponse(id, MCPErrorCode::InvalidParams, "Missing uri");
        }
        
        size_t uriStart = params.find("\"", uriPos + 6) + 1;
        size_t uriEnd = params.find("\"", uriStart);
        String uri = Utf8ToWide(params.substr(uriStart, uriEnd - uriStart));
        
        std::string content = resourceManager_->ReadResource(uri);
        
        auto result = JsonBuilder::Object({
            {"contents", JsonBuilder::Array({
                JsonBuilder::Object({
                    {"uri", JsonBuilder::String(WideToUtf8(uri))},
                    {"mimeType", JsonBuilder::String("text/plain")},
                    {"text", JsonBuilder::String(content)}
                })
            })}
        });
        
        return CreateSuccessResponse(id, result);
    }
    
    std::string CreateSuccessResponse(const std::string& id, const std::string& result) {
        return JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"id", id},
            {"result", result}
        });
    }
    
    std::string CreateErrorResponse(const std::string& id, MCPErrorCode code, const std::string& message) {
        return JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"id", id},
            {"error", JsonBuilder::Object({
                {"code", JsonBuilder::Int(static_cast<int>(code))},
                {"message", JsonBuilder::String(message)}
            })}
        });
    }
    
    void Log(const String& message) {
        if (logCallback_) {
            logCallback_(message);
        }
    }
    
    static std::string WideToUtf8(const String& wide) {
        if (wide.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }
    
    static String Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        String result(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), size);
        return result;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// MCP Client - Connect to external MCP servers
// ═══════════════════════════════════════════════════════════════════════════════

class MCPClient {
public:
    struct Config {
        std::string host{"127.0.0.1"};
        int port{8080};
        std::chrono::milliseconds timeout{30000};
    };
    
    explicit MCPClient(const Config& config) : config_(config) {}
    ~MCPClient() { Disconnect(); }
    
    bool Connect() {
        if (connected_) return true;
        
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
        
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }
        
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(config_.port);
        inet_pton(AF_INET, config_.host.c_str(), &serverAddr.sin_addr);
        
        if (connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
        
        connected_ = true;
        
        // Initialize
        auto response = SendRequest("initialize", JsonBuilder::Object({
            {"protocolVersion", JsonBuilder::String("2024-11-05")},
            {"capabilities", JsonBuilder::Object({})},
            {"clientInfo", JsonBuilder::Object({
                {"name", JsonBuilder::String("RawrXD Client")},
                {"version", JsonBuilder::String("1.0.0")}
            })}
        }));
        
        // Send initialized notification
        SendNotification("initialized", "{}");
        
        return true;
    }
    
    void Disconnect() {
        if (!connected_) return;
        
        SendRequest("shutdown", "null");
        SendNotification("exit", "null");
        
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        
        WSACleanup();
        connected_ = false;
    }
    
    bool IsConnected() const { return connected_; }
    
    std::string CallTool(const String& toolName, const std::string& arguments) {
        auto params = JsonBuilder::Object({
            {"name", JsonBuilder::String(WideToUtf8(toolName))},
            {"arguments", arguments}
        });
        
        return SendRequest("tools/call", params);
    }

private:
    Config config_;
    SOCKET socket_{INVALID_SOCKET};
    std::atomic<bool> connected_{false};
    std::atomic<int> nextId_{1};
    
    std::string SendRequest(const std::string& method, const std::string& params) {
        int id = nextId_++;
        
        auto message = JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"id", JsonBuilder::Int(id)},
            {"method", JsonBuilder::String(method)},
            {"params", params}
        });
        message += "\n";
        
        send(socket_, message.c_str(), (int)message.size(), 0);
        
        // Read response
        char buffer[8192];
        int bytesReceived = recv(socket_, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            return "";
        }
        buffer[bytesReceived] = '\0';
        
        return std::string(buffer);
    }
    
    void SendNotification(const std::string& method, const std::string& params) {
        auto message = JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"method", JsonBuilder::String(method)},
            {"params", params}
        });
        message += "\n";
        
        send(socket_, message.c_str(), (int)message.size(), 0);
    }
    
    static std::string WideToUtf8(const String& wide) {
        if (wide.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }
};

} // namespace MCP
} // namespace RawrXD
