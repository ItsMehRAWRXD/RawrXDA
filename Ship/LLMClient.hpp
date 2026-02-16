// LLMClient.hpp - WinHTTP-based Ollama Client with Streaming Support
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "ToolExecutionEngine.hpp"
#include <winhttp.h>
#include <thread>
#include <queue>
#include <condition_variable>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

// Chat message role
enum class MessageRole {
    System,
    User,
    Assistant,
    Tool
};

inline String roleToString(MessageRole role) {
    switch (role) {
        case MessageRole::System: return L"system";
        case MessageRole::User: return L"user";
        case MessageRole::Assistant: return L"assistant";
        case MessageRole::Tool: return L"tool";
    }
    return L"user";
}

inline MessageRole stringToRole(const String& str) {
    if (str == L"system") return MessageRole::System;
    if (str == L"assistant") return MessageRole::Assistant;
    if (str == L"tool") return MessageRole::Tool;
    return MessageRole::User;
}

// Chat message
struct ChatMessage {
    MessageRole role = MessageRole::User;
    String content;
    String name;                      // For tool calls
    String toolCallId;                // For tool responses
    Vector<JsonObject> toolCalls;     // Tool calls in assistant messages

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"role"] = roleToString(role);
        obj[L"content"] = content;
        if (!name.empty()) {
            obj[L"name"] = name;
        }
        if (!toolCallId.empty()) {
            obj[L"tool_call_id"] = toolCallId;
        }
        if (!toolCalls.empty()) {
            JsonArray calls;
            for (const auto& tc : toolCalls) {
                calls.push_back(tc);
            }
            obj[L"tool_calls"] = calls;
        }
        return obj;
    }

    static ChatMessage fromJson(const JsonObject& obj) {
        ChatMessage msg;
        if (auto it = obj.find(L"role"); it != obj.end()) {
            msg.role = stringToRole(std::get<String>(it->second));
        }
        if (auto it = obj.find(L"content"); it != obj.end()) {
            msg.content = std::get<String>(it->second);
        }
        if (auto it = obj.find(L"name"); it != obj.end()) {
            msg.name = std::get<String>(it->second);
        }
        if (auto it = obj.find(L"tool_call_id"); it != obj.end()) {
            msg.toolCallId = std::get<String>(it->second);
        }
        if (auto it = obj.find(L"tool_calls"); it != obj.end()) {
            if (std::holds_alternative<JsonArray>(it->second)) {
                for (const auto& tc : std::get<JsonArray>(it->second)) {
                    if (std::holds_alternative<JsonObject>(tc)) {
                        msg.toolCalls.push_back(std::get<JsonObject>(tc));
                    }
                }
            }
        }
        return msg;
    }
};

// Tool call extracted from LLM response
struct ToolCall {
    String id;
    String name;
    JsonObject arguments;
};

// LLM completion options
struct CompletionOptions {
    String model = L"qwen2.5-coder:14b";
    double temperature = 0.7;
    int maxTokens = 4096;
    double topP = 0.9;
    int topK = 40;
    Vector<String> stop;
    bool stream = true;
    JsonArray tools;

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"model"] = model;
        obj[L"temperature"] = temperature;
        obj[L"max_tokens"] = static_cast<int64_t>(maxTokens);
        obj[L"top_p"] = topP;
        obj[L"top_k"] = static_cast<int64_t>(topK);
        obj[L"stream"] = stream;
        if (!tools.empty()) {
            obj[L"tools"] = tools;
        }
        if (!stop.empty()) {
            JsonArray stopArr;
            for (const auto& s : stop) {
                stopArr.push_back(s);
            }
            obj[L"stop"] = stopArr;
        }
        return obj;
    }
};

// Streaming callback types
using StreamCallback = std::function<void(const String& chunk)>;
using CompletionCallback = std::function<void(const String& fullResponse, const Vector<ToolCall>& toolCalls)>;
using ErrorCallback = std::function<void(const String& error)>;

// LLM response
struct LLMResponse {
    bool success = false;
    String content;
    Vector<ToolCall> toolCalls;
    String error;
    int promptTokens = 0;
    int completionTokens = 0;
    int64_t responseTimeMs = 0;
};

// HTTP client wrapper using WinHTTP
class WinHttpClient {
public:
    WinHttpClient(const String& host, int port, bool https = false)
        : m_host(host), m_port(port), m_https(https)
    {
        m_session = WinHttpOpen(L"RawrXD/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
    }

    ~WinHttpClient() {
        if (m_session) WinHttpCloseHandle(m_session);
    }

    struct Response {
        int statusCode = 0;
        std::string body;
        String error;
    };

    Response post(const String& path, const std::string& body, bool stream = false,
                  std::function<void(const std::string&)> onChunk = nullptr)
    {
        Response resp;

        if (!m_session) {
            resp.error = L"Failed to open WinHTTP session";
            return resp;
        }

        HINTERNET connect = WinHttpConnect(m_session,
            m_host.c_str(),
            static_cast<INTERNET_PORT>(m_port), 0);

        if (!connect) {
            resp.error = L"Failed to connect to host";
            return resp;
        }

        HINTERNET request = WinHttpOpenRequest(connect,
            L"POST",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            m_https ? WINHTTP_FLAG_SECURE : 0);

        if (!request) {
            WinHttpCloseHandle(connect);
            resp.error = L"Failed to open request";
            return resp;
        }

        // Set headers
        WinHttpAddRequestHeaders(request,
            L"Content-Type: application/json",
            static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD);

        // Set timeouts
        DWORD timeout = 300000; // 5 minutes
        WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

        // Send request
        if (!WinHttpSendRequest(request,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()), 0))
        {
            resp.error = L"Failed to send request";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            return resp;
        }

        // Receive response
        if (!WinHttpReceiveResponse(request, nullptr)) {
            resp.error = "Failed to receive response";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            return resp;
        }

        // Get status code
        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
        resp.statusCode = static_cast<int>(statusCode);

        // Read response body
        char buffer[8192];
        DWORD bytesRead = 0;
        DWORD bytesAvailable = 0;

        while (WinHttpQueryDataAvailable(request, &bytesAvailable) && bytesAvailable > 0) {
            DWORD toRead = (std::min)(bytesAvailable, static_cast<DWORD>(sizeof(buffer)));
            if (WinHttpReadData(request, buffer, toRead, &bytesRead) && bytesRead > 0) {
                if (stream && onChunk) {
                    onChunk(std::string(buffer, bytesRead));
                } else {
                    resp.body.append(buffer, bytesRead);
                }
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        return resp;
    }

private:
    String m_host;
    int m_port;
    bool m_https;
    HINTERNET m_session = nullptr;
};

// Main LLM Client
class LLMClient {
public:
    LLMClient(const String& host = L"localhost", int port = 11434)
        : m_client(host, port, false)
    {
    }

    // Set model
    void setModel(const String& model) {
        m_options.model = model;
    }

    // Set temperature
    void setTemperature(double temp) {
        m_options.temperature = temp;
    }

    // Set max tokens
    void setMaxTokens(int tokens) {
        m_options.maxTokens = tokens;
    }

    // Set tools schema
    void setTools(const JsonArray& tools) {
        m_options.tools = tools;
    }

    // Synchronous completion
    LLMResponse complete(const Vector<ChatMessage>& messages) {
        auto start = std::chrono::steady_clock::now();
        LLMResponse resp;

        // Build request
        JsonObject request;
        request[L"model"] = m_options.model;
        request[L"stream"] = false;

        JsonArray msgArray;
        for (const auto& msg : messages) {
            msgArray.push_back(msg.toJson());
        }
        request[L"messages"] = msgArray;

        if (!m_options.tools.empty()) {
            request[L"tools"] = m_options.tools;
        }

        JsonObject options;
        options[L"temperature"] = m_options.temperature;
        options[L"num_predict"] = static_cast<int64_t>(m_options.maxTokens);
        options[L"top_p"] = m_options.topP;
        options[L"top_k"] = static_cast<int64_t>(m_options.topK);
        request[L"options"] = options;

        std::string body = JsonParser::Serialize(request, 0);

        // Send request
        auto httpResp = m_client.post(L"/api/chat", body, false);

        if (!httpResp.error.empty()) {
            resp.error = httpResp.error;
            return resp;
        }

        if (httpResp.statusCode != 200) {
            resp.error = L"HTTP error: " + std::to_wstring(httpResp.statusCode);
            return resp;
        }

        // Parse response
        auto jsonOpt = JsonParser::Parse(httpResp.body);
        if (!jsonOpt || !std::holds_alternative<JsonObject>(*jsonOpt)) {
            resp.error = L"Failed to parse response";
            return resp;
        }

        const auto& json = std::get<JsonObject>(*jsonOpt);

        // Extract message
        if (auto msgIt = json.find(L"message"); msgIt != json.end()) {
            if (std::holds_alternative<JsonObject>(msgIt->second)) {
                const auto& msgObj = std::get<JsonObject>(msgIt->second);

                if (auto contentIt = msgObj.find(L"content"); contentIt != msgObj.end()) {
                    resp.content = std::get<String>(contentIt->second);
                }

                // Extract tool calls
                if (auto tcIt = msgObj.find(L"tool_calls"); tcIt != msgObj.end()) {
                    if (std::holds_alternative<JsonArray>(tcIt->second)) {
                        for (const auto& tc : std::get<JsonArray>(tcIt->second)) {
                            if (std::holds_alternative<JsonObject>(tc)) {
                                resp.toolCalls.push_back(parseToolCall(std::get<JsonObject>(tc)));
                            }
                        }
                    }
                }
            }
        }

        resp.success = true;
        auto end = std::chrono::steady_clock::now();
        resp.responseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        return resp;
    }

    // Streaming completion
    void streamComplete(const Vector<ChatMessage>& messages,
                        StreamCallback onChunk,
                        CompletionCallback onComplete,
                        ErrorCallback onError = nullptr)
    {
        std::thread([this, messages, onChunk, onComplete, onError]() {
            // Build request
            JsonObject request;
            request[L"model"] = m_options.model;
            request[L"stream"] = true;

            JsonArray msgArray;
            for (const auto& msg : messages) {
                msgArray.push_back(msg.toJson());
            }
            request[L"messages"] = msgArray;

            if (!m_options.tools.empty()) {
                request[L"tools"] = m_options.tools;
            }

            JsonObject options;
            options[L"temperature"] = m_options.temperature;
            options[L"num_predict"] = static_cast<int64_t>(m_options.maxTokens);
            request[L"options"] = options;

            std::string body = JsonParser::Serialize(request, 0);

            String fullResponse;
            Vector<ToolCall> toolCalls;
            std::string buffer;

            auto httpResp = m_client.post(L"/api/chat", body, true,
                [&](const std::string& chunk) {
                    buffer += chunk;

                    // Process complete lines
                    size_t pos;
                    while ((pos = buffer.find('\n')) != std::string::npos) {
                        std::string line = buffer.substr(0, pos);
                        buffer = buffer.substr(pos + 1);

                        if (line.empty()) continue;

                        auto jsonOpt = JsonParser::Parse(line);
                        if (!jsonOpt || !std::holds_alternative<JsonObject>(*jsonOpt)) continue;

                        const auto& json = std::get<JsonObject>(*jsonOpt);

                        // Check for done
                        if (auto doneIt = json.find(L"done"); doneIt != json.end()) {
                            if (std::holds_alternative<bool>(doneIt->second) && std::get<bool>(doneIt->second)) {
                                // Final message may have tool calls
                                if (auto msgIt = json.find(L"message"); msgIt != json.end()) {
                                    if (std::holds_alternative<JsonObject>(msgIt->second)) {
                                        const auto& msgObj = std::get<JsonObject>(msgIt->second);
                                        if (auto tcIt = msgObj.find(L"tool_calls"); tcIt != msgObj.end()) {
                                            if (std::holds_alternative<JsonArray>(tcIt->second)) {
                                                for (const auto& tc : std::get<JsonArray>(tcIt->second)) {
                                                    if (std::holds_alternative<JsonObject>(tc)) {
                                                        toolCalls.push_back(parseToolCall(std::get<JsonObject>(tc)));
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                continue;
                            }
                        }

                        // Extract content chunk
                        if (auto msgIt = json.find(L"message"); msgIt != json.end()) {
                            if (std::holds_alternative<JsonObject>(msgIt->second)) {
                                const auto& msgObj = std::get<JsonObject>(msgIt->second);
                                if (auto contentIt = msgObj.find(L"content"); contentIt != msgObj.end()) {
                                    if (std::holds_alternative<String>(contentIt->second)) {
                                        String content(std::get<String>(contentIt->second));
                                        fullResponse += content;
                                        if (onChunk) onChunk(content);
                                    }
                                }
                            }
                        }
                    }
                });

            if (!httpResp.error.empty()) {
                if (onError) onError(httpResp.error);
                return;
            }

            if (onComplete) onComplete(fullResponse, toolCalls);
        }).detach();
    }

    // Simple prompt completion
    String prompt(const String& userMessage) {
        Vector<ChatMessage> messages;
        ChatMessage msg;
        msg.role = MessageRole::User;
        msg.content = userMessage;
        messages.push_back(msg);

        auto resp = complete(messages);
        return resp.success ? resp.content : String();
    }

    // Check if server is available
    bool isAvailable() {
        auto resp = m_client.post(L"/api/tags", "{}", false);
        return resp.statusCode == 200;
    }

    // List available models
    Vector<String> listModels() {
        Vector<String> models;
        auto resp = m_client.post(L"/api/tags", "{}", false);
        if (resp.statusCode != 200) return models;

        auto jsonOpt = JsonParser::Parse(resp.body);
        if (!jsonOpt || !std::holds_alternative<JsonObject>(*jsonOpt)) return models;

        const auto& json = std::get<JsonObject>(*jsonOpt);
        if (auto it = json.find(L"models"); it != json.end()) {
            if (std::holds_alternative<JsonArray>(it->second)) {
                for (const auto& m : std::get<JsonArray>(it->second)) {
                    if (std::holds_alternative<JsonObject>(m)) {
                        const auto& modelObj = std::get<JsonObject>(m);
                        if (auto nameIt = modelObj.find(L"name"); nameIt != modelObj.end()) {
                            if (std::holds_alternative<String>(nameIt->second)) {
                                models.push_back(std::get<String>(nameIt->second));
                            }
                        }
                    }
                }
            }
        }
        return models;
    }

    // Get options
    CompletionOptions& options() { return m_options; }

private:
    ToolCall parseToolCall(const JsonObject& obj) {
        ToolCall tc;
        if (auto it = obj.find(L"id"); it != obj.end()) {
            tc.id = std::get<String>(it->second);
        }
        if (auto it = obj.find(L"function"); it != obj.end()) {
            if (std::holds_alternative<JsonObject>(it->second)) {
                const auto& func = std::get<JsonObject>(it->second);
                if (auto nameIt = func.find(L"name"); nameIt != func.end()) {
                    tc.name = std::get<String>(nameIt->second);
                }
                if (auto argsIt = func.find(L"arguments"); argsIt != func.end()) {
                    if (std::holds_alternative<String>(argsIt->second)) {
                        auto argsJson = JsonParser::Parse(StringUtils::ToUtf8(std::get<String>(argsIt->second)));
                        if (argsJson && std::holds_alternative<JsonObject>(*argsJson)) {
                            tc.arguments = std::get<JsonObject>(*argsJson);
                        }
                    } else if (std::holds_alternative<JsonObject>(argsIt->second)) {
                        tc.arguments = std::get<JsonObject>(argsIt->second);
                    }
                }
            }
        }
        return tc;
    }

    WinHttpClient m_client;
    CompletionOptions m_options;
};

// Conversation manager
class ConversationManager {
public:
    ConversationManager() = default;

    void setSystemPrompt(const String& prompt) {
        m_systemPrompt = prompt;
    }

    void addMessage(const ChatMessage& msg) {
        m_messages.push_back(msg);
        trimHistory();
    }

    void addUserMessage(const String& content) {
        ChatMessage msg;
        msg.role = MessageRole::User;
        msg.content = content;
        addMessage(msg);
    }

    void addAssistantMessage(const String& content, const Vector<JsonObject>& toolCalls = {}) {
        ChatMessage msg;
        msg.role = MessageRole::Assistant;
        msg.content = content;
        msg.toolCalls = toolCalls;
        addMessage(msg);
    }

    void addToolResult(const String& toolCallId, const String& name, const String& result) {
        ChatMessage msg;
        msg.role = MessageRole::Tool;
        msg.toolCallId = toolCallId;
        msg.name = name;
        msg.content = result;
        addMessage(msg);
    }

    Vector<ChatMessage> getMessages() const {
        Vector<ChatMessage> result;
        if (!m_systemPrompt.empty()) {
            ChatMessage sys;
            sys.role = MessageRole::System;
            sys.content = m_systemPrompt;
            result.push_back(sys);
        }
        result.insert(result.end(), m_messages.begin(), m_messages.end());
        return result;
    }

    void clear() {
        m_messages.clear();
    }

    void setMaxHistory(int max) {
        m_maxHistory = max;
        trimHistory();
    }

    int messageCount() const {
        return static_cast<int>(m_messages.size());
    }

private:
    void trimHistory() {
        if (m_maxHistory > 0 && static_cast<int>(m_messages.size()) > m_maxHistory) {
            m_messages.erase(m_messages.begin(),
                m_messages.begin() + (m_messages.size() - m_maxHistory));
        }
    }

    String m_systemPrompt;
    Vector<ChatMessage> m_messages;
    int m_maxHistory = 50;
};

} // namespace RawrXD
