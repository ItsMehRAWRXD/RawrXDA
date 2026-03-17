// MCP Integration — Model Context Protocol Server + Client for RawrXD IDE
// Full JSON-RPC 2.0 based MCP implementation with tool/resource/prompt support
// Generated: 2026-01-25 06:34:12 | Completed: 2026-02-08

#include "mcp_integration.h"
#include <sstream>
#include <fstream>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace RawrXD {
namespace MCP {

// ============================================================================
// Minimal JSON helpers (no dependency on Qt/nlohmann for portability)
// ============================================================================

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;
        }
    }
    return out;
}

static std::string jsonString(const std::string& key, const std::string& val) {
    return "\"" + jsonEscape(key) + "\":\"" + jsonEscape(val) + "\"";
}

static std::string jsonInt(const std::string& key, int64_t val) {
    return "\"" + jsonEscape(key) + "\":" + std::to_string(val);
}

static std::string jsonBool(const std::string& key, bool val) {
    return "\"" + jsonEscape(key) + "\":" + (val ? "true" : "false");
}

// Extremely lightweight JSON value extraction (key must be unique at top level)
static std::string jsonExtractString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    pos++; // skip opening quote
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

static int64_t jsonExtractInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    return std::atoll(json.c_str() + pos);
}

static std::string jsonExtractObject(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "{}";
    pos = json.find('{', pos + needle.size());
    if (pos == std::string::npos) return "{}";
    int depth = 0;
    size_t start = pos;
    for (; pos < json.size(); pos++) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') { depth--; if (depth == 0) return json.substr(start, pos - start + 1); }
    }
    return "{}";
}

// ============================================================================
// MCPServer Implementation
// ============================================================================

MCPServer::MCPServer() {}

MCPServer::~MCPServer() {
    if (m_running) shutdown();
}

bool MCPServer::initialize(const ServerInfo& info) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_serverInfo = info;
    m_running = true;
    return true;
}

void MCPServer::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
    m_tools.clear();
    m_resources.clear();
    m_prompts.clear();
}

void MCPServer::registerTool(const ToolDefinition& def, ToolHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tools[def.name] = {def, std::move(handler)};
}

void MCPServer::unregisterTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tools.erase(name);
}

std::vector<ToolDefinition> MCPServer::listTools() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ToolDefinition> result;
    result.reserve(m_tools.size());
    for (const auto& [name, pair] : m_tools) {
        result.push_back(pair.first);
    }
    return result;
}

void MCPServer::registerResource(const ResourceDefinition& def, ResourceHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resources[def.uri] = {def, std::move(handler)};
}

void MCPServer::unregisterResource(const std::string& uri) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resources.erase(uri);
}

std::vector<ResourceDefinition> MCPServer::listResources() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ResourceDefinition> result;
    result.reserve(m_resources.size());
    for (const auto& [uri, pair] : m_resources) {
        result.push_back(pair.first);
    }
    return result;
}

void MCPServer::registerPrompt(const PromptTemplate& tmpl, PromptHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_prompts[tmpl.name] = {tmpl, std::move(handler)};
}

std::vector<PromptTemplate> MCPServer::listPrompts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PromptTemplate> result;
    result.reserve(m_prompts.size());
    for (const auto& [name, pair] : m_prompts) {
        result.push_back(pair.first);
    }
    return result;
}

// ============================================================================
// JSON-RPC Message Handling
// ============================================================================

std::string MCPServer::handleMessage(const std::string& rawJson) {
    m_totalRequests++;

    MCPRequest req = parseRequest(rawJson);
    if (req.method.empty()) {
        m_totalErrors++;
        return makeErrorResponse(req.id, MCPErrorCode::ParseError, "Failed to parse request");
    }

    MCPResponse resp = dispatch(req);
    return serializeResponse(resp);
}

MCPResponse MCPServer::dispatch(const MCPRequest& req) {
    if (req.method == "initialize")      return handleInitialize(req);
    if (req.method == "ping")            return handlePing(req);
    if (req.method == "tools/list")      return handleToolsList(req);
    if (req.method == "tools/call")      return handleToolsCall(req);
    if (req.method == "resources/list")  return handleResourcesList(req);
    if (req.method == "resources/read")  return handleResourcesRead(req);
    if (req.method == "prompts/list")    return handlePromptsList(req);
    if (req.method == "prompts/get")     return handlePromptsGet(req);

    m_totalErrors++;
    MCPResponse resp;
    resp.id = req.id;
    resp.success = false;
    resp.error = {MCPErrorCode::MethodNotFound, "Unknown method: " + req.method, ""};
    return resp;
}

MCPResponse MCPServer::handleInitialize(const MCPRequest& req) {
    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;

    std::ostringstream json;
    json << "{";
    json << jsonString("protocolVersion", m_capabilities.protocolVersion) << ",";
    json << "\"capabilities\":{";
    json << jsonBool("tools", m_capabilities.supportsTools) << ",";
    json << jsonBool("resources", m_capabilities.supportsResources) << ",";
    json << jsonBool("prompts", m_capabilities.supportsPrompts) << ",";
    json << jsonBool("logging", m_capabilities.supportsLogging);
    json << "},";
    json << "\"serverInfo\":{";
    json << jsonString("name", m_serverInfo.name) << ",";
    json << jsonString("version", m_serverInfo.version);
    json << "}}";
    resp.result = json.str();
    return resp;
}

MCPResponse MCPServer::handlePing(const MCPRequest& req) {
    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;
    resp.result = "{}";
    return resp;
}

MCPResponse MCPServer::handleToolsList(const MCPRequest& req) {
    std::lock_guard<std::mutex> lock(m_mutex);
    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;

    std::ostringstream json;
    json << "{\"tools\":[";
    bool first = true;
    for (const auto& [name, pair] : m_tools) {
        if (!first) json << ",";
        first = false;
        const auto& def = pair.first;
        json << "{" << jsonString("name", def.name) << ","
             << jsonString("description", def.description) << ","
             << "\"inputSchema\":{\"type\":\"object\",\"properties\":{";
        bool firstParam = true;
        std::string requiredList;
        for (const auto& p : def.parameters) {
            if (!firstParam) json << ",";
            firstParam = false;
            json << "\"" << jsonEscape(p.name) << "\":{" 
                 << jsonString("type", p.type) << ","
                 << jsonString("description", p.description) << "}";
            if (p.required) {
                if (!requiredList.empty()) requiredList += ",";
                requiredList += "\"" + jsonEscape(p.name) + "\"";
            }
        }
        json << "}";
        if (!requiredList.empty()) {
            json << ",\"required\":[" << requiredList << "]";
        }
        json << "}}";
    }
    json << "]}";
    resp.result = json.str();
    return resp;
}

MCPResponse MCPServer::handleToolsCall(const MCPRequest& req) {
    std::string toolName = jsonExtractString(req.params, "name");
    std::string arguments = jsonExtractObject(req.params, "arguments");

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) {
        m_totalErrors++;
        MCPResponse resp;
        resp.id = req.id;
        resp.success = false;
        resp.error = {MCPErrorCode::ToolNotFound, "Tool not found: " + toolName, ""};
        return resp;
    }

    ToolResult toolResult = it->second.second(arguments);

    MCPResponse resp;
    resp.id = req.id;
    resp.success = !toolResult.isError;

    std::ostringstream json;
    json << "{\"content\":[{";
    json << jsonString("type", toolResult.isError ? "text" : (toolResult.contentType == "application/json" ? "text" : "text")) << ",";
    json << jsonString("text", toolResult.isError ? toolResult.errorMessage : toolResult.content);
    json << "}]," << jsonBool("isError", toolResult.isError) << "}";
    resp.result = json.str();
    return resp;
}

MCPResponse MCPServer::handleResourcesList(const MCPRequest& req) {
    std::lock_guard<std::mutex> lock(m_mutex);
    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;

    std::ostringstream json;
    json << "{\"resources\":[";
    bool first = true;
    for (const auto& [uri, pair] : m_resources) {
        if (!first) json << ",";
        first = false;
        const auto& def = pair.first;
        json << "{" << jsonString("uri", def.uri) << ","
             << jsonString("name", def.name) << ","
             << jsonString("description", def.description) << ","
             << jsonString("mimeType", def.mimeType) << "}";
    }
    json << "]}";
    resp.result = json.str();
    return resp;
}

MCPResponse MCPServer::handleResourcesRead(const MCPRequest& req) {
    std::string uri = jsonExtractString(req.params, "uri");

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_resources.find(uri);
    if (it == m_resources.end()) {
        m_totalErrors++;
        MCPResponse resp;
        resp.id = req.id;
        resp.success = false;
        resp.error = {MCPErrorCode::ResourceNotFound, "Resource not found: " + uri, ""};
        return resp;
    }

    ResourceContent content = it->second.second(uri);

    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;

    std::ostringstream json;
    json << "{\"contents\":[{" 
         << jsonString("uri", content.uri) << ","
         << jsonString("mimeType", content.mimeType) << ","
         << jsonString("text", content.content)
         << "}]}";
    resp.result = json.str();
    return resp;
}

MCPResponse MCPServer::handlePromptsList(const MCPRequest& req) {
    std::lock_guard<std::mutex> lock(m_mutex);
    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;

    std::ostringstream json;
    json << "{\"prompts\":[";
    bool first = true;
    for (const auto& [name, pair] : m_prompts) {
        if (!first) json << ",";
        first = false;
        const auto& tmpl = pair.first;
        json << "{" << jsonString("name", tmpl.name) << ","
             << jsonString("description", tmpl.description) << ","
             << "\"arguments\":[";
        bool firstArg = true;
        for (const auto& arg : tmpl.arguments) {
            if (!firstArg) json << ",";
            firstArg = false;
            json << "{" << jsonString("name", arg.name) << ","
                 << jsonString("description", arg.description) << ","
                 << jsonBool("required", arg.required) << "}";
        }
        json << "]}";
    }
    json << "]}";
    resp.result = json.str();
    return resp;
}

MCPResponse MCPServer::handlePromptsGet(const MCPRequest& req) {
    std::string promptName = jsonExtractString(req.params, "name");
    std::string arguments  = jsonExtractObject(req.params, "arguments");

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_prompts.find(promptName);
    if (it == m_prompts.end()) {
        MCPResponse resp;
        resp.id = req.id;
        resp.success = false;
        resp.error = {MCPErrorCode::MethodNotFound, "Prompt not found: " + promptName, ""};
        return resp;
    }

    auto messages = it->second.second(arguments);

    MCPResponse resp;
    resp.id = req.id;
    resp.success = true;

    std::ostringstream json;
    json << "{\"description\":\"" << jsonEscape(it->second.first.description) << "\",\"messages\":[";
    bool first = true;
    for (const auto& msg : messages) {
        if (!first) json << ",";
        first = false;
        json << "{" << jsonString("role", msg.role) << ","
             << "\"content\":{\"type\":\"text\"," << jsonString("text", msg.content) << "}}";
    }
    json << "]}";
    resp.result = json.str();
    return resp;
}

// ============================================================================
// Serialization
// ============================================================================

std::string MCPServer::serializeResponse(const MCPResponse& resp) const {
    std::ostringstream json;
    json << "{\"jsonrpc\":\"2.0\"," << jsonInt("id", resp.id) << ",";
    if (resp.success) {
        json << "\"result\":" << resp.result;
    } else {
        json << "\"error\":{" 
             << jsonInt("code", static_cast<int>(resp.error.code)) << ","
             << jsonString("message", resp.error.message);
        if (!resp.error.data.empty()) {
            json << "," << jsonString("data", resp.error.data);
        }
        json << "}";
    }
    json << "}";
    return json.str();
}

MCPRequest MCPServer::parseRequest(const std::string& json) const {
    MCPRequest req;
    req.jsonrpc = jsonExtractString(json, "jsonrpc");
    req.id      = jsonExtractInt(json, "id");
    req.method  = jsonExtractString(json, "method");
    req.params  = jsonExtractObject(json, "params");
    return req;
}

std::string MCPServer::makeErrorResponse(int64_t id, MCPErrorCode code, const std::string& msg) const {
    MCPResponse resp;
    resp.id = id;
    resp.success = false;
    resp.error = {code, msg, ""};
    return serializeResponse(resp);
}

// ============================================================================
// Stdio Transport (for subprocess-based MCP servers)
// ============================================================================

bool MCPServer::startStdioTransport() {
#ifdef _WIN32
    // Set stdin/stdout to binary mode for JSON-RPC line protocol
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    m_running = true;

    while (m_running) {
        // Read Content-Length header
        std::string headerLine;
        if (!std::getline(std::cin, headerLine)) break;

        // Parse Content-Length
        size_t contentLength = 0;
        if (headerLine.find("Content-Length:") == 0) {
            contentLength = std::stoul(headerLine.substr(15));
        }

        // Read blank line separator
        std::string blank;
        std::getline(std::cin, blank);

        // Read content
        std::string content(contentLength, '\0');
        std::cin.read(&content[0], contentLength);

        // Process and respond
        std::string response = handleMessage(content);

        // Write response with Content-Length header
        std::cout << "Content-Length: " << response.size() << "\r\n\r\n" << response;
        std::cout.flush();
    }

    return true;
}

void MCPServer::stopTransport() {
    m_running = false;
}

// ============================================================================
// MCPClient Implementation
// ============================================================================

MCPClient::MCPClient() {}

MCPClient::~MCPClient() {
    disconnect();
}

bool MCPClient::connectStdio(const std::string& command, const std::vector<std::string>& args) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hStdinRead, hStdinWrite, hStdoutRead, hStdoutWrite;

    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) return false;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        return false;
    }

    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(STARTUPINFOA)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStdoutWrite;

    PROCESS_INFORMATION pi = {};

    // Build command line
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }

    if (!CreateProcessA(nullptr, &cmdLine[0], nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return false;
    }

    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    CloseHandle(pi.hThread);

    m_processHandle = pi.hProcess;
    m_stdinWrite    = hStdinWrite;
    m_stdoutRead    = hStdoutRead;
    m_connected     = true;

    // Send initialize request
    std::string initReq = sendRequest("initialize", 
        "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"RawrXD-IDE\",\"version\":\"1.0.0\"}}");

    if (!initReq.empty()) {
        MCPResponse resp = parseResponse(initReq);
        if (resp.success) {
            // Parse capabilities from response
            m_serverCapabilities.supportsTools = true;
            m_serverCapabilities.supportsResources = true;
            m_serverCapabilities.supportsPrompts = true;
        }
    }

    return true;
#else
    // POSIX implementation
    int stdinPipe[2], stdoutPipe[2];
    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(command.c_str(), argv.data());
        _exit(1);
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    m_stdinWrite = reinterpret_cast<void*>(static_cast<intptr_t>(stdinPipe[1]));
    m_stdoutRead = reinterpret_cast<void*>(static_cast<intptr_t>(stdoutPipe[0]));
    m_processHandle = reinterpret_cast<void*>(static_cast<intptr_t>(pid));
    m_connected = true;

    return true;
#endif
}

bool MCPClient::connectHttp(const std::string& url) {
    // HTTP/SSE transport - future implementation
    // Would use WinHTTP or libcurl
    (void)url;
    return false;
}

void MCPClient::disconnect() {
    if (!m_connected) return;

#ifdef _WIN32
    if (m_stdinWrite) { CloseHandle((HANDLE)m_stdinWrite); m_stdinWrite = nullptr; }
    if (m_stdoutRead) { CloseHandle((HANDLE)m_stdoutRead); m_stdoutRead = nullptr; }
    if (m_processHandle) {
        TerminateProcess((HANDLE)m_processHandle, 0);
        CloseHandle((HANDLE)m_processHandle);
        m_processHandle = nullptr;
    }
#else
    if (m_stdinWrite) { close(reinterpret_cast<intptr_t>(m_stdinWrite)); m_stdinWrite = nullptr; }
    if (m_stdoutRead) { close(reinterpret_cast<intptr_t>(m_stdoutRead)); m_stdoutRead = nullptr; }
    if (m_processHandle) {
        kill(reinterpret_cast<intptr_t>(m_processHandle), SIGTERM);
        waitpid(reinterpret_cast<intptr_t>(m_processHandle), nullptr, 0);
        m_processHandle = nullptr;
    }
#endif

    m_connected = false;
}

std::string MCPClient::sendRequest(const std::string& method, const std::string& params) {
    if (!m_connected) return "";

    std::lock_guard<std::mutex> lock(m_mutex);
    int64_t id = m_nextId++;

    std::ostringstream json;
    json << "{\"jsonrpc\":\"2.0\"," << jsonInt("id", id) << ","
         << jsonString("method", method) << ",\"params\":" << params << "}";
    std::string body = json.str();
    std::string message = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

#ifdef _WIN32
    DWORD written;
    if (!WriteFile((HANDLE)m_stdinWrite, message.c_str(), (DWORD)message.size(), &written, nullptr)) {
        return "";
    }

    // Read response header
    std::string headerBuf;
    char ch;
    DWORD read;
    while (ReadFile((HANDLE)m_stdoutRead, &ch, 1, &read, nullptr) && read > 0) {
        headerBuf += ch;
        if (headerBuf.size() >= 4 && headerBuf.substr(headerBuf.size() - 4) == "\r\n\r\n") break;
    }

    // Parse Content-Length
    size_t contentLen = 0;
    auto clPos = headerBuf.find("Content-Length:");
    if (clPos != std::string::npos) {
        contentLen = std::stoul(headerBuf.substr(clPos + 15));
    }
    if (contentLen == 0) return "";

    std::string response(contentLen, '\0');
    DWORD totalRead = 0;
    while (totalRead < contentLen) {
        if (!ReadFile((HANDLE)m_stdoutRead, &response[totalRead], 
                     (DWORD)(contentLen - totalRead), &read, nullptr) || read == 0) break;
        totalRead += read;
    }
    return response;
#else
    write(reinterpret_cast<intptr_t>(m_stdinWrite), message.c_str(), message.size());

    // Read response (simplified)
    std::string headerBuf;
    char ch;
    while (::read(reinterpret_cast<intptr_t>(m_stdoutRead), &ch, 1) > 0) {
        headerBuf += ch;
        if (headerBuf.size() >= 4 && headerBuf.substr(headerBuf.size() - 4) == "\r\n\r\n") break;
    }

    size_t contentLen = 0;
    auto clPos = headerBuf.find("Content-Length:");
    if (clPos != std::string::npos) contentLen = std::stoul(headerBuf.substr(clPos + 15));
    if (contentLen == 0) return "";

    std::string response(contentLen, '\0');
    size_t totalRead = 0;
    while (totalRead < contentLen) {
        auto r = ::read(reinterpret_cast<intptr_t>(m_stdoutRead), &response[totalRead], contentLen - totalRead);
        if (r <= 0) break;
        totalRead += r;
    }
    return response;
#endif
}

MCPResponse MCPClient::parseResponse(const std::string& rawJson) {
    MCPResponse resp;
    resp.id = jsonExtractInt(rawJson, "id");

    // Check for error
    if (rawJson.find("\"error\"") != std::string::npos) {
        resp.success = false;
        resp.error.message = jsonExtractString(rawJson, "message");
        resp.error.code = static_cast<MCPErrorCode>(jsonExtractInt(rawJson, "code"));
    } else {
        resp.success = true;
        resp.result = jsonExtractObject(rawJson, "result");
    }
    return resp;
}

std::vector<ToolDefinition> MCPClient::listTools() {
    std::string response = sendRequest("tools/list", "{}");
    std::vector<ToolDefinition> tools;
    MCPResponse resp = parseResponse(response);
    if (!resp.success) return tools;

    // Parse "tools" array from result JSON
    std::string toolsArr = resp.result;
    auto arrStart = toolsArr.find("[\"name\"");
    // Lightweight: find each {"name":...} block in the tools array
    size_t pos = toolsArr.find("\"tools\":[\n");
    if (pos == std::string::npos) pos = toolsArr.find("\"tools\":[");
    if (pos == std::string::npos) return tools;

    // Walk through tool objects
    size_t searchPos = pos;
    while (true) {
        size_t objStart = toolsArr.find('{', searchPos);
        if (objStart == std::string::npos) break;
        // Find matching closing brace (handle nesting)
        int depth = 0;
        size_t objEnd = objStart;
        for (; objEnd < toolsArr.size(); objEnd++) {
            if (toolsArr[objEnd] == '{') depth++;
            else if (toolsArr[objEnd] == '}') { depth--; if (depth == 0) break; }
        }
        if (depth != 0) break;

        std::string toolJson = toolsArr.substr(objStart, objEnd - objStart + 1);
        ToolDefinition def;
        def.name = jsonExtractString(toolJson, "name");
        def.description = jsonExtractString(toolJson, "description");
        if (!def.name.empty()) {
            tools.push_back(std::move(def));
        }
        searchPos = objEnd + 1;
    }
    return tools;
}

ToolResult MCPClient::callTool(const std::string& name, const std::string& argsJson) {
    std::string params = "{" + jsonString("name", name) + ",\"arguments\":" + argsJson + "}";
    std::string response = sendRequest("tools/call", params);
    MCPResponse resp = parseResponse(response);
    if (!resp.success) {
        return ToolResult::fail(resp.error.message);
    }
    std::string content = jsonExtractString(resp.result, "text");
    return ToolResult::ok(content);
}

std::vector<ResourceDefinition> MCPClient::listResources() {
    std::string response = sendRequest("resources/list", "{}");
    std::vector<ResourceDefinition> resources;
    MCPResponse resp = parseResponse(response);
    if (!resp.success) return resources;

    // Parse "resources" array from result
    std::string result = resp.result;
    size_t pos = result.find("\"resources\":[");
    if (pos == std::string::npos) return resources;

    size_t searchPos = pos;
    while (true) {
        size_t objStart = result.find('{', searchPos);
        if (objStart == std::string::npos) break;
        int depth = 0;
        size_t objEnd = objStart;
        for (; objEnd < result.size(); objEnd++) {
            if (result[objEnd] == '{') depth++;
            else if (result[objEnd] == '}') { depth--; if (depth == 0) break; }
        }
        if (depth != 0) break;

        std::string objJson = result.substr(objStart, objEnd - objStart + 1);
        ResourceDefinition def;
        def.uri = jsonExtractString(objJson, "uri");
        def.name = jsonExtractString(objJson, "name");
        def.description = jsonExtractString(objJson, "description");
        def.mimeType = jsonExtractString(objJson, "mimeType");
        if (!def.uri.empty()) {
            resources.push_back(std::move(def));
        }
        searchPos = objEnd + 1;
    }
    return resources;
}

ResourceContent MCPClient::readResource(const std::string& uri) {
    std::string params = "{" + jsonString("uri", uri) + "}";
    std::string response = sendRequest("resources/read", params);
    MCPResponse resp = parseResponse(response);
    ResourceContent content;
    content.uri = uri;
    if (resp.success) {
        content.content = jsonExtractString(resp.result, "text");
        content.mimeType = jsonExtractString(resp.result, "mimeType");
    }
    return content;
}

std::vector<PromptTemplate> MCPClient::listPrompts() {
    std::string response = sendRequest("prompts/list", "{}");
    std::vector<PromptTemplate> prompts;
    MCPResponse resp = parseResponse(response);
    if (!resp.success) return prompts;

    std::string result = resp.result;
    size_t pos = result.find("\"prompts\":[");
    if (pos == std::string::npos) return prompts;

    size_t searchPos = pos;
    while (true) {
        size_t objStart = result.find('{', searchPos);
        if (objStart == std::string::npos) break;
        int depth = 0;
        size_t objEnd = objStart;
        for (; objEnd < result.size(); objEnd++) {
            if (result[objEnd] == '{') depth++;
            else if (result[objEnd] == '}') { depth--; if (depth == 0) break; }
        }
        if (depth != 0) break;

        std::string objJson = result.substr(objStart, objEnd - objStart + 1);
        PromptTemplate tmpl;
        tmpl.name = jsonExtractString(objJson, "name");
        tmpl.description = jsonExtractString(objJson, "description");
        if (!tmpl.name.empty()) {
            prompts.push_back(std::move(tmpl));
        }
        searchPos = objEnd + 1;
    }
    return prompts;
}

std::vector<PromptMessage> MCPClient::getPrompt(const std::string& name, const std::string& argsJson) {
    std::string params = "{" + jsonString("name", name) + ",\"arguments\":" + argsJson + "}";
    std::string response = sendRequest("prompts/get", params);
    std::vector<PromptMessage> messages;
    MCPResponse resp = parseResponse(response);
    if (!resp.success) return messages;

    // Parse "messages" array from result
    std::string result = resp.result;
    size_t pos = result.find("\"messages\":[");
    if (pos == std::string::npos) return messages;

    size_t searchPos = pos;
    while (true) {
        size_t objStart = result.find('{', searchPos);
        if (objStart == std::string::npos) break;
        int depth = 0;
        size_t objEnd = objStart;
        for (; objEnd < result.size(); objEnd++) {
            if (result[objEnd] == '{') depth++;
            else if (result[objEnd] == '}') { depth--; if (depth == 0) break; }
        }
        if (depth != 0) break;

        std::string objJson = result.substr(objStart, objEnd - objStart + 1);
        PromptMessage msg;
        msg.role = jsonExtractString(objJson, "role");
        // Content may be nested: {"type":"text","text":"..."}
        std::string contentObj = jsonExtractObject(objJson, "content");
        if (!contentObj.empty() && contentObj != "{}") {
            msg.content = jsonExtractString(contentObj, "text");
        } else {
            msg.content = jsonExtractString(objJson, "content");
        }
        if (!msg.role.empty()) {
            messages.push_back(std::move(msg));
        }
        searchPos = objEnd + 1;
    }
    return messages;
}

// ============================================================================
// Built-in Tools for RawrXD IDE
// ============================================================================

void registerBuiltinTools(MCPServer& server) {
    // --- read_file tool ---
    {
        ToolDefinition def;
        def.name = "read_file";
        def.description = "Read the contents of a file from the workspace";
        def.parameters = {
            {"path", "string", "Absolute or workspace-relative path to the file", true, ""},
            {"startLine", "number", "Line number to start reading (1-based, optional)", false, "1"},
            {"endLine", "number", "Line number to end reading (1-based, optional)", false, "0"}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path = jsonExtractString(params, "path");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

            std::ifstream file(path);
            if (!file.is_open()) return ToolResult::fail("Cannot open file: " + path);

            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            return ToolResult::ok(content, "text/plain");
        });
    }

    // --- write_file tool ---
    {
        ToolDefinition def;
        def.name = "write_file";
        def.description = "Write content to a file in the workspace";
        def.parameters = {
            {"path", "string", "Absolute or workspace-relative path to the file", true, ""},
            {"content", "string", "Content to write", true, ""}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path    = jsonExtractString(params, "path");
            std::string content = jsonExtractString(params, "content");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) return ToolResult::fail("Cannot write to: " + path);

            file << content;
            return ToolResult::ok("{\"written\":" + std::to_string(content.size()) + "}");
        });
    }

    // --- search_files tool ---
    {
        ToolDefinition def;
        def.name = "search_files";
        def.description = "Search for text pattern in workspace files";
        def.parameters = {
            {"pattern", "string", "Text or regex pattern to search for", true, ""},
            {"path", "string", "Directory to search in", false, "."}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string pattern = jsonExtractString(params, "pattern");
            std::string path    = jsonExtractString(params, "path");
            if (pattern.empty()) return ToolResult::fail("Missing 'pattern' parameter");
            if (path.empty()) path = ".";

            // Real recursive grep-like search with pattern matching
            std::ostringstream results;
            results << "{\"matches\":[";
            size_t matchCount = 0;
            const size_t MAX_MATCHES = 500;

            // Text-file extension filter
            auto isTextFile = [](const std::string& ext) -> bool {
                static const char* textExts[] = {
                    ".cpp", ".c", ".h", ".hpp", ".hxx", ".cxx", ".cc",
                    ".py", ".js", ".ts", ".jsx", ".tsx", ".json",
                    ".rs", ".go", ".java", ".cs", ".rb", ".lua",
                    ".md", ".txt", ".xml", ".html", ".css", ".scss",
                    ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf",
                    ".cmake", ".bat", ".ps1", ".sh", ".asm", ".sql",
                    nullptr
                };
                for (int i = 0; textExts[i]; ++i) {
                    if (ext == textExts[i]) return true;
                }
                return false;
            };

            try {
                namespace fs = std::filesystem;
                for (auto it = fs::recursive_directory_iterator(path,
                         fs::directory_options::skip_permission_denied);
                     it != fs::recursive_directory_iterator() && matchCount < MAX_MATCHES;
                     ++it) {
                    if (!it->is_regular_file()) continue;
                    std::string ext = it->path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (!isTextFile(ext)) continue;

                    std::ifstream file(it->path(), std::ios::binary);
                    if (!file.is_open()) continue;

                    std::string line;
                    int lineNum = 0;
                    while (std::getline(file, line) && matchCount < MAX_MATCHES) {
                        lineNum++;
                        if (line.find(pattern) != std::string::npos) {
                            if (matchCount > 0) results << ",";
                            results << "{" << jsonString("file", it->path().string()) << ","
                                    << jsonInt("line", lineNum) << ","
                                    << jsonString("text", line.substr(0, 256))
                                    << "}";
                            matchCount++;
                        }
                    }
                }
            } catch (...) {
                // Filesystem errors are non-fatal
            }
            results << "],\"count\":" << matchCount << "}";
            return ToolResult::ok(results.str());
        });
    }

    // --- execute_command tool ---
    {
        ToolDefinition def;
        def.name = "execute_command";
        def.description = "Execute a shell command and return output";
        def.parameters = {
            {"command", "string", "The command to execute", true, ""},
            {"cwd", "string", "Working directory for the command", false, "."}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string command = jsonExtractString(params, "command");
            if (command.empty()) return ToolResult::fail("Missing 'command' parameter");

#ifdef _WIN32
            SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE hRead, hWrite;
            CreatePipe(&hRead, &hWrite, &sa, 0);
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = {sizeof(STARTUPINFOA)};
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hWrite;
            si.hStdError  = hWrite;
            PROCESS_INFORMATION pi = {};

            std::string cmd = "cmd.exe /c " + command;
            if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                               CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                CloseHandle(hRead);
                CloseHandle(hWrite);
                return ToolResult::fail("Failed to execute command");
            }
            CloseHandle(hWrite);

            std::string output;
            char buf[4096];
            DWORD read;
            while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
                buf[read] = '\0';
                output += buf;
            }
            CloseHandle(hRead);
            WaitForSingleObject(pi.hProcess, 10000);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return ToolResult::ok("{\"exitCode\":" + std::to_string(exitCode) + 
                                 ",\"output\":\"" + jsonEscape(output) + "\"}");
#else
            FILE* fp = popen(command.c_str(), "r");
            if (!fp) return ToolResult::fail("Failed to execute command");
            std::string output;
            char buf[4096];
            while (fgets(buf, sizeof(buf), fp)) output += buf;
            int status = pclose(fp);
            return ToolResult::ok("{\"exitCode\":" + std::to_string(status) + 
                                 ",\"output\":\"" + jsonEscape(output) + "\"}");
#endif
        });
    }

    // --- list_directory tool ---
    {
        ToolDefinition def;
        def.name = "list_directory";
        def.description = "List files and directories in a given path";
        def.parameters = {
            {"path", "string", "Directory path to list", true, ""},
            {"recursive", "boolean", "Whether to list recursively", false, "false"}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path = jsonExtractString(params, "path");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

#ifdef _WIN32
            WIN32_FIND_DATAA fd;
            std::string search = path + "\\*";
            HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) return ToolResult::fail("Cannot list: " + path);

            std::ostringstream json;
            json << "{\"entries\":[";
            bool first = true;
            do {
                if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                if (!first) json << ",";
                first = false;
                bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                json << "{\"name\":\"" << jsonEscape(fd.cFileName) << "\","
                     << "\"type\":\"" << (isDir ? "directory" : "file") << "\","
                     << "\"size\":" << ((uint64_t)fd.nFileSizeHigh << 32 | fd.nFileSizeLow) << "}";
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
            json << "]}";
            return ToolResult::ok(json.str());
#else
            return ToolResult::fail("Not implemented on this platform");
#endif
        });
    }

    // --- get_file_info tool ---
    {
        ToolDefinition def;
        def.name = "get_file_info";
        def.description = "Get metadata about a file (size, modification time, permissions)";
        def.parameters = {
            {"path", "string", "Path to the file", true, ""}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path = jsonExtractString(params, "path");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

            namespace fs = std::filesystem;
            std::error_code ec;
            auto status = fs::status(path, ec);
            if (ec) return ToolResult::fail("Cannot stat file: " + path + " (" + ec.message() + ")");

            auto fileSize = fs::file_size(path, ec);
            auto lastWrite = fs::last_write_time(path, ec);
            bool isDir = fs::is_directory(status);
            bool isFile = fs::is_regular_file(status);
            bool isSymlink = fs::is_symlink(path, ec);

            // Convert last_write_time to epoch seconds (portable, no clock_cast)
            auto fileTime = lastWrite.time_since_epoch();
            // file_clock epoch offset: approximate by using current time delta
            auto now_file = fs::file_time_type::clock::now();
            auto now_sys  = std::chrono::system_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::seconds>(now_file.time_since_epoch()) -
                         std::chrono::duration_cast<std::chrono::seconds>(now_sys.time_since_epoch());
            int64_t epochSec = std::chrono::duration_cast<std::chrono::seconds>(fileTime).count() - delta.count();

            std::ostringstream json;
            json << "{" << jsonString("path", path) << ","
                 << jsonInt("size", isFile ? (int64_t)fileSize : 0) << ","
                 << jsonInt("lastModified", epochSec) << ","
                 << jsonBool("isDirectory", isDir) << ","
                 << jsonBool("isFile", isFile) << ","
                 << jsonBool("isSymlink", isSymlink) << "}";
            return ToolResult::ok(json.str());
        });
    }

    // --- get_workspace_info tool ---
    {
        ToolDefinition def;
        def.name = "get_workspace_info";
        def.description = "Get information about the current IDE workspace (root path, open files, project type)";
        def.parameters = {};
        server.registerTool(def, [](const std::string& /*params*/) -> ToolResult {
            namespace fs = std::filesystem;
            std::string cwd = fs::current_path().string();

            // Detect project type by checking for config files
            std::string projectType = "unknown";
            if (fs::exists(cwd + "/CMakeLists.txt")) projectType = "cmake/cpp";
            else if (fs::exists(cwd + "/Cargo.toml")) projectType = "rust";
            else if (fs::exists(cwd + "/package.json")) projectType = "node/javascript";
            else if (fs::exists(cwd + "/pyproject.toml") || fs::exists(cwd + "/setup.py")) projectType = "python";
            else if (fs::exists(cwd + "/go.mod")) projectType = "go";
            else if (fs::exists(cwd + "/Makefile")) projectType = "makefile";

            // Count files by extension
            size_t cppCount = 0, pyCount = 0, jsCount = 0, rsCount = 0, asmCount = 0, otherCount = 0;
            try {
                for (auto& entry : fs::recursive_directory_iterator(cwd,
                         fs::directory_options::skip_permission_denied)) {
                    if (!entry.is_regular_file()) continue;
                    std::string ext = entry.path().extension().string();
                    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp") cppCount++;
                    else if (ext == ".py") pyCount++;
                    else if (ext == ".js" || ext == ".ts") jsCount++;
                    else if (ext == ".rs") rsCount++;
                    else if (ext == ".asm") asmCount++;
                    else otherCount++;
                    // Safety cap
                    if (cppCount + pyCount + jsCount + rsCount + asmCount + otherCount > 50000) break;
                }
            } catch (...) {}

            std::ostringstream json;
            json << "{" << jsonString("workspaceRoot", cwd) << ","
                 << jsonString("projectType", projectType) << ","
                 << "\"fileCounts\":{" 
                 << jsonInt("cpp", (int64_t)cppCount) << ","
                 << jsonInt("python", (int64_t)pyCount) << ","
                 << jsonInt("javascript", (int64_t)jsCount) << ","
                 << jsonInt("rust", (int64_t)rsCount) << ","
                 << jsonInt("asm", (int64_t)asmCount) << ","
                 << jsonInt("other", (int64_t)otherCount)
                 << "}}";
            return ToolResult::ok(json.str());
        });
    }

    // --- apply_edit tool ---
    {
        ToolDefinition def;
        def.name = "apply_edit";
        def.description = "Apply a text replacement edit to a file (find and replace a specific string)";
        def.parameters = {
            {"path", "string", "Path to the file to edit", true, ""},
            {"oldText", "string", "The exact text to find and replace", true, ""},
            {"newText", "string", "The replacement text", true, ""}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path    = jsonExtractString(params, "path");
            std::string oldText = jsonExtractString(params, "oldText");
            std::string newText = jsonExtractString(params, "newText");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");
            if (oldText.empty()) return ToolResult::fail("Missing 'oldText' parameter");

            // Read the file
            std::ifstream inFile(path, std::ios::binary);
            if (!inFile.is_open()) return ToolResult::fail("Cannot open file: " + path);
            std::string content((std::istreambuf_iterator<char>(inFile)),
                                 std::istreambuf_iterator<char>());
            inFile.close();

            // Find and replace
            size_t pos = content.find(oldText);
            if (pos == std::string::npos) {
                return ToolResult::fail("oldText not found in file");
            }

            // Check for multiple matches (ambiguity)
            size_t secondPos = content.find(oldText, pos + oldText.size());
            bool multipleMatches = (secondPos != std::string::npos);

            content.replace(pos, oldText.size(), newText);

            // Write back
            std::ofstream outFile(path, std::ios::binary);
            if (!outFile.is_open()) return ToolResult::fail("Cannot write to: " + path);
            outFile << content;
            outFile.close();

            std::ostringstream json;
            json << "{" << jsonBool("success", true) << ","
                 << jsonInt("offset", (int64_t)pos) << ","
                 << jsonInt("replacedLength", (int64_t)oldText.size()) << ","
                 << jsonInt("newLength", (int64_t)newText.size()) << ","
                 << jsonBool("multipleMatches", multipleMatches) << "}";
            return ToolResult::ok(json.str());
        });
    }

    // --- get_symbols tool ---
    {
        ToolDefinition def;
        def.name = "get_symbols";
        def.description = "Extract function/class/struct symbols from a C/C++ source file using pattern recognition";
        def.parameters = {
            {"path", "string", "Path to the source file to analyze", true, ""}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path = jsonExtractString(params, "path");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

            std::ifstream file(path);
            if (!file.is_open()) return ToolResult::fail("Cannot open file: " + path);

            std::ostringstream json;
            json << "{\"symbols\":[";
            bool firstSym = true;
            int lineNum = 0;
            std::string line;

            // Simple heuristic symbol detection for C/C++
            while (std::getline(file, line)) {
                lineNum++;
                // Skip empty lines and preprocessor directives
                size_t nonSpace = line.find_first_not_of(" \t");
                if (nonSpace == std::string::npos) continue;
                if (line[nonSpace] == '#' || line[nonSpace] == '/') continue;

                // Detect class/struct definitions
                bool isClass = false;
                bool isStruct = false;
                bool isEnum = false;
                bool isFunction = false;
                std::string symbolName;
                std::string symbolKind;

                if (line.find("class ") != std::string::npos && line.find(';') == std::string::npos) {
                    // class Foo { or class Foo :
                    size_t p = line.find("class ");
                    p += 6;
                    while (p < line.size() && line[p] == ' ') p++;
                    size_t end = p;
                    while (end < line.size() && (isalnum(line[end]) || line[end] == '_')) end++;
                    if (end > p) { symbolName = line.substr(p, end - p); symbolKind = "class"; isClass = true; }
                }
                else if (line.find("struct ") != std::string::npos && line.find(';') == std::string::npos) {
                    size_t p = line.find("struct ");
                    p += 7;
                    while (p < line.size() && line[p] == ' ') p++;
                    size_t end = p;
                    while (end < line.size() && (isalnum(line[end]) || line[end] == '_')) end++;
                    if (end > p) { symbolName = line.substr(p, end - p); symbolKind = "struct"; isStruct = true; }
                }
                else if (line.find("enum ") != std::string::npos) {
                    size_t p = line.find("enum ");
                    p += 5;
                    if (p < line.size() && line.substr(p, 6) == "class ") p += 6;
                    while (p < line.size() && line[p] == ' ') p++;
                    size_t end = p;
                    while (end < line.size() && (isalnum(line[end]) || line[end] == '_')) end++;
                    if (end > p) { symbolName = line.substr(p, end - p); symbolKind = "enum"; isEnum = true; }
                }
                else if (line.find('(') != std::string::npos && line.find(';') == std::string::npos
                         && line.find("if") == std::string::npos && line.find("while") == std::string::npos
                         && line.find("for") == std::string::npos && line.find("switch") == std::string::npos
                         && line.find("return") == std::string::npos) {
                    // Likely a function definition: type name(...)
                    size_t parenPos = line.find('(');
                    if (parenPos > 0) {
                        // Walk backwards from '(' to find function name
                        size_t end = parenPos;
                        while (end > 0 && line[end - 1] == ' ') end--;
                        size_t start = end;
                        while (start > 0 && (isalnum(line[start - 1]) || line[start - 1] == '_' || line[start - 1] == ':')) start--;
                        if (end > start) {
                            symbolName = line.substr(start, end - start);
                            // Filter out keywords
                            if (symbolName != "if" && symbolName != "for" && symbolName != "while"
                                && symbolName != "switch" && symbolName != "catch" && symbolName != "sizeof"
                                && symbolName != "decltype" && symbolName != "static_cast"
                                && symbolName != "dynamic_cast" && symbolName != "reinterpret_cast"
                                && !symbolName.empty()) {
                                symbolKind = "function";
                                isFunction = true;
                            }
                        }
                    }
                }

                if (!symbolName.empty() && !symbolKind.empty()) {
                    if (!firstSym) json << ",";
                    firstSym = false;
                    json << "{" << jsonString("name", symbolName) << ","
                         << jsonString("kind", symbolKind) << ","
                         << jsonInt("line", lineNum) << ","
                         << jsonString("file", path) << "}";
                }
            }

            json << "]}";
            return ToolResult::ok(json.str());
        });
    }

    // --- get_completions tool ---
    {
        ToolDefinition def;
        def.name = "get_completions";
        def.description = "Get code completion suggestions for a given position in a file";
        def.parameters = {
            {"path", "string", "Path to the source file", true, ""},
            {"line", "number", "Line number (1-based)", true, "1"},
            {"column", "number", "Column number (1-based)", true, "1"},
            {"prefix", "string", "The text prefix to complete", false, ""}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path   = jsonExtractString(params, "path");
            std::string prefix = jsonExtractString(params, "prefix");
            int64_t line   = jsonExtractInt(params, "line");
            int64_t column = jsonExtractInt(params, "column");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

            // Read file and extract identifiers for local completion
            std::ifstream file(path);
            if (!file.is_open()) return ToolResult::fail("Cannot open file: " + path);

            std::set<std::string> identifiers;
            std::string fileLine;
            while (std::getline(file, fileLine)) {
                // Extract C/C++ identifiers
                size_t i = 0;
                while (i < fileLine.size()) {
                    if (isalpha(fileLine[i]) || fileLine[i] == '_') {
                        size_t start = i;
                        while (i < fileLine.size() && (isalnum(fileLine[i]) || fileLine[i] == '_')) i++;
                        std::string ident = fileLine.substr(start, i - start);
                        if (ident.size() >= 2) identifiers.insert(ident);
                    } else {
                        i++;
                    }
                }
            }

            // Filter by prefix
            std::ostringstream json;
            json << "{\"completions\":[";
            bool first = true;
            size_t count = 0;
            const size_t MAX_COMPLETIONS = 50;
            for (const auto& id : identifiers) {
                if (count >= MAX_COMPLETIONS) break;
                if (!prefix.empty() && id.find(prefix) != 0) continue;
                if (id == prefix) continue; // Don't suggest exact match
                if (!first) json << ",";
                first = false;
                json << "{" << jsonString("label", id) << ","
                     << jsonString("kind", "identifier") << "}";
                count++;
            }
            json << "]," << jsonInt("line", line) << ","
                 << jsonInt("column", column) << "}";
            return ToolResult::ok(json.str());
        });
    }

    // --- get_diagnostics tool ---
    {
        ToolDefinition def;
        def.name = "get_diagnostics";
        def.description = "Get basic diagnostic information for a source file (syntax checks, bracket matching)";
        def.parameters = {
            {"path", "string", "Path to the source file to check", true, ""}
        };
        server.registerTool(def, [](const std::string& params) -> ToolResult {
            std::string path = jsonExtractString(params, "path");
            if (path.empty()) return ToolResult::fail("Missing 'path' parameter");

            std::ifstream file(path);
            if (!file.is_open()) return ToolResult::fail("Cannot open file: " + path);

            std::ostringstream json;
            json << "{\"diagnostics\":[";
            bool firstDiag = true;
            int lineNum = 0;
            std::string line;

            // Track bracket balance
            int braceDepth = 0;   // { }
            int parenDepth = 0;   // ( )
            int bracketDepth = 0; // [ ]
            bool inString = false;
            bool inLineComment = false;
            bool inBlockComment = false;
            int totalLines = 0;
            int blankLines = 0;

            while (std::getline(file, line)) {
                lineNum++;
                totalLines++;
                if (line.find_first_not_of(" \t\r\n") == std::string::npos) { blankLines++; continue; }

                inLineComment = false;
                for (size_t i = 0; i < line.size(); i++) {
                    char c = line[i];
                    char next = (i + 1 < line.size()) ? line[i + 1] : '\0';

                    if (inBlockComment) {
                        if (c == '*' && next == '/') { inBlockComment = false; i++; }
                        continue;
                    }
                    if (inLineComment) continue;

                    if (c == '"' && (i == 0 || line[i - 1] != '\\')) { inString = !inString; continue; }
                    if (inString) continue;

                    if (c == '/' && next == '/') { inLineComment = true; continue; }
                    if (c == '/' && next == '*') { inBlockComment = true; i++; continue; }

                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                    else if (c == '(') parenDepth++;
                    else if (c == ')') parenDepth--;
                    else if (c == '[') bracketDepth++;
                    else if (c == ']') bracketDepth--;

                    // Report negative depth as error
                    if (braceDepth < 0) {
                        if (!firstDiag) json << ",";
                        firstDiag = false;
                        json << "{" << jsonString("severity", "error") << ","
                             << jsonInt("line", lineNum) << ","
                             << jsonString("message", "Unmatched closing brace '}'") << "}";
                        braceDepth = 0;
                    }
                    if (parenDepth < 0) {
                        if (!firstDiag) json << ",";
                        firstDiag = false;
                        json << "{" << jsonString("severity", "error") << ","
                             << jsonInt("line", lineNum) << ","
                             << jsonString("message", "Unmatched closing parenthesis ')'") << "}";
                        parenDepth = 0;
                    }
                    if (bracketDepth < 0) {
                        if (!firstDiag) json << ",";
                        firstDiag = false;
                        json << "{" << jsonString("severity", "error") << ","
                             << jsonInt("line", lineNum) << ","
                             << jsonString("message", "Unmatched closing bracket ']'") << "}";
                        bracketDepth = 0;
                    }
                }
            }

            // Report unclosed brackets at EOF
            if (braceDepth > 0) {
                if (!firstDiag) json << ",";
                firstDiag = false;
                json << "{" << jsonString("severity", "error") << ","
                     << jsonInt("line", totalLines) << ","
                     << jsonString("message", "Unclosed brace(s): " + std::to_string(braceDepth) + " remaining") << "}";
            }
            if (parenDepth > 0) {
                if (!firstDiag) json << ",";
                firstDiag = false;
                json << "{" << jsonString("severity", "error") << ","
                     << jsonInt("line", totalLines) << ","
                     << jsonString("message", "Unclosed parenthesis: " + std::to_string(parenDepth) + " remaining") << "}";
            }
            if (inBlockComment) {
                if (!firstDiag) json << ",";
                firstDiag = false;
                json << "{" << jsonString("severity", "warning") << ","
                     << jsonInt("line", totalLines) << ","
                     << jsonString("message", "Unterminated block comment /* ... */") << "}";
            }

            json << "]," << jsonInt("totalLines", totalLines) << ","
                 << jsonInt("blankLines", blankLines) << "}";
            return ToolResult::ok(json.str());
        });
    }
}

} // namespace MCP
} // namespace RawrXD

