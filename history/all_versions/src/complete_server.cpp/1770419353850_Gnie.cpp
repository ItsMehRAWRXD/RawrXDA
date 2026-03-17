#include "complete_server.h"
#include "agentic_engine.h"
#include "subagent_core.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace RawrXD {

namespace {
#ifdef _WIN32
using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
inline int CloseSocket(SocketType s) { return closesocket(s); }
#else
using SocketType = int;
constexpr SocketType kInvalidSocket = -1;
inline int CloseSocket(SocketType s) { return close(s); }
#endif

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

bool ExtractJsonString(const std::string& body, const std::string& key, std::string& out) {
    const std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    if (pos >= body.size() || body[pos] != '"') return false;
    ++pos;
    std::string result;
    while (pos < body.size()) {
        char c = body[pos++];
        if (c == '\\' && pos < body.size()) {
            char esc = body[pos++];
            switch (esc) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default: result.push_back(esc); break;
            }
            continue;
        }
        if (c == '"') break;
        result.push_back(c);
    }
    out = result;
    return true;
}

bool ExtractJsonNumber(const std::string& body, const std::string& key, std::string& out) {
    const std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    size_t start = pos;
    while (pos < body.size() && (std::isdigit(static_cast<unsigned char>(body[pos])) || body[pos] == '-' || body[pos] == '.')) {
        ++pos;
    }
    if (start == pos) return false;
    out = body.substr(start, pos - start);
    return true;
}

std::string EscapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

bool ParseRequest(const std::string& data, std::string& method, std::string& path, std::string& body) {
    auto header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;
    std::string header = data.substr(0, header_end);
    body = data.substr(header_end + 4);

    std::istringstream header_stream(header);
    std::string request_line;
    if (!std::getline(header_stream, request_line)) return false;
    if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();

    std::istringstream line_stream(request_line);
    line_stream >> method >> path;
    return !method.empty() && !path.empty();
}

std::string BuildResponse(int status, const std::string& body, const std::vector<std::string>& headers) {
    std::ostringstream oss;
    if (status == 200) {
        oss << "HTTP/1.1 200 OK\r\n";
    } else if (status == 204) {
        oss << "HTTP/1.1 204 No Content\r\n";
    } else if (status == 400) {
        oss << "HTTP/1.1 400 Bad Request\r\n";
    } else {
        oss << "HTTP/1.1 500 Internal Server Error\r\n";
    }

    for (const auto& header : headers) {
        oss << header << "\r\n";
    }

    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

} // namespace

CompletionServer::CompletionServer() : running_(false), engine_(nullptr) {}

CompletionServer::~CompletionServer() {
    Stop();
}

bool CompletionServer::Start(uint16_t port, CPUInferenceEngine* engine, std::string model_path) {
    if (running_) return false;
    engine_ = engine;
    model_path_ = std::move(model_path);
    running_ = true;
    server_thread_ = std::thread(&CompletionServer::Run, this, port);
    return true;
}

void CompletionServer::Stop() {
    if (!running_) return;
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void CompletionServer::Run(uint16_t port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[CompletionServer] WSAStartup failed." << std::endl;
        running_ = false;
        return;
    }
#endif

    SocketType server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == kInvalidSocket) {
        std::cerr << "[CompletionServer] Failed to create socket." << std::endl;
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "[CompletionServer] Failed to bind port " << port << "." << std::endl;
        CloseSocket(server_fd);
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    if (listen(server_fd, 8) != 0) {
        std::cerr << "[CompletionServer] Failed to listen." << std::endl;
        CloseSocket(server_fd);
        running_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    std::cout << "[CompletionServer] Listening on port " << port << "..." << std::endl;

    while (running_) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif
        SocketType client = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client == kInvalidSocket) {
            continue;
        }
        std::thread(&CompletionServer::HandleClient, this, static_cast<int>(client)).detach();
    }

    CloseSocket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void CompletionServer::HandleClient(int client_fd) {
    SocketType client = static_cast<SocketType>(client_fd);
    std::string data;
    char buffer[4096];
    int received = 0;
    while ((received = recv(client, buffer, sizeof(buffer), 0)) > 0) {
        data.append(buffer, buffer + received);
        if (data.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        CloseSocket(client);
        return;
    }

    std::string headers = data.substr(0, header_end + 4);
    std::string body = data.substr(header_end + 4);

    size_t content_length = 0;
    auto cl_pos = ToLower(headers).find("content-length:");
    if (cl_pos != std::string::npos) {
        size_t line_end = headers.find("\r\n", cl_pos);
        std::string value = headers.substr(cl_pos + 15, line_end - (cl_pos + 15));
        content_length = static_cast<size_t>(std::stoul(Trim(value)));
    }

    while (body.size() < content_length && (received = recv(client, buffer, sizeof(buffer), 0)) > 0) {
        body.append(buffer, buffer + received);
    }

    std::string method;
    std::string path;
    std::string parsed_body;
    ParseRequest(data, method, path, parsed_body);
    parsed_body = body;

    std::vector<std::string> response_headers = {
        "Content-Type: application/json",
        "Access-Control-Allow-Origin: *",
        "Access-Control-Allow-Methods: GET, POST, OPTIONS",
        "Access-Control-Allow-Headers: Content-Type"
    };

    std::string response_body;
    int status = 200;

    if (method == "OPTIONS") {
        status = 204;
        response_body.clear();
    } else if (method == "GET" && path == "/status") {
        bool model_loaded = engine_ && engine_->IsModelLoaded();
        std::string escaped_path = EscapeJson(model_path_);
        response_body = std::string("{\"ready\":") + (running_ ? "true" : "false") +
                        ",\"model_loaded\":" + (model_loaded ? "true" : "false") +
                        ",\"model_path\":\"" + escaped_path + "\"" +
                        ",\"backend\":\"rawrxd\"" +
                        ",\"agentic\":" + (agentic_engine_ ? "true" : "false") +
                        ",\"subagents\":" + (subagent_mgr_ ? "true" : "false") +
                        ",\"capabilities\":{\"completion\":true,\"streaming\":true"
                        ",\"chat\":true,\"subagent\":true,\"chain\":true,\"swarm\":true}}";
    } else if (method == "POST" && path == "/complete") {
        response_body = HandleCompleteRequest(parsed_body);
    } else if (method == "POST" && path == "/complete/stream") {
        HandleCompleteStreamRequest(static_cast<int>(client), parsed_body);
        CloseSocket(client);
        return;
    }
    // === Agentic API Routes ===
    else if (method == "POST" && path == "/api/chat") {
        response_body = HandleChatRequest(parsed_body);
    } else if (method == "POST" && path == "/api/subagent") {
        response_body = HandleSubAgentRequest(parsed_body);
    } else if (method == "POST" && path == "/api/chain") {
        response_body = HandleChainRequest(parsed_body);
    } else if (method == "POST" && path == "/api/swarm") {
        response_body = HandleSwarmRequest(parsed_body);
    } else if (method == "GET" && path == "/api/agents") {
        response_body = HandleAgentsListRequest();
    } else if (method == "GET" && path == "/api/agents/status") {
        response_body = HandleAgentsStatusRequest();
    } else {
        status = 400;
        response_body = R"({"error":"unknown_endpoint"})";
    }

    std::string response = BuildResponse(status, response_body, response_headers);
    send(client, response.c_str(), static_cast<int>(response.size()), 0);
    CloseSocket(client);
}

std::string CompletionServer::HandleCompleteRequest(const std::string& body) {
    std::string buffer;
    std::string cursor_offset_raw;
    std::string max_tokens_raw;

    int max_tokens = 128;
    size_t cursor_offset = 0;

    ExtractJsonString(body, "buffer", buffer);
    if (ExtractJsonNumber(body, "cursor_offset", cursor_offset_raw)) {
        cursor_offset = static_cast<size_t>(std::stoull(cursor_offset_raw));
    }
    if (ExtractJsonNumber(body, "max_tokens", max_tokens_raw)) {
        max_tokens = std::max(0, std::stoi(max_tokens_raw));
    }

    if (!engine_ || !engine_->IsModelLoaded()) {
        return R"({"completion":""})";
    }

    if (cursor_offset > buffer.size()) {
        cursor_offset = buffer.size();
    }

    std::string prompt = buffer.substr(0, cursor_offset);
    auto tokens = engine_->Tokenize(prompt);
    auto generated = engine_->Generate(tokens, max_tokens);
    std::string completion = engine_->Detokenize(generated);

    std::string escaped = EscapeJson(completion);
    return std::string("{\"completion\":\"") + escaped + "\"}";
}

void CompletionServer::HandleCompleteStreamRequest(int client_fd, const std::string& body) {
    SocketType client = static_cast<SocketType>(client_fd);
    
    std::string buffer;
    std::string cursor_offset_raw;
    std::string max_tokens_raw;
    std::string language;
    std::string mode;  // "complete", "refactor", "docs"

    int max_tokens = 128;
    size_t cursor_offset = 0;

    ExtractJsonString(body, "buffer", buffer);
    ExtractJsonString(body, "language", language);
    ExtractJsonString(body, "mode", mode);
    if (mode.empty()) mode = "complete";
    
    if (ExtractJsonNumber(body, "cursor_offset", cursor_offset_raw)) {
        cursor_offset = static_cast<size_t>(std::stoull(cursor_offset_raw));
    }
    if (ExtractJsonNumber(body, "max_tokens", max_tokens_raw)) {
        max_tokens = std::max(0, std::stoi(max_tokens_raw));
    }

    // SSE headers
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: text/event-stream\r\n";
    oss << "Cache-Control: no-cache\r\n";
    oss << "Connection: close\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type\r\n";
    oss << "\r\n";

    std::string headers = oss.str();
    int send_result = send(client, headers.c_str(), static_cast<int>(headers.size()), 0);
    if (send_result < 0) {
        return;  // Client disconnected before we could send headers
    }

    // Check engine state
    if (!engine_ || !engine_->IsModelLoaded()) {
        std::string event = "event: error\r\ndata: {\"error\":\"model_not_loaded\"}\r\n\r\n";
        send(client, event.c_str(), static_cast<int>(event.size()), 0);
        return;
    }

    if (cursor_offset > buffer.size()) {
        cursor_offset = buffer.size();
    }

    // Slice context window: last 2-4k chars before cursor (or less if file is smaller)
    const size_t context_window = 4096;
    size_t context_start = cursor_offset > context_window ? cursor_offset - context_window : 0;
    std::string context = buffer.substr(context_start, cursor_offset - context_start);

    // Build prompt based on mode
    std::string system_prompt;
    std::string user_prompt;

    if (mode == "refactor") {
        system_prompt = "You are an expert software engineer.\n"
                       "You refactor code to be cleaner, safer, and more idiomatic.\n"
                       "Do not explain.\n"
                       "Do not add comments unless they already exist.\n"
                       "Only output the modified code.";
        user_prompt = std::string("Language: ") + language + "\n\n"
                     "Context:\n" + context + "\n\n"
                     "Task:\n"
                     "Refactor the code to improve clarity and correctness.\n"
                     "Preserve behavior.\n"
                     "Preserve style and formatting.\n"
                     "Do not introduce new abstractions unless necessary.";
    } else if (mode == "docs") {
        system_prompt = "You are an expert software engineer.\n"
                       "You write concise, accurate documentation comments.\n"
                       "Do not change code behavior.\n"
                       "Do not explain outside of comments.";
        user_prompt = std::string("Language: ") + language + "\n\n"
                     "Context:\n" + context + "\n\n"
                     "Task:\n"
                     "Add concise documentation comments explaining intent, parameters, and behavior.\n"
                     "Use the language's standard doc comment style.\n"
                     "Do not modify the code.";
    } else {
        // Default: completion
        system_prompt = "You are an expert software engineer.\n"
                       "You complete code accurately, concisely, and idiomatically.\n"
                       "Do not explain.\n"
                       "Do not repeat the prompt.\n"
                       "Only output the completion.";
        user_prompt = std::string("Language: ") + language + "\n\n"
                     "Context:\n" + context + "\n\n"
                     "Task:\n"
                     "Continue the code at the cursor.\n"
                     "Preserve style, indentation, and conventions.\n"
                     "Do not add unrelated code.";
    }

    // Prepare full prompt for inference
    std::string full_prompt = system_prompt + "\n\n" + user_prompt;
    auto tokens = engine_->Tokenize(full_prompt);
    auto generated = engine_->Generate(tokens, max_tokens);

    // Stream each token
    for (const auto& token : generated) {
        std::string text = engine_->Detokenize({token});
        std::string escaped = EscapeJson(text);
        
        std::ostringstream event;
        event << "event: token\r\n";
        event << "data: {\"token\":\"" << escaped << "\"}\r\n";
        event << "\r\n";
        
        std::string event_str = event.str();
        send_result = send(client, event_str.c_str(), static_cast<int>(event_str.size()), 0);
        
        // If send fails, client disconnected—break inference loop immediately
        if (send_result < 0) {
            return;
        }
    }

    // Send end event
    std::string end_event = "event: end\r\ndata: {}\r\n\r\n";
    send(client, end_event.c_str(), static_cast<int>(end_event.size()), 0);
}


} // namespace RawrXD
