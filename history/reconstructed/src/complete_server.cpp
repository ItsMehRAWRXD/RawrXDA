#include "complete_server.h"
#include "agentic_engine.h"
#include "subagent_core.h"
#include "agent_history.h"
#include "agent_policy.h"
#include "agent_explainability.h"
#include "ai_backend.h"

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
                        ",\"backend\":\"" + (backend_mgr_ ? backend_mgr_->getActiveBackendName() : "rawrxd") + "\"" +
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
    } else if ((method == "GET" || method == "POST") &&
               (path == "/api/agents/history" || path.rfind("/api/agents/history?", 0) == 0)) {
        response_body = HandleHistoryRequest(path, parsed_body);
    } else if (method == "POST" && path == "/api/agents/replay") {
        response_body = HandleReplayRequest(parsed_body);
    }
    // === Phase 7: Policy API Routes ===
    else if ((method == "GET" || method == "POST") &&
             (path == "/api/policies" || path.rfind("/api/policies?", 0) == 0)) {
        response_body = HandlePoliciesRequest(path, parsed_body);
    } else if (method == "GET" && path == "/api/policies/suggestions") {
        response_body = HandlePolicySuggestionsRequest(parsed_body);
    } else if (method == "POST" && path == "/api/policies/apply") {
        response_body = HandlePolicyApplyRequest(parsed_body);
    } else if (method == "POST" && path == "/api/policies/reject") {
        response_body = HandlePolicyRejectRequest(parsed_body);
    } else if (method == "GET" && path == "/api/policies/export") {
        response_body = HandlePolicyExportRequest();
    } else if (method == "POST" && path == "/api/policies/import") {
        response_body = HandlePolicyImportRequest(parsed_body);
    } else if (method == "GET" && path == "/api/policies/heuristics") {
        response_body = HandlePolicyHeuristicsRequest();
    } else if (method == "GET" && path == "/api/policies/stats") {
        response_body = HandlePolicyStatsRequest();
    }
    // === Phase 8A: Explainability API Routes ===
    else if (method == "GET" &&
             (path == "/api/agents/explain" || path.rfind("/api/agents/explain?", 0) == 0)) {
        response_body = HandleExplainRequest(path, parsed_body);
    } else if (method == "GET" && path == "/api/agents/explain/stats") {
        response_body = HandleExplainStatsRequest();
    }
    // === Phase 8B: Backend Switcher API Routes ===
    else if (method == "GET" && path == "/api/backends") {
        response_body = HandleBackendsListRequest();
    } else if (method == "GET" && path == "/api/backends/status") {
        response_body = HandleBackendsStatusRequest();
    } else if (method == "POST" && path == "/api/backends/use") {
        response_body = HandleBackendsUseRequest(parsed_body);
    }
    // === Cursor IDE Integration Routes ===
    else if (method == "POST" && path == "/cursor/inline-edit") {
        response_body = HandleCursorInlineEditRequest(parsed_body);
    } else if (method == "POST" && path == "/cursor/diff-preview") {
        response_body = HandleCursorDiffPreviewRequest(parsed_body);
    } else if (method == "POST" && path == "/cursor/ghost-text") {
        response_body = HandleCursorGhostTextRequest(parsed_body);
    } else if (method == "POST" && path == "/cursor/context") {
        response_body = HandleCursorContextRequest(parsed_body);
    } else if (method == "POST" && path == "/cursor/keystroke") {
        response_body = HandleCursorKeystrokeRequest(parsed_body);
    } else if (method == "POST" && path == "/cursor/multi-file") {
        response_body = HandleCursorMultiFileRequest(parsed_body);
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

// ============================================================================
// Agentic API Handlers
// ============================================================================

std::string CompletionServer::HandleChatRequest(const std::string& body) {
    if (!agentic_engine_) {
        return R"({"error":"agentic_engine_not_available"})";
    }

    std::string message;
    ExtractJsonString(body, "message", message);
    if (message.empty()) ExtractJsonString(body, "prompt", message);
    if (message.empty()) {
        return R"({"error":"missing_message"})";
    }

    std::string response = agentic_engine_->chat(message);

    // Auto-dispatch tool calls if subagent manager is available
    std::string toolResult;
    bool hadToolCall = false;
    if (subagent_mgr_) {
        hadToolCall = subagent_mgr_->dispatchToolCall("api", response, toolResult);
    }

    std::string escaped = EscapeJson(response);
    std::string result = "{\"response\":\"" + escaped + "\"";
    if (hadToolCall) {
        result += ",\"tool_result\":\"" + EscapeJson(toolResult) + "\"";
    }
    result += "}";
    return result;
}

std::string CompletionServer::HandleSubAgentRequest(const std::string& body) {
    if (!subagent_mgr_) {
        return R"({"error":"subagent_manager_not_available"})";
    }

    std::string description, prompt;
    ExtractJsonString(body, "description", description);
    ExtractJsonString(body, "prompt", prompt);
    if (prompt.empty()) {
        return R"({"error":"missing_prompt"})";
    }
    if (description.empty()) description = "API subagent";

    std::string timeoutStr;
    int timeout = 120000;
    if (ExtractJsonNumber(body, "timeout", timeoutStr)) {
        timeout = std::max(1000, std::stoi(timeoutStr));
    }

    std::string agentId = subagent_mgr_->spawnSubAgent("api", description, prompt);
    bool success = subagent_mgr_->waitForSubAgent(agentId, timeout);
    std::string result = subagent_mgr_->getSubAgentResult(agentId);

    return "{\"agent_id\":\"" + agentId + "\""
           ",\"success\":" + (success ? "true" : "false") +
           ",\"result\":\"" + EscapeJson(result) + "\"}";
}

std::string CompletionServer::HandleChainRequest(const std::string& body) {
    if (!subagent_mgr_) {
        return R"({"error":"subagent_manager_not_available"})";
    }

    // Parse steps array
    std::vector<std::string> steps;
    size_t stepsPos = body.find("\"steps\"");
    if (stepsPos == std::string::npos) {
        return R"({"error":"missing_steps_array"})";
    }

    size_t arrStart = body.find('[', stepsPos);
    if (arrStart == std::string::npos) {
        return R"({"error":"invalid_steps_format"})";
    }

    int depth = 0;
    size_t arrEnd = arrStart;
    for (size_t i = arrStart; i < body.size(); i++) {
        if (body[i] == '[') depth++;
        else if (body[i] == ']') { depth--; if (depth == 0) { arrEnd = i; break; } }
    }

    std::string arr = body.substr(arrStart + 1, arrEnd - arrStart - 1);
    size_t strStart = 0;
    while ((strStart = arr.find('"', strStart)) != std::string::npos) {
        strStart++;
        std::string value;
        for (size_t i = strStart; i < arr.size(); i++) {
            if (arr[i] == '\\' && i + 1 < arr.size()) { value += arr[++i]; }
            else if (arr[i] == '"') { strStart = i + 1; break; }
            else value += arr[i];
        }
        if (!value.empty()) steps.push_back(value);
    }

    if (steps.empty()) {
        return R"({"error":"empty_steps"})";
    }

    std::string initialInput;
    ExtractJsonString(body, "input", initialInput);

    std::string result = subagent_mgr_->executeChain("api", steps, initialInput);

    // Get step details
    auto chainSteps = subagent_mgr_->getChainSteps();
    std::string stepsJson = "[";
    for (size_t i = 0; i < chainSteps.size(); i++) {
        if (i > 0) stepsJson += ",";
        stepsJson += "{\"index\":" + std::to_string(chainSteps[i].index)
                   + ",\"state\":\"" + (chainSteps[i].state == SubAgent::State::Completed ? "completed" : "failed")
                   + "\",\"result\":\"" + EscapeJson(chainSteps[i].result) + "\"}";
    }
    stepsJson += "]";

    return "{\"result\":\"" + EscapeJson(result) + "\""
           ",\"steps\":" + stepsJson +
           ",\"step_count\":" + std::to_string(steps.size()) + "}";
}

std::string CompletionServer::HandleSwarmRequest(const std::string& body) {
    if (!subagent_mgr_) {
        return R"({"error":"subagent_manager_not_available"})";
    }

    // Parse prompts array
    std::vector<std::string> prompts;
    size_t promptsPos = body.find("\"prompts\"");
    if (promptsPos == std::string::npos) {
        return R"({"error":"missing_prompts_array"})";
    }

    size_t arrStart = body.find('[', promptsPos);
    if (arrStart == std::string::npos) {
        return R"({"error":"invalid_prompts_format"})";
    }

    int depth = 0;
    size_t arrEnd = arrStart;
    for (size_t i = arrStart; i < body.size(); i++) {
        if (body[i] == '[') depth++;
        else if (body[i] == ']') { depth--; if (depth == 0) { arrEnd = i; break; } }
    }

    std::string arr = body.substr(arrStart + 1, arrEnd - arrStart - 1);
    size_t strStart = 0;
    while ((strStart = arr.find('"', strStart)) != std::string::npos) {
        strStart++;
        std::string value;
        for (size_t i = strStart; i < arr.size(); i++) {
            if (arr[i] == '\\' && i + 1 < arr.size()) { value += arr[++i]; }
            else if (arr[i] == '"') { strStart = i + 1; break; }
            else value += arr[i];
        }
        if (!value.empty()) prompts.push_back(value);
    }

    if (prompts.empty()) {
        return R"({"error":"empty_prompts"})";
    }

    // Parse config
    SwarmConfig config;
    std::string tmp;
    if (ExtractJsonString(body, "strategy", tmp) || ExtractJsonString(body, "mergeStrategy", tmp)) {
        config.mergeStrategy = tmp;
    }
    if (ExtractJsonNumber(body, "maxParallel", tmp)) {
        config.maxParallel = std::max(1, std::stoi(tmp));
    }
    if (ExtractJsonNumber(body, "timeoutMs", tmp)) {
        config.timeoutMs = std::max(1000, std::stoi(tmp));
    }
    ExtractJsonString(body, "mergePrompt", config.mergePrompt);

    std::string result = subagent_mgr_->executeSwarm("api", prompts, config);

    return "{\"result\":\"" + EscapeJson(result) + "\""
           ",\"task_count\":" + std::to_string(prompts.size()) +
           ",\"strategy\":\"" + config.mergeStrategy + "\"}";
}

std::string CompletionServer::HandleAgentsListRequest() {
    if (!subagent_mgr_) {
        return R"({"error":"subagent_manager_not_available"})";
    }

    auto agents = subagent_mgr_->getAllSubAgents();
    std::string json = "{\"agents\":[";
    for (size_t i = 0; i < agents.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"id\":\"" + agents[i].id + "\""
                ",\"parent_id\":\"" + agents[i].parentId + "\""
                ",\"description\":\"" + EscapeJson(agents[i].description) + "\""
                ",\"state\":\"" + agents[i].stateString() + "\""
                ",\"progress\":" + std::to_string(agents[i].progress) +
                ",\"elapsed_ms\":" + std::to_string(agents[i].elapsedMs()) +
                ",\"tokens\":" + std::to_string(agents[i].tokensGenerated) + "}";
    }
    json += "],\"total\":" + std::to_string(agents.size()) + "}";
    return json;
}

std::string CompletionServer::HandleAgentsStatusRequest() {
    if (!subagent_mgr_) {
        return R"({"error":"subagent_manager_not_available"})";
    }

    std::string summary = subagent_mgr_->getStatusSummary();
    int active = subagent_mgr_->activeSubAgentCount();
    int total = subagent_mgr_->totalSpawned();

    auto todos = subagent_mgr_->getTodoList();
    std::string todosJson = "[";
    for (size_t i = 0; i < todos.size(); i++) {
        if (i > 0) todosJson += ",";
        todosJson += todos[i].toJSON();
    }
    todosJson += "]";

    return "{\"summary\":\"" + EscapeJson(summary) + "\""
           ",\"active\":" + std::to_string(active) +
           ",\"total_spawned\":" + std::to_string(total) +
           ",\"todos\":" + todosJson + "}";
}

// ============================================================================
// Phase 5 — History & Replay Handlers
// ============================================================================

std::string CompletionServer::HandleHistoryRequest(const std::string& path, const std::string& body) {
    if (!history_recorder_) {
        return R"({"error":"history_recorder_not_available"})";
    }

    HistoryQuery q;

    // Parse query parameters from URL path (e.g., /api/agents/history?agent_id=sa-xxx&limit=50)
    auto parseParam = [&](const std::string& key) -> std::string {
        std::string needle = key + "=";
        auto pos = path.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        auto end = path.find('&', pos);
        if (end == std::string::npos) end = path.size();
        return path.substr(pos, end - pos);
    };

    // Also check POST body for JSON filter fields
    std::string tmp;
    if (!parseParam("agent_id").empty()) q.agentId = parseParam("agent_id");
    else if (ExtractJsonString(body, "agent_id", tmp)) q.agentId = tmp;

    if (!parseParam("session_id").empty()) q.sessionId = parseParam("session_id");
    else if (ExtractJsonString(body, "session_id", tmp)) q.sessionId = tmp;

    if (!parseParam("event_type").empty()) q.eventType = parseParam("event_type");
    else if (ExtractJsonString(body, "event_type", tmp)) q.eventType = tmp;

    if (!parseParam("parent_id").empty()) q.parentId = parseParam("parent_id");
    else if (ExtractJsonString(body, "parent_id", tmp)) q.parentId = tmp;

    std::string limitStr = parseParam("limit");
    if (!limitStr.empty()) q.limit = std::max(1, std::stoi(limitStr));
    else if (ExtractJsonNumber(body, "limit", tmp)) q.limit = std::max(1, std::stoi(tmp));

    std::string offsetStr = parseParam("offset");
    if (!offsetStr.empty()) q.offset = std::max(0, std::stoi(offsetStr));
    else if (ExtractJsonNumber(body, "offset", tmp)) q.offset = std::max(0, std::stoi(tmp));

    auto events = history_recorder_->query(q);
    std::string eventsJson = history_recorder_->toJSON(events);
    std::string stats = history_recorder_->getStatsSummary();

    return "{\"events\":" + eventsJson +
           ",\"count\":" + std::to_string(events.size()) +
           ",\"stats\":" + stats + "}";
}

std::string CompletionServer::HandleReplayRequest(const std::string& body) {
    if (!history_recorder_) {
        return R"({"error":"history_recorder_not_available"})";
    }
    if (!subagent_mgr_) {
        return R"({"error":"subagent_manager_not_available"})";
    }

    ReplayRequest req;
    std::string tmp;
    if (ExtractJsonString(body, "agent_id", tmp)) req.originalAgentId = tmp;
    if (ExtractJsonString(body, "session_id", tmp)) req.originalSessionId = tmp;
    if (ExtractJsonString(body, "event_type", tmp)) req.eventType = tmp;

    // Check for dry_run
    auto dryPos = body.find("\"dry_run\"");
    if (dryPos != std::string::npos) {
        auto valPos = body.find(':', dryPos);
        if (valPos != std::string::npos) {
            auto afterColon = body.substr(valPos + 1, 10);
            req.dryRun = (afterColon.find("true") != std::string::npos);
        }
    }

    ReplayResult result = history_recorder_->replay(req, subagent_mgr_);

    return "{\"success\":" + std::string(result.success ? "true" : "false") +
           ",\"result\":\"" + EscapeJson(result.result) + "\"" +
           ",\"original_result\":\"" + EscapeJson(result.originalResult) + "\"" +
           ",\"events_replayed\":" + std::to_string(result.eventsReplayed) +
           ",\"duration_ms\":" + std::to_string(result.durationMs) + "}";
}

// ============================================================================
// Phase 7 — Policy API Handlers
// ============================================================================

std::string CompletionServer::HandlePoliciesRequest(const std::string& path, const std::string& body) {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    auto policies = policy_engine_->getAllPolicies();
    std::string json = "{\"policies\":[";
    for (size_t i = 0; i < policies.size(); ++i) {
        if (i > 0) json += ",";
        json += policies[i].toJSON();
    }
    json += "],\"count\":" + std::to_string(policies.size()) + "}";
    return json;
}

std::string CompletionServer::HandlePolicySuggestionsRequest(const std::string& body) {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    // Generate fresh suggestions from history
    auto suggestions = policy_engine_->generateSuggestions();
    auto pending = policy_engine_->getPendingSuggestions();

    std::string json = "{\"suggestions\":[";
    for (size_t i = 0; i < pending.size(); ++i) {
        if (i > 0) json += ",";
        json += pending[i].toJSON();
    }
    json += "],\"pending\":" + std::to_string(pending.size()) +
            ",\"new_generated\":" + std::to_string(suggestions.size()) + "}";
    return json;
}

std::string CompletionServer::HandlePolicyApplyRequest(const std::string& body) {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    std::string suggestionId;
    if (!ExtractJsonString(body, "suggestion_id", suggestionId)) {
        return R"({"error":"missing_suggestion_id"})";
    }

    bool ok = policy_engine_->acceptSuggestion(suggestionId);
    return "{\"success\":" + std::string(ok ? "true" : "false") +
           ",\"suggestion_id\":\"" + EscapeJson(suggestionId) + "\"}";
}

std::string CompletionServer::HandlePolicyRejectRequest(const std::string& body) {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    std::string suggestionId;
    if (!ExtractJsonString(body, "suggestion_id", suggestionId)) {
        return R"({"error":"missing_suggestion_id"})";
    }

    bool ok = policy_engine_->rejectSuggestion(suggestionId);
    return "{\"success\":" + std::string(ok ? "true" : "false") +
           ",\"suggestion_id\":\"" + EscapeJson(suggestionId) + "\"}";
}

std::string CompletionServer::HandlePolicyExportRequest() {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    return policy_engine_->exportPolicies();
}

std::string CompletionServer::HandlePolicyImportRequest(const std::string& body) {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    int count = policy_engine_->importPolicies(body);
    policy_engine_->save();
    return "{\"imported\":" + std::to_string(count) + "}";
}

std::string CompletionServer::HandlePolicyHeuristicsRequest() {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    policy_engine_->computeHeuristics();
    return policy_engine_->heuristicsSummaryJSON();
}

std::string CompletionServer::HandlePolicyStatsRequest() {
    if (!policy_engine_) {
        return R"({"error":"policy_engine_not_available"})";
    }

    return policy_engine_->getStatsSummary();
}

// ============================================================================
// Phase 8A: Explainability API Handlers
// ============================================================================

std::string CompletionServer::HandleExplainRequest(const std::string& path, const std::string& body) {
    if (!explain_engine_) {
        return R"({"error":"explainability_engine_not_available"})";
    }

    // Parse agent_id from query string: /api/agents/explain?agent_id=xxx
    std::string agentId;
    auto qpos = path.find('?');
    if (qpos != std::string::npos) {
        std::string qs = path.substr(qpos + 1);
        // Simple query param parser
        size_t pos = 0;
        while (pos < qs.size()) {
            size_t eq = qs.find('=', pos);
            if (eq == std::string::npos) break;
            size_t amp = qs.find('&', eq);
            if (amp == std::string::npos) amp = qs.size();
            std::string key = qs.substr(pos, eq - pos);
            std::string val = qs.substr(eq + 1, amp - eq - 1);
            if (key == "agent_id") agentId = val;
            pos = amp + 1;
        }
    }

    // Also check JSON body for agent_id
    if (agentId.empty() && !body.empty()) {
        auto idPos = body.find("\"agent_id\"");
        if (idPos != std::string::npos) {
            auto colonPos = body.find(':', idPos);
            auto quoteStart = body.find('"', colonPos + 1);
            auto quoteEnd = body.find('"', quoteStart + 1);
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                agentId = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
        }
    }

    if (agentId.empty()) {
        // Return session-level explanation
        auto session = explain_engine_->explainSession();
        return session.toJSON();
    }

    // Build trace for specific agent
    auto trace = explain_engine_->traceAuto(agentId);
    return trace.toJSON();
}

std::string CompletionServer::HandleExplainStatsRequest() {
    if (!explain_engine_) {
        return R"({"error":"explainability_engine_not_available"})";
    }

    // Build aggregated stats
    auto session = explain_engine_->explainSession();
    std::ostringstream ss;
    ss << "{";
    ss << "\"totalEvents\":" << session.totalEvents;
    ss << ",\"agentSpawns\":" << session.agentSpawns;
    ss << ",\"chainExecutions\":" << session.chainExecutions;
    ss << ",\"swarmExecutions\":" << session.swarmExecutions;
    ss << ",\"failures\":" << session.failures;
    ss << ",\"retries\":" << session.retries;
    ss << ",\"policyFirings\":" << session.policyFirings;
    ss << ",\"failureAttributionCount\":" << session.failureAttributions.size();
    ss << ",\"policyAttributionCount\":" << session.policyAttributions.size();
    ss << ",\"traceCount\":" << session.traces.size();
    ss << "}";
    return ss.str();
}

// ============================================================================
// Phase 8B — Backend Switcher API Handlers
// ============================================================================

std::string CompletionServer::HandleBackendsListRequest() {
    if (!backend_mgr_) {
        return R"({"error":"backend_manager_not_initialized"})";
    }
    return backend_mgr_->toJSON();
}

std::string CompletionServer::HandleBackendsStatusRequest() {
    if (!backend_mgr_) {
        return R"({"error":"backend_manager_not_initialized"})";
    }
    return backend_mgr_->getStatsJSON();
}

std::string CompletionServer::HandleBackendsUseRequest(const std::string& body) {
    if (!backend_mgr_) {
        return R"({"error":"backend_manager_not_initialized"})";
    }

    // Parse "id" field from JSON body — minimal extraction
    std::string id;
    auto pos = body.find("\"id\"");
    if (pos == std::string::npos) pos = body.find("\"backend\"");
    if (pos != std::string::npos) {
        auto colon = body.find(':', pos);
        if (colon != std::string::npos) {
            auto qStart = body.find('"', colon + 1);
            if (qStart != std::string::npos) {
                auto qEnd = body.find('"', qStart + 1);
                if (qEnd != std::string::npos) {
                    id = body.substr(qStart + 1, qEnd - qStart - 1);
                }
            }
        }
    }

    if (id.empty()) {
        return R"({"success":false,"error":"Missing 'id' or 'backend' field in request body"})";
    }

    bool ok = backend_mgr_->setActiveBackend(id);
    if (ok) {
        return "{\"success\":true,\"active\":\"" + id +
               "\",\"activeName\":\"" + backend_mgr_->getActiveBackendName() + "\"}";
    }
    return "{\"success\":false,\"error\":\"Backend '" + id + "' not found or disabled\"}";
}

// ============================================================================
// Cursor IDE Integration Handlers
// ============================================================================

std::string CompletionServer::HandleCursorInlineEditRequest(const std::string& body) {
    // Request format: {"code": "...", "instruction": "...", "language": "cpp"}
    std::string code, instruction, language;
    ExtractJsonString(body, "code", code);
    ExtractJsonString(body, "instruction", instruction);
    ExtractJsonString(body, "language", language);
    
    if (code.empty() || instruction.empty()) {
        return R"({"error":"Missing code or instruction"})";
    }
    
    // Build a prompt for inline editing
    std::string prompt = "You are an expert code editor. " + instruction + "\n\n";
    prompt += "Current " + language + " code:\n```" + language + "\n" + code + "\n```\n";
    prompt += "Provide ONLY the improved code in a code block.\n";
    
    // Call inference engine if available
    if (!engine_ || !engine_->IsModelLoaded()) {
        return R"({"error":"Model not loaded"})";
    }
    
    auto tokens = engine_->Tokenize(prompt);
    auto generated = engine_->Generate(tokens, 2048);
    std::string result = engine_->Detokenize(generated);
    
    // Extract code from code block if present
    size_t codeStart = result.find("```");
    size_t codeEnd = std::string::npos;
    if (codeStart != std::string::npos) {
        codeStart = result.find('\n', codeStart) + 1;
        codeEnd = result.find("```", codeStart);
        if (codeEnd != std::string::npos) {
            result = result.substr(codeStart, codeEnd - codeStart);
        }
    }
    
    std::string escaped = EscapeJson(result);
    return "{\"success\":true,\"edited_code\":\"" + escaped + "\"}";
}

std::string CompletionServer::HandleCursorDiffPreviewRequest(const std::string& body) {
    // Request format: {"original": "...", "edited": "..."}
    std::string original, edited;
    ExtractJsonString(body, "original", original);
    ExtractJsonString(body, "edited", edited);
    
    if (original.empty() || edited.empty()) {
        return R"({"error":"Missing original or edited content"})";
    }
    
    // Simple line-by-line diff
    std::vector<std::string> origLines, editedLines;
    std::istringstream origStream(original), editStream(edited);
    std::string line;
    
    while (std::getline(origStream, line)) {
        origLines.push_back(line);
    }
    while (std::getline(editStream, line)) {
        editedLines.push_back(line);
    }
    
    std::stringstream diffJson;
    diffJson << R"({"diffs":[)";
    
    size_t maxLines = std::max(origLines.size(), editedLines.size());
    for (size_t i = 0; i < maxLines; i++) {
        if (i > 0) diffJson << ",";
        
        std::string origLine = i < origLines.size() ? origLines[i] : "";
        std::string editedLine = i < editedLines.size() ? editedLines[i] : "";
        
        if (origLine == editedLine) {
            diffJson << R"({"type":"unchanged","line":)" << (i + 1) << R"(,"content":")" 
                     << EscapeJson(origLine) << "\"}";
        } else {
            if (!origLine.empty()) {
                diffJson << R"({"type":"removed","line":)" << (i + 1) << R"(,"content":")" 
                         << EscapeJson(origLine) << "\"}";
            }
            if (!editedLine.empty()) {
                diffJson << ",{\"type\":\"added\",\"line\":" << (i + 1) << R"(,"content":")"
                         << EscapeJson(editedLine) << "\"}";
            }
        }
    }
    
    diffJson << "]}";
    return diffJson.str();
}

std::string CompletionServer::HandleCursorGhostTextRequest(const std::string& body) {
    // Request format: {"buffer": "...", "cursor_offset": N, "max_length": N}
    std::string buffer;
    std::string cursor_offset_raw, max_length_raw;
    ExtractJsonString(body, "buffer", buffer);
    ExtractJsonNumber(body, "cursor_offset", cursor_offset_raw);
    ExtractJsonNumber(body, "max_length", max_length_raw);
    
    size_t cursor_offset = 0;
    int max_length = 80;
    
    if (!cursor_offset_raw.empty()) {
        cursor_offset = static_cast<size_t>(std::stoull(cursor_offset_raw));
    }
    if (!max_length_raw.empty()) {
        max_length = std::stoi(max_length_raw);
    }
    
    if (cursor_offset > buffer.size()) {
        cursor_offset = buffer.size();
    }
    
    // Get completion
    std::string prompt = buffer.substr(0, cursor_offset);
    std::string completion;
    
    if (engine_ && engine_->IsModelLoaded()) {
        auto tokens = engine_->Tokenize(prompt);
        auto generated = engine_->Generate(tokens, 256);
        completion = engine_->Detokenize(generated);
    }
    
    // Take first line of completion
    size_t newlinePos = completion.find('\n');
    std::string ghostText = (newlinePos != std::string::npos) 
        ? completion.substr(0, newlinePos) 
        : completion;
    
    // Trim to max length
    if (ghostText.length() > max_length) {
        ghostText = ghostText.substr(0, max_length - 3) + "...";
    }
    
    std::string escaped = EscapeJson(ghostText);
    return "{\"success\":true,\"ghost_text\":\"" + escaped + "\",\"opacity\":0.4,\"style\":\"italic\"}";
}

std::string CompletionServer::HandleCursorContextRequest(const std::string& body) {
    // Request format: {"file_path": "...", "language": "cpp", "line": N, "column": N}
    std::string filePath, language;
    std::string line_raw, column_raw;
    ExtractJsonString(body, "file_path", filePath);
    ExtractJsonString(body, "language", language);
    ExtractJsonNumber(body, "line", line_raw);
    ExtractJsonNumber(body, "column", column_raw);
    
    // Return context about the current cursor position
    // This would typically read from LSP or file system
    std::stringstream context;
    context << R"({"file_path":")" << EscapeJson(filePath) << "\""
            << R"(,"language":")" << language << "\""
            << R"(,"line":)" << (line_raw.empty() ? "0" : line_raw)
            << R"(,"column":)" << (column_raw.empty() ? "0" : column_raw)
            << R"(,"has_cursor":true,"suggestions_available":true})";
    
    return context.str();
}

std::string CompletionServer::HandleCursorKeystrokeRequest(const std::string& body) {
    // Request format: {"key": ".", "buffer": "...", "cursor_offset": N, "language": "cpp"}
    std::string key, buffer, language;
    std::string cursor_offset_raw;
    ExtractJsonString(body, "key", key);
    ExtractJsonString(body, "buffer", buffer);
    ExtractJsonString(body, "language", language);
    ExtractJsonNumber(body, "cursor_offset", cursor_offset_raw);
    
    size_t cursor_offset = 0;
    if (!cursor_offset_raw.empty()) {
        cursor_offset = static_cast<size_t>(std::stoull(cursor_offset_raw));
    }
    
    // Determine what to do based on keystroke
    std::string action = "none";
    bool shouldComplete = false;
    
    if (key == ".") {
        action = "member_completion";
        shouldComplete = true;
    } else if (key == ":") {
        action = "scope_resolution";
        shouldComplete = true;
    } else if (key == "(") {
        action = "function_signature";
        shouldComplete = true;
    } else if (key == "\t") {
        action = "smart_indent_or_complete";
        shouldComplete = true;
    }
    
    // Generate completions if needed
    std::vector<std::string> completions;
    if (shouldComplete && engine_ && engine_->IsModelLoaded() && cursor_offset <= buffer.size()) {
        std::string prompt = buffer.substr(0, cursor_offset);
        auto tokens = engine_->Tokenize(prompt);
        auto generated = engine_->Generate(tokens, 256);
        completions.push_back(engine_->Detokenize(generated));
    }
    
    std::stringstream response;
    response << R"({"action":")" << action << "\""
             << R"(,"should_complete":)" << (shouldComplete ? "true" : "false")
             << R"(,"completions":[)";
    
    for (size_t i = 0; i < completions.size(); i++) {
        if (i > 0) response << ",";
        response << "\"" << EscapeJson(completions[i]) << "\"";
    }
    
    response << "]}";
    return response.str();
}

std::string CompletionServer::HandleCursorMultiFileRequest(const std::string& body) {
    // Request format: {"current_file": "...", "related_files": [...], "language": "cpp"}
    // This would integrate with LSP to get symbol info, imports, etc.
    
    std::string currentFile;
    ExtractJsonString(body, "current_file", currentFile);
    
    // Build multi-file context for better completions
    std::stringstream context;
    context << R"({"current_file":")" << EscapeJson(currentFile) << "\""
            << R"(,"related_files":[],"imports":[])"
            << R"(,"symbols":[],"context_size":0,"success":true})";
    
    return context.str();
}

} // namespace RawrXD
