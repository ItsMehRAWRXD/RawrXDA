#include "complete_server.h"
#include "agentic_engine.h"
#include "subagent_core.h"
#include "agent_history.h"
#include "agent_policy.h"
#include "agent_explainability.h"
#include "ai_backend.h"
#include "gpu/speculative_decoder_v2.h"
#include "core/flash_attention.h"
#include "activation_compressor.h"
#include "core/distributed_pipeline_orchestrator.hpp"
#include "core/hotpatch_control_plane.hpp"
#include "core/static_analysis_engine.hpp"
#include "core/semantic_code_intelligence.hpp"
#include "core/enterprise_telemetry_compliance.hpp"

// Phase 26: ReverseEngineered MASM Kernel Bridge
#include "../include/reverse_engineered_bridge.h"

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

bool CompletionServer::Start(uint16_t port, InferenceEngine* engine, std::string model_path) {
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
    // === Phase 10: Speculative Decoding API Routes ===
    else if (method == "GET" && path == "/api/speculative/status") {
        response_body = HandleSpecDecStatusRequest();
    } else if (method == "POST" && path == "/api/speculative/config") {
        response_body = HandleSpecDecConfigRequest(parsed_body);
    } else if (method == "GET" && path == "/api/speculative/stats") {
        response_body = HandleSpecDecStatsRequest();
    } else if (method == "POST" && path == "/api/speculative/generate") {
        response_body = HandleSpecDecGenerateRequest(parsed_body);
    } else if (method == "POST" && path == "/api/speculative/reset") {
        response_body = HandleSpecDecResetRequest();
    }
    // === Phase 11: Flash Attention v2 API Routes ===
    else if (method == "GET" && path == "/api/flash-attention/status") {
        response_body = HandleFlashAttnStatusRequest();
    } else if (method == "GET" && path == "/api/flash-attention/config") {
        response_body = HandleFlashAttnConfigRequest();
    } else if (method == "POST" && path == "/api/flash-attention/benchmark") {
        response_body = HandleFlashAttnBenchmarkRequest(parsed_body);
    }
    // === Phase 12: Extreme Compression API Routes ===
    else if (method == "GET" && path == "/api/compression/status") {
        response_body = HandleCompressionStatusRequest();
    } else if (method == "GET" && path == "/api/compression/profiles") {
        response_body = HandleCompressionProfilesRequest();
    } else if (method == "POST" && path == "/api/compression/compress") {
        response_body = HandleCompressionCompressRequest(parsed_body);
    } else if (method == "GET" && path == "/api/compression/stats") {
        response_body = HandleCompressionStatsRequest();
    }
    // === Phase 13: Distributed Pipeline Orchestrator API Routes ===
    else if (method == "GET" && path == "/api/pipeline/status") {
        response_body = HandlePipelineStatusRequest();
    } else if (method == "POST" && path == "/api/pipeline/submit") {
        response_body = HandlePipelineSubmitRequest(parsed_body);
    } else if (method == "GET" && path == "/api/pipeline/tasks") {
        response_body = HandlePipelineTasksRequest();
    } else if (method == "POST" && path == "/api/pipeline/cancel") {
        response_body = HandlePipelineCancelRequest(parsed_body);
    } else if (method == "GET" && path == "/api/pipeline/nodes") {
        response_body = HandlePipelineNodesRequest();
    }
    // === Phase 14: Advanced Hotpatch Control Plane API Routes ===
    else if (method == "GET" && path == "/api/hotpatch-cp/status") {
        response_body = HandleHotpatchCPStatusRequest();
    } else if (method == "GET" && path == "/api/hotpatch-cp/patches") {
        response_body = HandleHotpatchCPPatchesRequest();
    } else if (method == "POST" && path == "/api/hotpatch-cp/apply") {
        response_body = HandleHotpatchCPApplyRequest(parsed_body);
    } else if (method == "POST" && path == "/api/hotpatch-cp/rollback") {
        response_body = HandleHotpatchCPRollbackRequest(parsed_body);
    } else if (method == "GET" && path == "/api/hotpatch-cp/audit") {
        response_body = HandleHotpatchCPAuditRequest();
    }
    // === Phase 15: Static Analysis Engine API Routes ===
    else if (method == "GET" && path == "/api/analysis/status") {
        response_body = HandleAnalysisStatusRequest();
    } else if (method == "GET" && path == "/api/analysis/functions") {
        response_body = HandleAnalysisFunctionsRequest();
    } else if (method == "POST" && path == "/api/analysis/run") {
        response_body = HandleAnalysisRunRequest(parsed_body);
    } else if (method == "POST" && path == "/api/analysis/cfg") {
        response_body = HandleAnalysisCfgRequest(parsed_body);
    }
    // === Phase 16: Semantic Code Intelligence API Routes ===
    else if (method == "GET" && path == "/api/semantic/status") {
        response_body = HandleSemanticStatusRequest();
    } else if (method == "POST" && path == "/api/semantic/index") {
        response_body = HandleSemanticIndexRequest(parsed_body);
    } else if (method == "POST" && path == "/api/semantic/search") {
        response_body = HandleSemanticSearchRequest(parsed_body);
    } else if (method == "POST" && path == "/api/semantic/goto") {
        response_body = HandleSemanticGotoRequest(parsed_body);
    } else if (method == "POST" && path == "/api/semantic/references") {
        response_body = HandleSemanticReferencesRequest(parsed_body);
    }
    // === Phase 17: Enterprise Telemetry & Compliance API Routes ===
    else if (method == "GET" && path == "/api/telemetry/status") {
        response_body = HandleTelemetryStatusRequest();
    } else if ((method == "GET" || method == "POST") && path == "/api/telemetry/audit") {
        response_body = HandleTelemetryAuditRequest(parsed_body);
    } else if (method == "GET" && path == "/api/telemetry/compliance") {
        response_body = HandleTelemetryComplianceRequest();
    } else if (method == "GET" && path == "/api/telemetry/license") {
        response_body = HandleTelemetryLicenseRequest();
    } else if (method == "GET" && path == "/api/telemetry/metrics") {
        response_body = HandleTelemetryMetricsRequest();
    } else if (method == "POST" && path == "/api/telemetry/export") {
        response_body = HandleTelemetryExportRequest(parsed_body);
    }
    // === Phase 26: ReverseEngineered Kernel API Routes ===
    else if (method == "GET" && path == "/api/scheduler/status") {
        response_body = HandleSchedulerStatusRequest();
    } else if (method == "POST" && path == "/api/scheduler/submit") {
        response_body = HandleSchedulerSubmitRequest(parsed_body);
    } else if (method == "GET" && path == "/api/conflict/status") {
        response_body = HandleConflictStatusRequest();
    } else if (method == "GET" && path == "/api/heartbeat/status") {
        response_body = HandleHeartbeatStatusRequest();
    } else if (method == "POST" && path == "/api/heartbeat/add") {
        response_body = HandleHeartbeatAddRequest(parsed_body);
    } else if (method == "GET" && path == "/api/gpu/dma/status") {
        response_body = HandleGpuDmaStatusRequest();
    } else if (method == "POST" && path == "/api/tensor/bench") {
        response_body = HandleTensorBenchRequest(parsed_body);
    } else if (method == "GET" && path == "/api/timer") {
        response_body = HandleTimerRequest();
    } else if (method == "POST" && path == "/api/crc32") {
        response_body = HandleCrc32Request(parsed_body);
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
// Phase 10 — Speculative Decoding API Handlers
// ============================================================================

std::string CompletionServer::HandleSpecDecStatusRequest() {
    auto& dec = RawrXD::Speculative::SpeculativeDecoderV2::Global();
    auto stats = dec.getStats();
    bool generating = dec.isGenerating();
    auto& cfg = dec.getConfig();

    return "{\"phase\":10,\"name\":\"speculative_decoding\",\"ready\":true"
           ",\"generating\":" + std::string(generating ? "true" : "false") +
           ",\"acceptanceRate\":" + std::to_string(stats.acceptanceRate) +
           ",\"speedupRatio\":" + std::to_string(stats.speedupRatio) +
           ",\"maxDraftTokens\":" + std::to_string(cfg.maxDraftTokens) +
           ",\"adaptiveDraftLen\":" + std::string(cfg.adaptiveDraftLen ? "true" : "false") +
           ",\"treeSpeculation\":" + std::string(cfg.treeSpeculation ? "true" : "false") +
           "}";
}

std::string CompletionServer::HandleSpecDecConfigRequest(const std::string& body) {
    auto& dec = RawrXD::Speculative::SpeculativeDecoderV2::Global();
    auto cfg = dec.getConfig();

    // Parse optional fields from body
    std::string val;
    auto pos = body.find("\"maxDraftTokens\"");
    if (pos != std::string::npos) {
        auto colon = body.find(':', pos);
        if (colon != std::string::npos)
            cfg.maxDraftTokens = std::atoi(body.c_str() + colon + 1);
    }

    pos = body.find("\"acceptanceThreshold\"");
    if (pos != std::string::npos) {
        auto colon = body.find(':', pos);
        if (colon != std::string::npos)
            cfg.acceptanceThreshold = static_cast<float>(std::atof(body.c_str() + colon + 1));
    }

    pos = body.find("\"adaptiveDraftLen\"");
    if (pos != std::string::npos) {
        cfg.adaptiveDraftLen = (body.find("true", pos) != std::string::npos);
    }

    pos = body.find("\"treeSpeculation\"");
    if (pos != std::string::npos) {
        cfg.treeSpeculation = (body.find("true", pos) != std::string::npos);
    }

    pos = body.find("\"treeBranching\"");
    if (pos != std::string::npos) {
        auto colon = body.find(':', pos);
        if (colon != std::string::npos)
            cfg.treeBranching = std::atoi(body.c_str() + colon + 1);
    }

    pos = body.find("\"treeDepth\"");
    if (pos != std::string::npos) {
        auto colon = body.find(':', pos);
        if (colon != std::string::npos)
            cfg.treeDepth = std::atoi(body.c_str() + colon + 1);
    }

    dec.setConfig(cfg);

    return "{\"success\":true,\"maxDraftTokens\":" + std::to_string(cfg.maxDraftTokens) +
           ",\"acceptanceThreshold\":" + std::to_string(cfg.acceptanceThreshold) +
           ",\"adaptiveDraftLen\":" + std::string(cfg.adaptiveDraftLen ? "true" : "false") +
           ",\"treeSpeculation\":" + std::string(cfg.treeSpeculation ? "true" : "false") +
           "}";
}

std::string CompletionServer::HandleSpecDecStatsRequest() {
    auto& dec = RawrXD::Speculative::SpeculativeDecoderV2::Global();
    auto stats = dec.getStats();

    return "{\"totalDrafted\":" + std::to_string(stats.totalDrafted) +
           ",\"totalAccepted\":" + std::to_string(stats.totalAccepted) +
           ",\"totalRejected\":" + std::to_string(stats.totalRejected) +
           ",\"totalVerified\":" + std::to_string(stats.totalVerified) +
           ",\"acceptanceRate\":" + std::to_string(stats.acceptanceRate) +
           ",\"tokensPerSecond\":" + std::to_string(stats.tokensPerSecond) +
           ",\"speedupRatio\":" + std::to_string(stats.speedupRatio) +
           ",\"currentDraftLen\":" + std::to_string(stats.currentDraftLen) +
           ",\"avgDraftLatencyMs\":" + std::to_string(stats.avgDraftLatencyMs) +
           ",\"avgVerifyLatencyMs\":" + std::to_string(stats.avgVerifyLatencyMs) +
           "}";
}

std::string CompletionServer::HandleSpecDecGenerateRequest(const std::string& body) {
    auto& dec = RawrXD::Speculative::SpeculativeDecoderV2::Global();

    // Extract prompt and max_tokens
    std::string prompt;
    int maxTokens = 128;
    ExtractJsonString(body, "prompt", prompt);
    std::string maxTokRaw;
    if (ExtractJsonNumber(body, "max_tokens", maxTokRaw))
        maxTokens = std::max(1, std::atoi(maxTokRaw.c_str()));

    if (prompt.empty()) {
        return R"({"error":"Missing 'prompt' field"})";
    }

    auto result = dec.generateFromText(prompt, maxTokens);
    if (!result.success) {
        return std::string("{\"error\":\"") + result.detail + "\"}";
    }

    // Build token array
    std::string tokJson = "[";
    for (size_t i = 0; i < result.tokens.size(); ++i) {
        if (i > 0) tokJson += ",";
        tokJson += "{\"id\":" + std::to_string(result.tokens[i].id) +
                   ",\"logprob\":" + std::to_string(result.tokens[i].logprob) +
                   ",\"text\":\"" + result.tokens[i].text + "\"}";
    }
    tokJson += "]";

    return "{\"success\":true,\"tokens\":" + tokJson +
           ",\"acceptanceRate\":" + std::to_string(result.stats.acceptanceRate) +
           ",\"speedupRatio\":" + std::to_string(result.stats.speedupRatio) +
           ",\"tokensPerSecond\":" + std::to_string(result.stats.tokensPerSecond) +
           "}";
}

std::string CompletionServer::HandleSpecDecResetRequest() {
    auto& dec = RawrXD::Speculative::SpeculativeDecoderV2::Global();
    dec.resetStats();
    return R"({"success":true,"message":"Speculative decoding stats reset"})";
}

// ============================================================================
// Phase 11 — Flash Attention v2 API Handlers
// ============================================================================

std::string CompletionServer::HandleFlashAttnStatusRequest() {
    RawrXD::FlashAttentionEngine engine;
    bool ready = engine.Initialize();

    return "{\"phase\":11,\"name\":\"flash_attention_v2\",\"ready\":" +
           std::string(ready ? "true" : "false") +
           ",\"hasAVX512\":" + std::string(engine.HasAVX512() ? "true" : "false") +
           ",\"licensed\":" + std::string(engine.IsLicensed() ? "true" : "false") +
           ",\"status\":\"" + engine.GetStatusString() +
           "\",\"totalCalls\":" + std::to_string(engine.GetCallCount()) +
           ",\"totalTiles\":" + std::to_string(engine.GetTileCount()) +
           "}";
}

std::string CompletionServer::HandleFlashAttnConfigRequest() {
    RawrXD::FlashAttentionEngine engine;
    engine.Initialize();

    auto tc = engine.GetTileConfig();

    return "{\"tileM\":" + std::to_string(tc.tileM) +
           ",\"tileN\":" + std::to_string(tc.tileN) +
           ",\"headDim\":" + std::to_string(tc.headDim) +
           ",\"scratchBytes\":" + std::to_string(tc.scratchBytes) +
           ",\"hasAVX512\":" + std::string(engine.HasAVX512() ? "true" : "false") +
           ",\"isReady\":" + std::string(engine.IsReady() ? "true" : "false") +
           "}";
}

std::string CompletionServer::HandleFlashAttnBenchmarkRequest(const std::string& body) {
    RawrXD::FlashAttentionEngine engine;
    if (!engine.Initialize()) {
        return "{\"error\":\"Flash Attention not available (no AVX-512 or license)\"}}";
    }

    // Parse optional benchmark params
    int seqLen = 512, headDim = 128, numHeads = 32, batchSize = 1;
    std::string val;
    if (ExtractJsonNumber(body, "seq_len", val))   seqLen   = std::max(16, std::atoi(val.c_str()));
    if (ExtractJsonNumber(body, "head_dim", val))   headDim  = std::max(16, std::atoi(val.c_str()));
    if (ExtractJsonNumber(body, "num_heads", val))  numHeads = std::max(1, std::atoi(val.c_str()));
    if (ExtractJsonNumber(body, "batch_size", val)) batchSize = std::max(1, std::atoi(val.c_str()));

    size_t totalElems = static_cast<size_t>(batchSize) * numHeads * seqLen * headDim;

    // Allocate aligned buffers
    size_t allocSize = totalElems * sizeof(float);
    float* Q = static_cast<float*>(_aligned_malloc(allocSize, 64));
    float* K = static_cast<float*>(_aligned_malloc(allocSize, 64));
    float* V = static_cast<float*>(_aligned_malloc(allocSize, 64));
    float* O = static_cast<float*>(_aligned_malloc(allocSize, 64));

    if (!Q || !K || !V || !O) {
        if (Q) _aligned_free(Q); if (K) _aligned_free(K);
        if (V) _aligned_free(V); if (O) _aligned_free(O);
        return R"({"error":"Memory allocation failed for benchmark"})";
    }

    // Fill with small test values
    for (size_t i = 0; i < totalElems; ++i) {
        Q[i] = 0.01f * (i % 100);
        K[i] = 0.01f * ((i + 7) % 100);
        V[i] = 0.01f * ((i + 13) % 100);
        O[i] = 0.0f;
    }

    RawrXD::FlashAttentionConfig cfg = {};
    cfg.Q = Q; cfg.K = K; cfg.V = V; cfg.O = O;
    cfg.seqLenM     = seqLen;
    cfg.seqLenN     = seqLen;
    cfg.headDim     = headDim;
    cfg.numHeads    = numHeads;
    cfg.numKVHeads  = numHeads;
    cfg.batchSize   = batchSize;
    cfg.causal      = 1;
    cfg.ComputeScale();

    // Warm up
    engine.Forward(cfg);

    // Benchmark: 10 iterations
    auto start = std::chrono::high_resolution_clock::now();
    constexpr int ITERS = 10;
    for (int i = 0; i < ITERS; ++i) {
        engine.Forward(cfg);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    double avgMs = totalMs / ITERS;

    // FLOPs estimate: 2 * batch * heads * seqM * seqN * headDim (for Q*K^T + Attn*V)
    double flops = 2.0 * batchSize * numHeads * seqLen * seqLen * headDim * 2.0;
    double gflops = (flops / (avgMs * 1e6));

    _aligned_free(Q); _aligned_free(K);
    _aligned_free(V); _aligned_free(O);

    return "{\"success\":true,\"avgLatencyMs\":" + std::to_string(avgMs) +
           ",\"totalMs\":" + std::to_string(totalMs) +
           ",\"iterations\":" + std::to_string(ITERS) +
           ",\"seqLen\":" + std::to_string(seqLen) +
           ",\"headDim\":" + std::to_string(headDim) +
           ",\"numHeads\":" + std::to_string(numHeads) +
           ",\"batchSize\":" + std::to_string(batchSize) +
           ",\"estimatedGFLOPS\":" + std::to_string(gflops) +
           ",\"totalCalls\":" + std::to_string(engine.GetCallCount()) +
           ",\"totalTiles\":" + std::to_string(engine.GetTileCount()) +
           "}";
}

// ============================================================================
// Phase 12 — Extreme Compression API Handlers
// ============================================================================

std::string CompletionServer::HandleCompressionStatusRequest() {
    return "{\"phase\":12,\"name\":\"extreme_compression\",\"ready\":true"
           ",\"engines\":[\"quantization_codec\",\"kv_cache_compressor\",\"activation_pruner\"]"
           ",\"profiles\":[\"aggressive\",\"balanced\",\"fast\"]"
           "}";
}

std::string CompletionServer::HandleCompressionProfilesRequest() {
    return R"({"profiles":[)"
           R"({"id":"aggressive","name":"Aggressive","ratio":"10x","description":"Max compression, 10x KV-cache reduction, int8 quantization + activation pruning"},)"
           R"({"id":"balanced","name":"Balanced","ratio":"5x","description":"Balance of speed and quality, int8 quantization with selective pruning"},)"
           R"({"id":"fast","name":"Fast","ratio":"2x","description":"Minimal compression overhead, FP16 cast + sliding window"})"
           R"(]})";
}

std::string CompletionServer::HandleCompressionCompressRequest(const std::string& body) {
    // Extract profile and data_size_bytes
    std::string profile;
    auto pos = body.find("\"profile\"");
    if (pos != std::string::npos) {
        auto qStart = body.find('"', body.find(':', pos) + 1);
        if (qStart != std::string::npos) {
            auto qEnd = body.find('"', qStart + 1);
            if (qEnd != std::string::npos)
                profile = body.substr(qStart + 1, qEnd - qStart - 1);
        }
    }

    if (profile.empty()) profile = "balanced";

    // Determine compression ratio based on profile
    float ratio = 5.0f;
    if (profile == "aggressive") ratio = 10.0f;
    else if (profile == "fast")  ratio = 2.0f;

    // Extract simulated input size
    size_t inputBytes = 1048576; // default 1MB
    std::string sizeStr;
    if (ExtractJsonNumber(body, "data_size_bytes", sizeStr))
        inputBytes = static_cast<size_t>(std::stoull(sizeStr));

    size_t outputBytes = static_cast<size_t>(inputBytes / ratio);

    auto start = std::chrono::high_resolution_clock::now();

    // Run actual quantization codec on synthetic data to get real timing
    inference::QuantizationCodec codec;
    size_t testElems = std::min(inputBytes / sizeof(float), static_cast<size_t>(65536));
    std::vector<float> testData(testElems, 0.5f);
    for (size_t i = 0; i < testElems; ++i)
        testData[i] = static_cast<float>(i % 256) * 0.01f;

    auto [quantized, params] = codec.quantizeChannelWise(testData.data(),
                                                          static_cast<uint32_t>(testElems), 1);

    auto end = std::chrono::high_resolution_clock::now();
    double compressMs = std::chrono::duration<double, std::milli>(end - start).count();

    return "{\"success\":true,\"profile\":\"" + profile +
           "\",\"inputBytes\":" + std::to_string(inputBytes) +
           ",\"outputBytes\":" + std::to_string(outputBytes) +
           ",\"compressionRatio\":" + std::to_string(ratio) +
           ",\"latencyMs\":" + std::to_string(compressMs) +
           ",\"quantizedElements\":" + std::to_string(quantized.size()) +
           "}";
}

std::string CompletionServer::HandleCompressionStatsRequest() {
    // Return compression subsystem stats
    return R"({"totalCompressions":0,"totalDecompressions":0,)"
           R"("avgCompressionRatio":5.0,"avgLatencyMs":0.0,)"
           R"("kvCacheCompressed":false,"activationPruningActive":false})";
}

// ═══════════════════════════════════════════════════════════════════════════
// Phase 13: Distributed Pipeline Orchestrator Handlers
// ═══════════════════════════════════════════════════════════════════════════

std::string CompletionServer::HandlePipelineStatusRequest() {
    auto& orch = DistributedPipelineOrchestrator::instance();
    const auto& s = orch.getStats();
    bool running = orch.isRunning();
    uint32_t nodes = orch.aliveNodeCount();
    size_t queueDepth = orch.totalQueueDepth();

    return "{\"phase\":13,\"name\":\"Distributed Pipeline Orchestrator\","
           "\"running\":" + std::string(running ? "true" : "false") +
           ",\"aliveNodes\":" + std::to_string(nodes) +
           ",\"queueDepth\":" + std::to_string(queueDepth) +
           ",\"stats\":{"
           "\"tasksSubmitted\":" + std::to_string(s.tasksSubmitted.load()) +
           ",\"tasksCompleted\":" + std::to_string(s.tasksCompleted.load()) +
           ",\"tasksFailed\":" + std::to_string(s.tasksFailed.load()) +
           ",\"tasksCancelled\":" + std::to_string(s.tasksCancelled.load()) +
           ",\"tasksTimedOut\":" + std::to_string(s.tasksTimedOut.load()) +
           ",\"tasksStolen\":" + std::to_string(s.tasksStolen.load()) +
           ",\"successRate\":" + std::to_string(s.successRate()) +
           ",\"avgExecutionTimeMs\":" + std::to_string(s.avgExecutionTimeMs()) +
           ",\"peakQueueDepth\":" + std::to_string(s.peakQueueDepth.load()) +
           "}}";
}

std::string CompletionServer::HandlePipelineSubmitRequest(const std::string& body) {
    auto& orch = DistributedPipelineOrchestrator::instance();

    if (!orch.isRunning()) {
        PatchResult init = orch.initialize(0);
        if (!init.success)
            return "{\"error\":\"pipeline_init_failed\",\"detail\":\"" +
                   std::string(init.detail ? init.detail : "unknown") + "\"}";
    }

    std::string taskName;
    ExtractJsonString(body, "name", taskName);
    if (taskName.empty()) taskName = "api_task";

    std::string priorityStr;
    ExtractJsonString(body, "priority", priorityStr);
    TaskPriority prio = TaskPriority::Normal;
    if (priorityStr == "critical") prio = TaskPriority::Critical;
    else if (priorityStr == "high") prio = TaskPriority::High;
    else if (priorityStr == "low") prio = TaskPriority::Low;
    else if (priorityStr == "background") prio = TaskPriority::Background;

    PipelineTask task;
    task.name = taskName;
    task.priority = prio;
    task.execute = nullptr; // API tasks are metadata-only submissions
    TaskResult r = orch.submitTask(task);

    return "{\"success\":" + std::string(r.success ? "true" : "false") +
           ",\"taskId\":" + std::to_string(r.taskId) +
           ",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"}";
}

std::string CompletionServer::HandlePipelineTasksRequest() {
    auto& orch = DistributedPipelineOrchestrator::instance();
    auto pending = orch.getPendingTasks();
    auto running = orch.getRunningTasks();

    std::string json = "{\"pending\":[";
    for (size_t i = 0; i < pending.size(); ++i) {
        if (i > 0) json += ",";
        json += std::to_string(pending[i]);
    }
    json += "],\"running\":[";
    for (size_t i = 0; i < running.size(); ++i) {
        if (i > 0) json += ",";
        json += std::to_string(running[i]);
    }
    json += "],\"pendingCount\":" + std::to_string(pending.size()) +
            ",\"runningCount\":" + std::to_string(running.size()) + "}";
    return json;
}

std::string CompletionServer::HandlePipelineCancelRequest(const std::string& body) {
    auto& orch = DistributedPipelineOrchestrator::instance();

    std::string taskIdStr;
    if (ExtractJsonString(body, "task_id", taskIdStr) ||
        ExtractJsonNumber(body, "task_id", taskIdStr)) {
        uint64_t taskId = std::stoull(taskIdStr);
        PatchResult r = orch.cancelTask(taskId);
        return "{\"success\":" + std::string(r.success ? "true" : "false") +
               ",\"taskId\":" + std::to_string(taskId) +
               ",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"}";
    }

    // Cancel all
    PatchResult r = orch.cancelAll();
    return "{\"success\":" + std::string(r.success ? "true" : "false") +
           ",\"cancelled\":\"all\",\"detail\":\"" +
           std::string(r.detail ? r.detail : "") + "\"}";
}

std::string CompletionServer::HandlePipelineNodesRequest() {
    auto& orch = DistributedPipelineOrchestrator::instance();
    auto nodes = orch.getNodeStatus();

    std::string json = "{\"nodeCount\":" + std::to_string(nodes.size()) +
                       ",\"aliveCount\":" + std::to_string(orch.aliveNodeCount()) +
                       ",\"nodes\":[";
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) json += ",";
        const auto& n = nodes[i];
        json += "{\"nodeId\":" + std::to_string(n.nodeId) +
                ",\"hostname\":\"" + n.hostname +
                "\",\"totalCores\":" + std::to_string(n.totalCores) +
                ",\"availableCores\":" + std::to_string(n.availableCores) +
                ",\"hasGPU\":" + (n.hasGPU ? "true" : "false") +
                ",\"gpuCount\":" + std::to_string(n.gpuCount) +
                ",\"loadAverage\":" + std::to_string(n.loadAverage) +
                ",\"alive\":" + (n.alive ? "true" : "false") + "}";
    }
    json += "]}";
    return json;
}

// ═══════════════════════════════════════════════════════════════════════════
// Phase 14: Advanced Hotpatch Control Plane Handlers
// ═══════════════════════════════════════════════════════════════════════════

std::string CompletionServer::HandleHotpatchCPStatusRequest() {
    auto& cp = HotpatchControlPlane::instance();
    const auto& s = cp.getStats();

    return "{\"phase\":14,\"name\":\"Advanced Hotpatch Control Plane\","
           "\"stats\":{"
           "\"totalPatches\":" + std::to_string(s.totalPatches.load()) +
           ",\"activePatches\":" + std::to_string(s.activePatches.load()) +
           ",\"totalTransactions\":" + std::to_string(s.totalTransactions.load()) +
           ",\"committedTransactions\":" + std::to_string(s.committedTransactions.load()) +
           ",\"rolledBackTransactions\":" + std::to_string(s.rolledBackTransactions.load()) +
           ",\"validationsPassed\":" + std::to_string(s.validationsPassed.load()) +
           ",\"validationsFailed\":" + std::to_string(s.validationsFailed.load()) +
           ",\"conflictsDetected\":" + std::to_string(s.conflictsDetected.load()) +
           ",\"dependencyErrors\":" + std::to_string(s.dependencyErrors.load()) +
           ",\"auditEntries\":" + std::to_string(s.auditEntries.load()) +
           "}}";
}

std::string CompletionServer::HandleHotpatchCPPatchesRequest() {
    auto& cp = HotpatchControlPlane::instance();
    auto patches = cp.getAllPatches();

    std::string json = "{\"patchCount\":" + std::to_string(patches.size()) + ",\"patches\":[";
    for (size_t i = 0; i < patches.size(); ++i) {
        if (i > 0) json += ",";
        const auto* p = patches[i];
        json += "{\"patchId\":" + std::to_string(p->patchId) +
                ",\"name\":\"" + p->name +
                "\",\"version\":\"" + p->version.toString() +
                "\",\"state\":" + std::to_string(static_cast<int>(p->state)) +
                ",\"safetyLevel\":" + std::to_string(static_cast<int>(p->safetyLevel)) +
                ",\"targetLayers\":" + std::to_string(p->targetLayers) +
                ",\"validated\":" + (p->validated ? "true" : "false") +
                ",\"depCount\":" + std::to_string(p->dependencies.size()) +
                ",\"conflictCount\":" + std::to_string(p->conflicts.size()) + "}";
    }
    json += "]}";
    return json;
}

std::string CompletionServer::HandleHotpatchCPApplyRequest(const std::string& body) {
    auto& cp = HotpatchControlPlane::instance();
    std::string patchIdStr;
    if (!ExtractJsonNumber(body, "patch_id", patchIdStr))
        return R"({"error":"missing_patch_id"})";

    uint64_t patchId = std::stoull(patchIdStr);

    std::string actor, reason;
    ExtractJsonString(body, "actor", actor);
    ExtractJsonString(body, "reason", reason);
    if (actor.empty()) actor = "api";
    if (reason.empty()) reason = "Applied via HTTP API";

    // Validate first
    cp.validatePatch(patchId);

    PatchResult r = cp.applyPatch(patchId, actor, reason);
    return "{\"success\":" + std::string(r.success ? "true" : "false") +
           ",\"patchId\":" + std::to_string(patchId) +
           ",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"}";
}

std::string CompletionServer::HandleHotpatchCPRollbackRequest(const std::string& body) {
    auto& cp = HotpatchControlPlane::instance();
    std::string patchIdStr;
    if (!ExtractJsonNumber(body, "patch_id", patchIdStr))
        return R"({"error":"missing_patch_id"})";

    uint64_t patchId = std::stoull(patchIdStr);

    std::string actor, reason;
    ExtractJsonString(body, "actor", actor);
    ExtractJsonString(body, "reason", reason);
    if (actor.empty()) actor = "api";
    if (reason.empty()) reason = "Rolled back via HTTP API";

    PatchResult r = cp.rollbackPatch(patchId, actor, reason);
    return "{\"success\":" + std::string(r.success ? "true" : "false") +
           ",\"patchId\":" + std::to_string(patchId) +
           ",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"}";
}

std::string CompletionServer::HandleHotpatchCPAuditRequest() {
    auto& cp = HotpatchControlPlane::instance();
    auto log = cp.getAuditLog(200);

    std::string json = "{\"auditLogSize\":" + std::to_string(cp.auditLogSize()) +
                       ",\"entries\":[";
    for (size_t i = 0; i < log.size(); ++i) {
        if (i > 0) json += ",";
        const auto& e = log[i];
        json += "{\"entryId\":" + std::to_string(e.entryId) +
                ",\"patchId\":" + std::to_string(e.patchId) +
                ",\"transactionId\":" + std::to_string(e.transactionId) +
                ",\"oldState\":" + std::to_string(static_cast<int>(e.oldState)) +
                ",\"newState\":" + std::to_string(static_cast<int>(e.newState)) +
                ",\"actor\":\"" + e.actor +
                "\",\"reason\":\"" + e.reason + "\"}";
    }
    json += "]}";
    return json;
}

// ═══════════════════════════════════════════════════════════════════════════
// Phase 15: Static Analysis Engine Handlers
// ═══════════════════════════════════════════════════════════════════════════

std::string CompletionServer::HandleAnalysisStatusRequest() {
    auto& eng = StaticAnalysisEngine::instance();
    const auto& s = eng.getStats();

    return "{\"phase\":15,\"name\":\"Static Analysis Engine (CFG/SSA)\","
           "\"functionCount\":" + std::to_string(eng.functionCount()) +
           ",\"stats\":{"
           "\"functionsAnalyzed\":" + std::to_string(s.functionsAnalyzed.load()) +
           ",\"blocksBuilt\":" + std::to_string(s.blocksBuilt.load()) +
           ",\"instructionsParsed\":" + std::to_string(s.instructionsParsed.load()) +
           ",\"phisInserted\":" + std::to_string(s.phisInserted.load()) +
           ",\"deadCodeEliminated\":" + std::to_string(s.deadCodeEliminated.load()) +
           ",\"constantsPropagated\":" + std::to_string(s.constantsPropagated.load()) +
           ",\"loopsDetected\":" + std::to_string(s.loopsDetected.load()) +
           ",\"totalAnalysisTimeUs\":" + std::to_string(s.totalAnalysisTimeUs.load()) +
           "}}";
}

std::string CompletionServer::HandleAnalysisFunctionsRequest() {
    auto& eng = StaticAnalysisEngine::instance();
    auto funcs = eng.getAllFunctions();

    std::string json = "{\"functionCount\":" + std::to_string(funcs.size()) + ",\"functions\":[";
    for (size_t i = 0; i < funcs.size(); ++i) {
        if (i > 0) json += ",";
        uint32_t fid = funcs[i];
        const auto* cfg = eng.getCFG(fid);
        json += "{\"functionId\":" + std::to_string(fid);
        if (cfg) {
            json += ",\"name\":\"" + cfg->functionName +
                    "\",\"entryAddress\":" + std::to_string(cfg->entryAddress) +
                    ",\"totalBlocks\":" + std::to_string(cfg->totalBlocks) +
                    ",\"totalInstructions\":" + std::to_string(cfg->totalInstructions) +
                    ",\"inSSA\":" + (cfg->inSSAForm ? "true" : "false");
        }
        json += "}";
    }
    json += "]}";
    return json;
}

std::string CompletionServer::HandleAnalysisRunRequest(const std::string& body) {
    auto& eng = StaticAnalysisEngine::instance();

    std::string funcIdStr;
    if (!ExtractJsonNumber(body, "function_id", funcIdStr))
        return R"({"error":"missing_function_id"})";

    uint32_t funcId = static_cast<uint32_t>(std::stoul(funcIdStr));
    PatchResult r = eng.runFullAnalysis(funcId);

    const auto* cfg = eng.getCFG(funcId);
    std::string json = "{\"success\":" + std::string(r.success ? "true" : "false") +
                       ",\"functionId\":" + std::to_string(funcId) +
                       ",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"";
    if (cfg) {
        auto loops = eng.getLoops(funcId);
        json += ",\"totalBlocks\":" + std::to_string(cfg->totalBlocks) +
                ",\"totalInstructions\":" + std::to_string(cfg->totalInstructions) +
                ",\"inSSA\":" + (cfg->inSSAForm ? "true" : "false") +
                ",\"loopCount\":" + std::to_string(loops.size());
    }
    json += "}";
    return json;
}

std::string CompletionServer::HandleAnalysisCfgRequest(const std::string& body) {
    auto& eng = StaticAnalysisEngine::instance();

    std::string funcIdStr;
    if (!ExtractJsonNumber(body, "function_id", funcIdStr))
        return R"({"error":"missing_function_id"})";

    uint32_t funcId = static_cast<uint32_t>(std::stoul(funcIdStr));
    const auto* cfg = eng.getCFG(funcId);
    if (!cfg)
        return "{\"error\":\"function_not_found\",\"functionId\":" +
               std::to_string(funcId) + "}";

    // Build block summary
    std::string json = "{\"functionId\":" + std::to_string(funcId) +
                       ",\"functionName\":\"" + cfg->functionName +
                       "\",\"entryBlock\":" + std::to_string(cfg->entryBlockId) +
                       ",\"inSSA\":" + (cfg->inSSAForm ? "true" : "false") +
                       ",\"blocks\":[";
    size_t bi = 0;
    for (const auto& [bid, blk] : cfg->blocks) {
        if (bi++ > 0) json += ",";
        json += "{\"id\":" + std::to_string(bid) +
                ",\"label\":\"" + blk.label +
                "\",\"instructions\":" + std::to_string(blk.instructionIds.size()) +
                ",\"predecessors\":[";
        for (size_t j = 0; j < blk.predecessors.size(); ++j) {
            if (j > 0) json += ",";
            json += std::to_string(blk.predecessors[j]);
        }
        json += "],\"successors\":[";
        for (size_t j = 0; j < blk.successors.size(); ++j) {
            if (j > 0) json += ",";
            json += std::to_string(blk.successors[j]);
        }
        json += "],\"isEntry\":" + std::string(blk.isEntryBlock ? "true" : "false") +
                ",\"isExit\":" + std::string(blk.isExitBlock ? "true" : "false") +
                ",\"isLoop\":" + std::string(blk.isLoop ? "true" : "false") +
                ",\"loopDepth\":" + std::to_string(blk.loopDepth) + "}";
    }
    json += "]}";
    return json;
}

// ═══════════════════════════════════════════════════════════════════════════
// Phase 16: Semantic Code Intelligence Handlers
// ═══════════════════════════════════════════════════════════════════════════

std::string CompletionServer::HandleSemanticStatusRequest() {
    auto& sci = SemanticCodeIntelligence::instance();
    const auto& s = sci.getStats();

    return "{\"phase\":16,\"name\":\"Semantic Code Intelligence\","
           "\"stats\":{"
           "\"totalSymbols\":" + std::to_string(s.totalSymbols.load()) +
           ",\"totalReferences\":" + std::to_string(s.totalReferences.load()) +
           ",\"totalTypes\":" + std::to_string(s.totalTypes.load()) +
           ",\"totalScopes\":" + std::to_string(s.totalScopes.load()) +
           ",\"filesIndexed\":" + std::to_string(s.filesIndexed.load()) +
           ",\"queriesServed\":" + std::to_string(s.queriesServed.load()) +
           ",\"cacheHits\":" + std::to_string(s.cacheHits.load()) +
           ",\"cacheMisses\":" + std::to_string(s.cacheMisses.load()) +
           ",\"indexBuildTimeUs\":" + std::to_string(s.indexBuildTimeUs.load()) +
           "}}";
}

std::string CompletionServer::HandleSemanticIndexRequest(const std::string& body) {
    auto& sci = SemanticCodeIntelligence::instance();

    std::string filePath;
    if (!ExtractJsonString(body, "file", filePath))
        return R"({"error":"missing_file_path"})";

    std::string reindex;
    ExtractJsonString(body, "reindex", reindex);

    PatchResult r;
    if (reindex == "true")
        r = sci.reindexFile(filePath);
    else
        r = sci.indexFile(filePath);

    return "{\"success\":" + std::string(r.success ? "true" : "false") +
           ",\"file\":\"" + filePath +
           "\",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"}";
}

std::string CompletionServer::HandleSemanticSearchRequest(const std::string& body) {
    auto& sci = SemanticCodeIntelligence::instance();

    std::string query;
    if (!ExtractJsonString(body, "query", query))
        return R"({"error":"missing_query"})";

    std::string maxStr;
    uint32_t maxResults = 50;
    if (ExtractJsonNumber(body, "max_results", maxStr))
        maxResults = static_cast<uint32_t>(std::stoul(maxStr));

    auto results = sci.searchSymbols(query, SymbolKind::Unknown, maxResults);

    std::string json = "{\"query\":\"" + query +
                       "\",\"resultCount\":" + std::to_string(results.size()) +
                       ",\"symbols\":[";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) json += ",";
        const auto* sym = results[i];
        json += "{\"symbolId\":" + std::to_string(sym->symbolId) +
                ",\"name\":\"" + sym->name +
                "\",\"qualifiedName\":\"" + sym->qualifiedName +
                "\",\"kind\":" + std::to_string(static_cast<int>(sym->kind)) +
                ",\"visibility\":" + std::to_string(static_cast<int>(sym->visibility)) +
                ",\"file\":\"" + sym->definition.filePath +
                "\",\"line\":" + std::to_string(sym->definition.line) +
                ",\"references\":" + std::to_string(sym->referenceCount) + "}";
    }
    json += "]}";
    return json;
}

std::string CompletionServer::HandleSemanticGotoRequest(const std::string& body) {
    auto& sci = SemanticCodeIntelligence::instance();

    std::string name, file;
    if (!ExtractJsonString(body, "name", name))
        return R"({"error":"missing_symbol_name"})";
    ExtractJsonString(body, "file", file);

    std::string lineStr, colStr;
    uint32_t line = 0, col = 0;
    if (ExtractJsonNumber(body, "line", lineStr)) line = static_cast<uint32_t>(std::stoul(lineStr));
    if (ExtractJsonNumber(body, "column", colStr)) col = static_cast<uint32_t>(std::stoul(colStr));

    SourceLocation ctx = SourceLocation::make(file, line, col);
    const auto* sym = sci.goToDefinition(name, ctx);

    if (!sym)
        return "{\"found\":false,\"name\":\"" + name + "\"}";

    return "{\"found\":true,\"symbolId\":" + std::to_string(sym->symbolId) +
           ",\"name\":\"" + sym->name +
           "\",\"qualifiedName\":\"" + sym->qualifiedName +
           "\",\"kind\":" + std::to_string(static_cast<int>(sym->kind)) +
           ",\"file\":\"" + sym->definition.filePath +
           "\",\"line\":" + std::to_string(sym->definition.line) +
           ",\"column\":" + std::to_string(sym->definition.column) +
           ",\"signature\":\"" + sym->signature + "\"}";
}

std::string CompletionServer::HandleSemanticReferencesRequest(const std::string& body) {
    auto& sci = SemanticCodeIntelligence::instance();

    std::string symIdStr;
    if (!ExtractJsonNumber(body, "symbol_id", symIdStr))
        return R"({"error":"missing_symbol_id"})";

    uint64_t symbolId = std::stoull(symIdStr);
    auto refs = sci.findAllReferences(symbolId);

    std::string json = "{\"symbolId\":" + std::to_string(symbolId) +
                       ",\"referenceCount\":" + std::to_string(refs.size()) +
                       ",\"references\":[";
    for (size_t i = 0; i < refs.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"file\":\"" + refs[i].filePath +
                "\",\"line\":" + std::to_string(refs[i].line) +
                ",\"column\":" + std::to_string(refs[i].column) + "}";
    }
    json += "]}";
    return json;
}

// ═══════════════════════════════════════════════════════════════════════════
// Phase 17: Enterprise Telemetry & Compliance Handlers
// ═══════════════════════════════════════════════════════════════════════════

std::string CompletionServer::HandleTelemetryStatusRequest() {
    auto& etc = EnterpriseTelemetryCompliance::instance();
    const auto& s = etc.stats();

    return "{\"phase\":17,\"name\":\"Enterprise Telemetry & Compliance\","
           "\"telemetryLevel\":" + std::to_string(static_cast<int>(etc.getTelemetryLevel())) +
           ",\"licenseTier\":" + std::to_string(static_cast<int>(etc.getCurrentTier())) +
           ",\"stats\":{"
           "\"totalSpans\":" + std::to_string(s.totalSpans.load()) +
           ",\"activeSpans\":" + std::to_string(s.activeSpans.load()) +
           ",\"completedSpans\":" + std::to_string(s.completedSpans.load()) +
           ",\"droppedSpans\":" + std::to_string(s.droppedSpans.load()) +
           ",\"auditEntries\":" + std::to_string(s.auditEntries.load()) +
           ",\"policyViolations\":" + std::to_string(s.policyViolations.load()) +
           ",\"licenseChecks\":" + std::to_string(s.licenseChecks.load()) +
           ",\"metricsRecorded\":" + std::to_string(s.metricsRecorded.load()) +
           ",\"exportsCompleted\":" + std::to_string(s.exportsCompleted.load()) +
           "}}";
}

std::string CompletionServer::HandleTelemetryAuditRequest(const std::string& body) {
    auto& etc = EnterpriseTelemetryCompliance::instance();

    // POST = record audit entry, GET = query
    std::string actor, resource, action, detail;
    if (ExtractJsonString(body, "actor", actor) &&
        ExtractJsonString(body, "action", action)) {
        // Recording a new audit entry
        ExtractJsonString(body, "resource", resource);
        ExtractJsonString(body, "detail", detail);

        uint64_t id = etc.recordAudit(AuditEventType::ConfigChange,
                                       actor, resource, action, detail);
        return "{\"recorded\":true,\"entryId\":" + std::to_string(id) + "}";
    }

    // Query mode — return recent audit entries
    std::string maxStr;
    uint32_t maxResults = 100;
    if (ExtractJsonNumber(body, "max_results", maxStr))
        maxResults = static_cast<uint32_t>(std::stoul(maxStr));

    auto entries = etc.queryAudit(AuditEventType::SystemStart, 0, maxResults);
    // queryAudit filters by type; for "all" we just get what's available
    // Use audit count as a proxy
    uint64_t auditCount = etc.getAuditCount();

    std::string json = "{\"totalAuditEntries\":" + std::to_string(auditCount) +
                       ",\"returned\":" + std::to_string(entries.size()) +
                       ",\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) json += ",";
        const auto& e = entries[i];
        json += "{\"entryId\":" + std::to_string(e.entryId) +
                ",\"eventType\":" + std::to_string(static_cast<int>(e.eventType)) +
                ",\"actor\":\"" + e.actor +
                "\",\"resource\":\"" + e.resource +
                "\",\"action\":\"" + e.action +
                "\",\"detail\":\"" + e.detail +
                "\",\"severity\":" + std::to_string(e.severity) +
                ",\"tamperSealed\":" + (e.tamperSealed ? "true" : "false") + "}";
    }
    json += "]}";
    return json;
}

std::string CompletionServer::HandleTelemetryComplianceRequest() {
    auto& etc = EnterpriseTelemetryCompliance::instance();
    auto violations = etc.getViolations(0, true);

    PatchResult integrity = etc.verifyAuditIntegrity();

    std::string json = "{\"auditIntegrity\":" +
                       std::string(integrity.success ? "true" : "false") +
                       ",\"integrityDetail\":\"" +
                       std::string(integrity.detail ? integrity.detail : "") +
                       "\",\"unresolvedViolations\":" +
                       std::to_string(violations.size()) +
                       ",\"violations\":[";
    for (size_t i = 0; i < violations.size(); ++i) {
        if (i > 0) json += ",";
        const auto& v = violations[i];
        json += "{\"violationId\":" + std::to_string(v.violationId) +
                ",\"policyId\":" + std::to_string(v.policyId) +
                ",\"description\":\"" + v.description +
                "\",\"resolved\":" + (v.resolved ? "true" : "false") + "}";
    }
    json += "]}";
    return json;
}

std::string CompletionServer::HandleTelemetryLicenseRequest() {
    auto& etc = EnterpriseTelemetryCompliance::instance();
    PatchResult valid = etc.validateLicense();
    LicenseTier tier = etc.getCurrentTier();
    UsageMeter usage = etc.getUsageMeter();

    const char* tierName = "Community";
    switch (tier) {
        case LicenseTier::Professional: tierName = "Professional"; break;
        case LicenseTier::Enterprise:   tierName = "Enterprise"; break;
        case LicenseTier::OEM:          tierName = "OEM"; break;
        default: break;
    }

    return "{\"licenseValid\":" + std::string(valid.success ? "true" : "false") +
           ",\"tier\":\"" + tierName +
           "\",\"usage\":{"
           "\"inferenceCount\":" + std::to_string(usage.inferenceCount.load()) +
           ",\"tokensProcessed\":" + std::to_string(usage.tokensProcessed.load()) +
           ",\"modelsLoaded\":" + std::to_string(usage.modelsLoaded.load()) +
           ",\"patchesApplied\":" + std::to_string(usage.patchesApplied.load()) +
           ",\"apiCallCount\":" + std::to_string(usage.apiCallCount.load()) +
           ",\"bytesTransferred\":" + std::to_string(usage.bytesTransferred.load()) +
           ",\"activeUsers\":" + std::to_string(usage.activeUsers.load()) +
           "}}";
}

std::string CompletionServer::HandleTelemetryMetricsRequest() {
    auto& etc = EnterpriseTelemetryCompliance::instance();
    auto metrics = etc.getMetrics("");

    std::string json = "{\"metricCount\":" + std::to_string(metrics.size()) +
                       ",\"metrics\":[";
    for (size_t i = 0; i < metrics.size() && i < 500; ++i) {
        if (i > 0) json += ",";
        const auto& m = metrics[i];
        json += "{\"name\":\"" + m.name +
                "\",\"type\":" + std::to_string(static_cast<int>(m.type)) +
                ",\"value\":" + std::to_string(m.value) +
                ",\"unit\":\"" + m.unit + "\"}";
    }
    json += "]}";
    return json;
}

std::string CompletionServer::HandleTelemetryExportRequest(const std::string& body) {
    auto& etc = EnterpriseTelemetryCompliance::instance();

    std::string format, outputPath;
    ExtractJsonString(body, "format", format);
    ExtractJsonString(body, "output_path", outputPath);

    if (outputPath.empty())
        outputPath = "RawrXD_TelemetryExport.json";

    PatchResult r;
    if (format == "otlp" || format == "opentelemetry")
        r = etc.exportTelemetryOTLP(outputPath.c_str());
    else
        r = etc.exportAuditLog(outputPath.c_str());

    return "{\"success\":" + std::string(r.success ? "true" : "false") +
           ",\"format\":\"" + (format.empty() ? "audit_log" : format) +
           "\",\"outputPath\":\"" + outputPath +
           "\",\"detail\":\"" + std::string(r.detail ? r.detail : "") + "\"}";
}

// ============================================================================
// Phase 26 — ReverseEngineered Kernel API Handlers
// ============================================================================

std::string CompletionServer::HandleSchedulerStatusRequest() {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    uint64_t tick = GetHighResTick();
    uint64_t us = TicksToMicroseconds(tick);
    return "{\"subsystem\":\"work_stealing_scheduler\""
           ",\"status\":\"active\""
           ",\"numa_aware\":true"
           ",\"deadlock_detection\":true"
           ",\"timer_tick\":" + std::to_string(tick) +
           ",\"timer_us\":" + std::to_string(us) + "}";
#else
    return R"({"subsystem":"work_stealing_scheduler","status":"disabled","reason":"no_masm"})";
#endif
}

std::string CompletionServer::HandleSchedulerSubmitRequest(const std::string& body) {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    std::string priority_str;
    uint32_t priority = 0;
    if (ExtractJsonNumber(body, "priority", priority_str)) {
        priority = static_cast<uint32_t>(std::stoul(priority_str));
    }

    // Submit a no-op benchmark task to measure scheduling overhead
    uint64_t t0 = GetHighResTick();
    auto testFn = [](void*) -> void {};
    int64_t taskId = Scheduler_SubmitTask(
        reinterpret_cast<void*>(+testFn), nullptr, priority, 0, nullptr);

    if (taskId < 0) {
        return "{\"success\":false,\"error\":\"submit_failed\""
               ",\"code\":" + std::to_string(taskId) + "}";
    }

    void* result = Scheduler_WaitForTask(taskId, 5000);
    uint64_t elapsed = TicksToMicroseconds(GetHighResTick() - t0);

    return "{\"success\":true"
           ",\"task_id\":" + std::to_string(taskId) +
           ",\"completed\":" + (result ? "true" : "false") +
           ",\"latency_us\":" + std::to_string(elapsed) +
           ",\"priority\":" + std::to_string(priority) + "}";
#else
    return R"({"success":false,"error":"masm_disabled"})";
#endif
}

std::string CompletionServer::HandleConflictStatusRequest() {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    return "{\"subsystem\":\"conflict_detector\""
           ",\"status\":\"active\""
           ",\"algorithm\":\"wait_for_graph_dfs\""
           ",\"scan_interval_ms\":1000"
           ",\"max_resources\":256}";
#else
    return R"({"subsystem":"conflict_detector","status":"disabled"})";
#endif
}

std::string CompletionServer::HandleHeartbeatStatusRequest() {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    return "{\"subsystem\":\"heartbeat_monitor\""
           ",\"status\":\"active\""
           ",\"protocol\":\"udp_gossip\""
           ",\"magic\":\"RHBT\""
           ",\"send_interval_ms\":500}";
#else
    return R"({"subsystem":"heartbeat_monitor","status":"disabled"})";
#endif
}

std::string CompletionServer::HandleHeartbeatAddRequest(const std::string& body) {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    std::string ip, port_str, id_str;
    ExtractJsonString(body, "ip", ip);
    ExtractJsonNumber(body, "port", port_str);
    ExtractJsonNumber(body, "node_id", id_str);

    if (ip.empty() || port_str.empty()) {
        return R"({"success":false,"error":"missing ip or port"})";
    }

    uint16_t port = static_cast<uint16_t>(std::stoul(port_str));
    uint32_t nodeId = id_str.empty() ? 1 : static_cast<uint32_t>(std::stoul(id_str));

    int rc = Heartbeat_AddNode(nodeId, ip.c_str(), port);
    return "{\"success\":" + std::string(rc == 0 ? "true" : "false") +
           ",\"node_id\":" + std::to_string(nodeId) +
           ",\"ip\":\"" + ip +
           "\",\"port\":" + std::to_string(port) + "}";
#else
    return R"({"success":false,"error":"masm_disabled"})";
#endif
}

std::string CompletionServer::HandleGpuDmaStatusRequest() {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    return "{\"subsystem\":\"gpu_dma_engine\""
           ",\"status\":\"ready\""
           ",\"fence_based\":true"
           ",\"dma_allocator\":\"VirtualAlloc\""
           ",\"avx512_matmul\":true"
           ",\"avx2_fallback\":true}";
#else
    return R"({"subsystem":"gpu_dma_engine","status":"disabled"})";
#endif
}

std::string CompletionServer::HandleTensorBenchRequest(const std::string& body) {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    std::string m_str, n_str, k_str, type_str;
    uint32_t M = 64, N = 64, K = 64;
    uint32_t quantType = QUANT_Q8_0;

    if (ExtractJsonNumber(body, "M", m_str)) M = static_cast<uint32_t>(std::stoul(m_str));
    if (ExtractJsonNumber(body, "N", n_str)) N = static_cast<uint32_t>(std::stoul(n_str));
    if (ExtractJsonNumber(body, "K", k_str)) K = static_cast<uint32_t>(std::stoul(k_str));
    if (ExtractJsonNumber(body, "quant_type", type_str)) quantType = static_cast<uint32_t>(std::stoul(type_str));

    // Clamp to reasonable sizes for API benchmark
    if (M > 1024) M = 1024;
    if (N > 1024) N = 1024;
    if (K > 1024) K = 1024;

    void* A = AllocateDMABuffer(static_cast<uint64_t>(M) * K);
    void* B = AllocateDMABuffer(static_cast<uint64_t>(K) * N);
    float* C = static_cast<float*>(AllocateDMABuffer(static_cast<uint64_t>(M) * N * sizeof(float)));

    if (!A || !B || !C) {
        if (A) VirtualFree(A, 0, MEM_RELEASE);
        if (B) VirtualFree(B, 0, MEM_RELEASE);
        if (C) VirtualFree(C, 0, MEM_RELEASE);
        return R"({"success":false,"error":"dma_alloc_failed"})";
    }

    memset(A, 1, static_cast<size_t>(M) * K);
    memset(B, 1, static_cast<size_t>(K) * N);

    uint64_t t0 = GetHighResTick();
    Tensor_QuantizedMatMul(A, B, C, M, N, K, quantType);
    uint64_t elapsed = TicksToMicroseconds(GetHighResTick() - t0);

    float c00 = C[0];
    float cLast = C[static_cast<size_t>(M-1) * N + (N-1)];

    VirtualFree(A, 0, MEM_RELEASE);
    VirtualFree(B, 0, MEM_RELEASE);
    VirtualFree(C, 0, MEM_RELEASE);

    // Compute GFLOPS: 2*M*N*K FLOPs / elapsed_us * 1e6 / 1e9
    double gflops = 0.0;
    if (elapsed > 0) {
        gflops = (2.0 * M * N * K) / (static_cast<double>(elapsed) * 1000.0);
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"success\":true,\"M\":%u,\"N\":%u,\"K\":%u"
             ",\"quant_type\":%u,\"latency_us\":%llu"
             ",\"gflops\":%.3f,\"C_0_0\":%.1f,\"C_last\":%.1f}",
             M, N, K, quantType,
             (unsigned long long)elapsed, gflops, c00, cLast);
    return std::string(buf);
#else
    return R"({"success":false,"error":"masm_disabled"})";
#endif
}

std::string CompletionServer::HandleTimerRequest() {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    uint64_t tick = GetHighResTick();
    uint64_t us = TicksToMicroseconds(tick);
    uint64_t ms = TicksToMilliseconds(tick);
    return "{\"tick\":" + std::to_string(tick) +
           ",\"microseconds\":" + std::to_string(us) +
           ",\"milliseconds\":" + std::to_string(ms) + "}";
#else
    return R"({"error":"masm_disabled"})";
#endif
}

std::string CompletionServer::HandleCrc32Request(const std::string& body) {
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    std::string data;
    ExtractJsonString(body, "data", data);
    if (data.empty()) {
        return R"({"success":false,"error":"missing data field"})";
    }

    uint32_t crc = CalculateCRC32(data.c_str(), data.size());

    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"success\":true,\"crc32\":\"0x%08X\",\"crc32_dec\":%u,\"input_len\":%zu}",
             crc, crc, data.size());
    return std::string(buf);
#else
    return R"({"success":false,"error":"masm_disabled"})";
#endif
}

} // namespace RawrXD
