// =============================================================================
// AgentOllamaClient.cpp — Streaming Ollama Client Implementation
// =============================================================================
#include "AgentOllamaClient.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#pragma comment(lib, "winhttp.lib")
#endif

using RawrXD::Agent::AgentOllamaClient;
using RawrXD::Agent::InferenceResult;

namespace {
    const char* kChatEndpoint = "/api/chat";
    const char* kGenerateEndpoint = "/api/generate";
    const char* kTagsEndpoint = "/api/tags";
    const char* kVersionEndpoint = "/api/version";

    std::wstring ToWide(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
        if (!out.empty() && out.back() == L'\0') out.pop_back();
        return out;
    }

    class MiniJSON {
    public:
        enum Type { NULL_TYPE, BOOL, NUMBER, STRING, ARRAY, OBJECT };
        
        Type type = NULL_TYPE;
        std::string string_val;
        double number_val = 0;
        bool bool_val = false;
        std::vector<MiniJSON> array_val;
        std::map<std::string, MiniJSON> object_val;
        
        static MiniJSON parse(const std::string& s) {
            size_t pos = 0;
            return parse_value(s, pos);
        }
        
        bool contains(const std::string& key) const {
            return type == OBJECT && object_val.count(key);
        }
        
        const MiniJSON& operator[](const std::string& key) const {
            static MiniJSON null;
            if (type != OBJECT) return null;
            auto it = object_val.find(key);
            return (it != object_val.end()) ? it->second : null;
        }
        
        std::string get_string() const {
            return (type == STRING) ? string_val : "";
        }
        
        double get_number() const {
            return (type == NUMBER) ? number_val : 0;
        }
        
        bool get_bool() const {
            return (type == BOOL) ? bool_val : false;
        }
        
        bool is_array() const { return type == ARRAY; }
        bool is_object() const { return type == OBJECT; }
        bool is_string() const { return type == STRING; }
        
        // nlohmann compatibility
        template<typename T> T get() const;
        bool is_null() const { return type == NULL_TYPE; }
        size_t size() const {
            if (type == ARRAY) return array_val.size();
            if (type == OBJECT) return object_val.size();
            return 0;
        }
        const MiniJSON& operator[](size_t idx) const {
            static MiniJSON null;
            return (type == ARRAY && idx < array_val.size()) ? array_val[idx] : null;
        }
        
    private:
        static void skip_ws(const std::string& s, size_t& pos) {
            while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
        }
        
        static MiniJSON parse_value(const std::string& s, size_t& pos) {
            skip_ws(s, pos);
            if (pos >= s.size()) return MiniJSON();
            
            char c = s[pos];
            if (c == '"') return parse_string(s, pos);
            if (c == '{') return parse_object(s, pos);
            if (c == '[') return parse_array(s, pos);
            if (c == 't') return parse_true(s, pos);
            if (c == 'f') return parse_false(s, pos);
            if (c == 'n') return parse_null(s, pos);
            if (c == '-' || isdigit((unsigned char)c)) return parse_number(s, pos);
            return MiniJSON();
        }

        static void log_parse_error(const std::string& msg, const std::string& s, size_t pos) {
            std::cerr << "[MiniJSON] " << msg << " at pos " << pos << ": '" 
                      << s.substr(pos, std::min((size_t)20, s.size() - pos)) << "'\n";
        }
        
        static MiniJSON parse_string(const std::string& s, size_t& pos) {
            MiniJSON j;
            j.type = STRING;
            if (pos >= s.size() || s[pos] != '"') {
                log_parse_error("Expected quote", s, pos);
                return j;
            }
            pos++; // skip opening "
            std::string result;
            while (pos < s.size() && s[pos] != '"') {
                if (s[pos] == '\\' && pos + 1 < s.size()) {
                    char next = s[++pos];
                    switch (next) {
                        case '"': result += '"'; break;
                        case '\\': result += '\\'; break;
                        case '/': result += '/'; break;
                        case 'b': result += '\b'; break;
                        case 'f': result += '\f'; break;
                        case 'n': result += '\n'; break;
                        case 'r': result += '\r'; break;
                        case 't': result += '\t'; break;
                        case 'u': if (pos + 4 < s.size()) { pos += 4; result += '?'; } break;
                        default: result += next;
                    }
                } else {
                    result += s[pos];
                }
                pos++;
            }
            if (pos < s.size()) pos++; // skip closing "
            else log_parse_error("Unterminated string", s, pos);
            j.string_val = result;
            return j;
        }
        
        static MiniJSON parse_object(const std::string& s, size_t& pos) {
            MiniJSON j;
            j.type = OBJECT;
            if (pos >= s.size() || s[pos] != '{') {
                log_parse_error("Expected {", s, pos);
                return j;
            }
            pos++; // skip {
            skip_ws(s, pos);
            if (pos < s.size() && s[pos] == '}') { pos++; return j; }
            while (pos < s.size()) {
                skip_ws(s, pos);
                if (pos >= s.size() || s[pos] != '"') {
                    log_parse_error("Expected key", s, pos);
                    break;
                }
                MiniJSON key = parse_string(s, pos);
                skip_ws(s, pos);
                if (pos >= s.size() || s[pos] != ':') {
                    log_parse_error("Expected :", s, pos);
                    break;
                }
                pos++; // skip :
                j.object_val[key.string_val] = parse_value(s, pos);
                skip_ws(s, pos);
                if (pos >= s.size()) break;
                if (s[pos] == ',') { pos++; continue; }
                if (s[pos] == '}') { pos++; break; }
                log_parse_error("Expected , or }", s, pos);
                break;
            }
            return j;
        }
        
        static MiniJSON parse_array(const std::string& s, size_t& pos) {
            MiniJSON j;
            j.type = ARRAY;
            pos++; // skip [
            skip_ws(s, pos);
            if (pos < s.size() && s[pos] == ']') { pos++; return j; }
            while (pos < s.size()) {
                j.array_val.push_back(parse_value(s, pos));
                skip_ws(s, pos);
                if (pos >= s.size()) break;
                if (s[pos] == ',') { pos++; continue; }
                if (s[pos] == ']') { pos++; break; }
            }
            return j;
        }
        
        static MiniJSON parse_number(const std::string& s, size_t& pos) {
            MiniJSON j;
            j.type = NUMBER;
            size_t start = pos;
            if (s[pos] == '-') pos++;
            while (pos < s.size() && isdigit((unsigned char)s[pos])) pos++;
            if (pos < s.size() && s[pos] == '.') {
                pos++;
                while (pos < s.size() && isdigit((unsigned char)s[pos])) pos++;
            }
            if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
                pos++;
                if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
                while (pos < s.size() && isdigit((unsigned char)s[pos])) pos++;
            }
            j.number_val = std::stod(s.substr(start, pos - start));
            return j;
        }
        
        static MiniJSON parse_true(const std::string& s, size_t& pos) {
            MiniJSON j;
            if (s.substr(pos, 4) == "true") { j.type = BOOL; j.bool_val = true; pos += 4; }
            return j;
        }
        
        static MiniJSON parse_false(const std::string& s, size_t& pos) {
            MiniJSON j;
            if (s.substr(pos, 5) == "false") { j.type = BOOL; j.bool_val = false; pos += 5; }
            return j;
        }
        
        static MiniJSON parse_null(const std::string& s, size_t& pos) {
            MiniJSON j; j.type = NULL_TYPE;
            if (s.substr(pos, 4) == "null") pos += 4;
            return j;
        }
    };
    
    template<> std::string MiniJSON::get<std::string>() const { return get_string(); }
    template<> double MiniJSON::get<double>() const { return get_number(); }
    template<> bool MiniJSON::get<bool>() const { return get_bool(); }

    using json_alias = MiniJSON;

    static json_alias manual_parse_models(const std::string& s) {
        std::cerr << "[MiniJSON] manual_parse_models starting, input size=" << s.size() << "\n";
        json_alias j;
        j.type = MiniJSON::OBJECT;
        json_alias arr;
        arr.type = MiniJSON::ARRAY;

        // Ollama tags response structure:
        // {"models":[{"name":"phi3:mini","modified_at":"...","size":...,"digest":"...","details":{...}},...]}
        
        std::string needle = "\"name\":";
        size_t pos = s.find(needle);
        while (pos != std::string::npos) {
            // Find the character following "name":
            size_t val_start = pos + needle.length();
            while (val_start < s.size() && isspace((unsigned char)s[val_start])) val_start++;
            
            if (val_start < s.size() && s[val_start] == '\"') {
                size_t q1 = val_start;
                size_t q2 = s.find('\"', q1 + 1);
                if (q2 != std::string::npos) {
                    std::string mname = s.substr(q1 + 1, q2 - q1 - 1);
                    std::cerr << "[MiniJSON] manual_parse_models: found model name: " << mname << "\n";
                    
                    json_alias mobj;
                    mobj.type = MiniJSON::OBJECT;
                    json_alias nameVal;
                    nameVal.type = MiniJSON::STRING;
                    nameVal.string_val = mname;
                    mobj.object_val["name"] = nameVal;
                    arr.array_val.push_back(mobj);
                    
                    pos = s.find(needle, q2 + 1);
                } else break;
            } else {
                pos = s.find(needle, val_start);
            }
        }
        std::cerr << "[MiniJSON] manual_parse_models: finished, found " << arr.array_val.size() << " models\n";
        j.object_val["models"] = arr;
        return j;
    }
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AgentOllamaClient::AgentOllamaClient(const OllamaConfig& config)
    : m_config(config)
{
#ifdef _WIN32
    InitWinHTTP();
#endif
}

AgentOllamaClient::~AgentOllamaClient() {
    CancelStream();
#ifdef _WIN32
    CleanupWinHTTP();
#endif
}

#ifdef _WIN32
void AgentOllamaClient::InitWinHTTP() {
    m_hSession = WinHttpOpen(L"RawrXD-Agent/1.0",
                             WINHTTP_ACCESS_TYPE_NO_PROXY,
                             WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0);
    if (m_hSession) {
        DWORD timeout = static_cast<DWORD>(m_config.timeout_ms);
        WinHttpSetTimeouts(m_hSession, timeout, timeout, timeout, timeout);
    } else {
        std::cerr << "[AgentOllamaClient] WinHttpOpen failed: " << GetLastError() << "\n";
    }
}

void AgentOllamaClient::CleanupWinHTTP() {
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
    }
}
#endif

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

bool AgentOllamaClient::TestConnection() {
    // A quick sanity check: the /api/version endpoint should return something.
    std::string resp = MakeGetRequest(kVersionEndpoint);
    return !resp.empty();
}

std::string AgentOllamaClient::GetVersion() {
    std::string resp = MakeGetRequest(kVersionEndpoint);
    if (resp.empty()) return "unknown";
    
    std::cerr << "[AgentOllamaClient] GetVersion: raw resp='" << resp << "'\n";
    try {
        const char* key = "\"version\"";
        size_t vpos = resp.find(key);
        if (vpos != std::string::npos) {
            size_t val_start = vpos + 9;
            while (val_start < resp.size() && (isspace((unsigned char)resp[val_start]) || resp[val_start] == ':')) val_start++;
            
            if (val_start < resp.size() && resp[val_start] == '\"') {
                size_t q1 = val_start;
                size_t q2 = resp.find('\"', q1 + 1);
                if (q2 != std::string::npos) {
                    std::string v = resp.substr(q1 + 1, q2 - q1 - 1);
                    std::cerr << "[AgentOllamaClient] GetVersion found: " << v << "\n";
                    return v;
                }
            }
        }
        std::cerr << "[AgentOllamaClient] GetVersion: Manual search failed despite presence in hex logs.\n";
        return "unknown";
    } catch (const std::exception& e) {
        std::cerr << "[AgentOllamaClient] GetVersion Exception: " << e.what() << "\n";
        return "unknown";
    }
}

std::vector<std::string> AgentOllamaClient::ListModels() {
    std::vector<std::string> models;
    std::string resp = MakeGetRequest(kTagsEndpoint);
    if (resp.empty()) return models;

    try {
        json_alias j = manual_parse_models(resp);
        if (j.contains("models") && j["models"].is_array()) {
            const json_alias& models_array = j["models"];
            for (size_t i = 0; i < models_array.size(); ++i) {
                const auto& model = models_array[i];
                if (model.contains("name")) {
                    std::string name = model["name"].get<std::string>();
                    models.push_back(name);
                    std::cerr << "[AgentOllamaClient] ListModels: Found model: " << name << "\n";
                }
            }
        }
        std::cerr << "[AgentOllamaClient] ListModels: Successfully parsed " << models.size() << " models\n";
    } catch (const std::exception& e) {
        std::cerr << "[AgentOllamaClient] ListModels: Exception: " << e.what() << "\n";
    }
    return models;
}

// ---------------------------------------------------------------------------
// Chat API
// ---------------------------------------------------------------------------

InferenceResult AgentOllamaClient::ChatSync(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& tools)
{
    nlohmann::json payload = BuildChatPayload(messages, tools, false);
    std::string resp = MakePostRequest(kChatEndpoint, payload.dump());
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    if (resp.empty()) {
        std::cerr << "[AgentOllamaClient] ChatSync: MakePostRequest returned an empty string!\n";
        return InferenceResult::error("Empty response from Ollama");
    }
    
    InferenceResult parsed = ParseChatResponse(resp);
    std::cerr << "[AgentOllamaClient] ChatSync: ParseChatResponse returned response_len=" << parsed.response.size() << "\n";
    return parsed;
}

bool AgentOllamaClient::ChatStream(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& tools,
    TokenCallback on_token,
    ToolCallCallback on_tool_call,
    DoneCallback on_done,
    ErrorCallback on_error)
{
    nlohmann::json payload = BuildChatPayload(messages, tools, true);
    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    std::string fullResponse;
    uint64_t promptTokens = 0, completionTokens = 0;
    double tps = 0.0;

    bool result = MakeStreamingPost(kChatEndpoint, payload.dump(),
        [&](const std::string& line) -> bool {
            if (m_cancelRequested.load()) return false;
            if (line.empty()) return true;

            try {
                nlohmann::json j = nlohmann::json::parse(line);
                if (j.is_string()) {
                    // Some servers wrap the JSON in a string value.
                    try {
                        j = nlohmann::json::parse(j.get<std::string>());
                    } catch (...) {
                        // fall back to raw string
                    }
                }

                bool done = j.value("done", false);

                // Extract possible text fields (Ollama uses "response" in some cases)
                std::string content;
                if (j.contains("message") && j["message"].is_object()) {
                    content = j["message"].value("content", "");
                }
                if (content.empty() && j.contains("response")) {
                    if (j["response"].is_string()) {
                        content = j["response"].get<std::string>();
                    } else if (j["response"].is_object()) {
                        content = j["response"].value("content", "");
                    }
                }
                if (content.empty() && j.contains("content")) {
                    content = j.value("content", "");
                }

                if (!content.empty()) {
                    fullResponse += content;
                    if (on_token) on_token(content);
                }

                if (done) {
                    promptTokens = j.value("prompt_eval_count", 0ULL);
                    completionTokens = j.value("eval_count", 0ULL);
                    uint64_t evalDuration = j.value("eval_duration", 0ULL);
                    if (evalDuration > 0) {
                        tps = static_cast<double>(completionTokens) /
                              (static_cast<double>(evalDuration) / 1e9);
                    }
                    return false; // Stop reading
                }
            } catch (...) {
                // Non-JSON line, skip
            }
            return true;
        },
        on_error);

    m_streaming.store(false);

    if (result && on_done) {
        on_done(fullResponse, promptTokens, completionTokens, tps);
    }

    m_totalTokens.fetch_add(completionTokens, std::memory_order_relaxed);
    return result;
}

// ---------------------------------------------------------------------------
// FIM API (Ghost Text)
// ---------------------------------------------------------------------------

InferenceResult AgentOllamaClient::FIMSync(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename)
{
    nlohmann::json payload = BuildFIMPayload(prefix, suffix, filename, false);
    std::string resp = MakePostRequest(kGenerateEndpoint, payload.dump());
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    if (resp.empty()) return InferenceResult::error("Empty FIM response");
    return ParseFIMResponse(resp);
}

bool AgentOllamaClient::FIMStream(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename,
    TokenCallback on_token,
    DoneCallback on_done,
    ErrorCallback on_error)
{
    nlohmann::json payload = BuildFIMPayload(prefix, suffix, filename, true);
    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    std::string fullResponse;
    uint64_t completionTokens = 0;
    double tps = 0.0;

    bool result = MakeStreamingPost(kGenerateEndpoint, payload.dump(),
        [&](const std::string& line) -> bool {
            if (m_cancelRequested.load()) return false;
            if (line.empty()) return true;

            try {
                nlohmann::json j = nlohmann::json::parse(line);
                bool done = j.value("done", false);

                std::string token = j.value("response", "");
                if (!token.empty()) {
                    fullResponse += token;
                    if (on_token) on_token(token);
                }

                if (done) {
                    completionTokens = j.value("eval_count", 0ULL);
                    uint64_t evalDuration = j.value("eval_duration", 0ULL);
                    if (evalDuration > 0) {
                        tps = static_cast<double>(completionTokens) /
                              (static_cast<double>(evalDuration) / 1e9);
                    }
                    return false;
                }
            } catch (...) {}
            return true;
        },
        on_error);

    m_streaming.store(false);

    if (result && on_done) {
        on_done(fullResponse, 0, completionTokens, tps);
    }

    m_totalTokens.fetch_add(completionTokens, std::memory_order_relaxed);
    return result;
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

void AgentOllamaClient::CancelStream() {
    m_cancelRequested.store(true);
}

void AgentOllamaClient::SetConfig(const OllamaConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

double AgentOllamaClient::GetAvgTokensPerSec() const {
    uint64_t tokens = m_totalTokens.load();
    if (tokens == 0 || m_totalDurationMs == 0.0) return 0.0;
    return static_cast<double>(tokens) / (m_totalDurationMs / 1000.0);
}

// ---------------------------------------------------------------------------
// JSON Payload Builders
// ---------------------------------------------------------------------------

nlohmann::json AgentOllamaClient::BuildChatPayload(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& tools,
    bool stream) const
{
    nlohmann::json payload;
    payload["model"] = m_config.chat_model;
    payload["stream"] = stream;

    // Messages
    nlohmann::json msgs = nlohmann::json::array();
    for (const auto& m : messages) {
        nlohmann::json msg;
        msg["role"] = m.role;
        msg["content"] = m.content;
        if (!m.tool_call_id.empty()) {
            msg["tool_call_id"] = m.tool_call_id;
        }
        if (!m.tool_calls.is_null() && !m.tool_calls.empty()) {
            msg["tool_calls"] = m.tool_calls;
        }
        msgs.push_back(msg);
    }
    payload["messages"] = msgs;

    // Tools (OpenAI function-calling format)
    if (!tools.empty() && tools.is_array()) {
        payload["tools"] = tools;
    }

    // Options
    nlohmann::json options;
    options["temperature"] = m_config.temperature;
    options["top_p"] = m_config.top_p;
    options["num_predict"] = m_config.max_tokens;
    options["num_ctx"] = m_config.num_ctx;
    if (m_config.use_gpu) {
        options["num_gpu"] = m_config.num_gpu;
    }
    payload["options"] = options;

    return payload;
}

nlohmann::json AgentOllamaClient::BuildFIMPayload(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename,
    bool stream) const
{
    nlohmann::json payload;
    payload["model"] = m_config.fim_model;
    payload["stream"] = stream;
    payload["raw"] = true;

    // Qwen2.5-Coder uses <|fim_prefix|>, <|fim_suffix|>, <|fim_middle|> tokens
    // DeepSeek Coder uses <|fim▁begin|>, <|fim▁hole|>, <|fim▁end|>
    // We use the Qwen format by default
    std::string prompt;
    prompt += "<|fim_prefix|>";
    if (!filename.empty()) {
        prompt += "# " + filename + "\n";
    }
    prompt += prefix;
    prompt += "<|fim_suffix|>";
    prompt += suffix;
    prompt += "<|fim_middle|>";

    payload["prompt"] = prompt;

    // FIM-specific options
    nlohmann::json options;
    options["temperature"] = 0.0f;  // Deterministic for completions
    options["top_p"] = 0.95f;
    options["num_predict"] = m_config.fim_max_tokens;
    options["num_ctx"] = m_config.num_ctx;
    {
        nlohmann::json stopArr = nlohmann::json::array();
        stopArr.push_back("<|fim_pad|>");
        stopArr.push_back("<|endoftext|>");
        stopArr.push_back("\n\n\n");
        options["stop"] = stopArr;
    }
    if (m_config.use_gpu) {
        options["num_gpu"] = m_config.num_gpu;
    }
    payload["options"] = options;

    return payload;
}

// ---------------------------------------------------------------------------
// Response Parsers
// ---------------------------------------------------------------------------

InferenceResult AgentOllamaClient::ParseChatResponse(const std::string& json_str) {
    InferenceResult result;
    result.success = true;
    result.has_tool_calls = false;
    result.response = "";
    result.prompt_tokens = 0;
    result.completion_tokens = 0;

    // MANUAL SCAN for "content":"..."
    size_t c_start = json_str.find("\"content\":\"");
    if (c_start != std::string::npos) {
        c_start += 11;
        size_t c_end = json_str.find("\"", c_start);
        if (c_end != std::string::npos) {
            result.response = json_str.substr(c_start, c_end - c_start);
            std::cerr << "[AgentOllamaClient] ParseChatResponse: MANUAL SCAN EXTRACTED: " << result.response << "\n";
        }
    }

    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        
        // Ollama uses these fields for token counts in /api/chat
        if (j.contains("prompt_eval_count")) {
            result.prompt_tokens = j["prompt_eval_count"].get<uint64_t>();
        } else if (j.contains("prompt_tokens")) {
            result.prompt_tokens = j["prompt_tokens"].get<uint64_t>();
        }

        if (j.contains("eval_count")) {
            result.completion_tokens = j["eval_count"].get<uint64_t>();
        } else if (j.contains("completion_tokens")) {
            result.completion_tokens = j["completion_tokens"].get<uint64_t>();
        }

        if (j.contains("eval_duration")) {
            uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
            if (eval_ns > 0 && result.completion_tokens > 0) {
                result.tokens_per_sec = (double)result.completion_tokens / ((double)eval_ns / 1e9);
            }
        }
        
        if (j.contains("total_duration")) {
            result.total_duration_ms = j["total_duration"].get<uint64_t>() / 1000000;
        }

        std::cerr << "[AgentOllamaClient] ParseChatResponse: JSON stats p=" << result.prompt_tokens 
                  << " c=" << result.completion_tokens << " tps=" << result.tokens_per_sec << "\n";
        
        // Final sanity check: if tokens are still 0, try the manual scanner on the raw string
        if (result.prompt_tokens == 0 || result.completion_tokens == 0) {
             std::cerr << "[AgentOllamaClient] ParseChatResponse: Tokens were zero in JSON, trying manual number scan...\n";
             auto manualIntScan = [&](const std::string& key) -> uint64_t {
                size_t pos = json_str.find("\"" + key + "\":");
                if (pos != std::string::npos) {
                    size_t start = json_str.find_first_of("0123456789", pos + key.length());
                    if (start != std::string::npos) {
                        size_t end = json_str.find_first_not_of("0123456789", start);
                        return std::stoull(json_str.substr(start, end - start));
                    }
                }
                return 0;
             };
             if (result.prompt_tokens == 0) result.prompt_tokens = manualIntScan("prompt_eval_count");
             if (result.completion_tokens == 0) result.completion_tokens = manualIntScan("eval_count");
        }

    } catch (const std::exception& e) {
        std::cerr << "[AgentOllamaClient] ParseChatResponse: JSON stats parse error: " << e.what() << "\n";
        // Manual fallback for tokens if JSON fails
        auto scanToken = [&](const std::string& key) -> uint64_t {
            size_t pos = json_str.find("\"" + key + "\":");
            if (pos != std::string::npos) {
                size_t val_start = json_str.find_first_of("0123456789", pos + key.length() + 2);
                if (val_start != std::string::npos) {
                    size_t val_end = json_str.find_first_not_of("0123456789", val_start);
                    return std::stoull(json_str.substr(val_start, val_end - val_start));
                }
            }
            return 0;
        };
        result.prompt_tokens = scanToken("prompt_eval_count");
        result.completion_tokens = scanToken("eval_count");
    }

    return result;
}

InferenceResult AgentOllamaClient::ParseFIMResponse(const std::string& json_str) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);

        if (j.contains("error")) {
            return InferenceResult::error(j["error"].get<std::string>());
        }

        InferenceResult result;
        result.success = true;
        result.has_tool_calls = false;
        result.response = j.value("response", "");
        result.completion_tokens = j.value("eval_count", 0ULL);

        uint64_t evalDuration = j.value("eval_duration", 0ULL);
        result.total_duration_ms = j.value("total_duration", 0ULL) / 1e6;
        if (evalDuration > 0) {
            result.tokens_per_sec = static_cast<double>(result.completion_tokens) /
                                    (static_cast<double>(evalDuration) / 1e9);
        }

        return result;
    } catch (const std::exception& e) {
        return InferenceResult::error(std::string("JSON parse error: ") + e.what());
    } catch (...) {
        return InferenceResult::error("Unknown JSON parse error");
    }
}

// ---------------------------------------------------------------------------
// HTTP Implementation (WinHTTP)
// ---------------------------------------------------------------------------

#ifdef _WIN32

std::string AgentOllamaClient::MakeGetRequest(const std::string& endpoint) {
    std::cerr << "[AgentOllamaClient] MakeGetRequest: host=" << m_config.host
              << " port=" << m_config.port << " endpoint=" << endpoint << "\n";
    if (!m_hSession) {
        std::cerr << "[AgentOllamaClient] MakeGetRequest: no WinHTTP session\n";
        return "";
    }

    std::wstring wHost = ToWide(m_config.host);
    HINTERNET hConnect = WinHttpConnect(m_hSession, wHost.c_str(),
                                        static_cast<INTERNET_PORT>(m_config.port), 0);
    if (!hConnect) {
        std::cerr << "[AgentOllamaClient] MakeGetRequest: WinHttpConnect failed: "
                  << GetLastError() << "\n";
        return "";
    }

    std::wstring wEndpoint = ToWide(endpoint);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        std::cerr << "[AgentOllamaClient] MakeGetRequest: WinHttpOpenRequest failed: "
                  << GetLastError() << "\n";
        WinHttpCloseHandle(hConnect);
        return "";
    }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        std::cerr << "[AgentOllamaClient] MakeGetRequest: WinHttpSendRequest/Receive failed: "
                  << err << "\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        if (statusCode != 200) {
            std::cerr << "[AgentOllamaClient] MakeGetRequest: HTTP status " << statusCode << "\n";
        }
    } else {
        std::cerr << "[AgentOllamaClient] MakeGetRequest: failed to query status code\n";
    }

    std::string response;
    char buffer[4096];

    DWORD bytesRead = 0;
    while (true) {
        BOOL ok = WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead);
        if (!ok) {
            DWORD err = GetLastError();
            std::cerr << "[AgentOllamaClient] MakeGetRequest: WinHttpReadData failed: " << err << "\n";
            break;
        }
        if (bytesRead == 0) {
            break;
        }
        std::cerr << "[AgentOllamaClient] MakeGetRequest: read " << bytesRead << " bytes\n";
        for (DWORD i = 0; i < bytesRead; ++i) {
            if (buffer[i] != 0) {
                response.push_back(buffer[i]);
            } else {
                std::cerr << "[AgentOllamaClient] MakeGetRequest: STRIPPING NULL BYTE at index " << response.size() << "\n";
            }
        }
    }

    std::cerr << "[AgentOllamaClient] MakeGetRequest: final response size=" << response.size() << "\n";
    if (!response.empty()) {
        std::string snippet = response.substr(0, std::min<size_t>(response.size(), 128));
        std::cerr << "[AgentOllamaClient] MakeGetRequest: response snippet='" << snippet << "'\n";
        std::cerr << "[AgentOllamaClient] MakeGetRequest: response hex=";
        for (size_t i = 0; i < response.size() && i < 64; ++i) {
            std::cerr << " " << std::hex << std::setw(2) << std::setfill('0')
                      << (static_cast<unsigned int>(static_cast<unsigned char>(response[i])));
        }
        std::cerr << std::dec << "\n";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return response;
}

std::string AgentOllamaClient::MakePostRequest(const std::string& endpoint,
                                                const std::string& body) {
    std::cerr << "[AgentOllamaClient] MakePostRequest: host=" << m_config.host 
              << " port=" << m_config.port << " endpoint=" << endpoint << "\n";
    std::cerr << "[AgentOllamaClient] MakePostRequest: body='" << body << "'\n";
    if (!m_hSession) return "";

    std::wstring wHost = ToWide(m_config.host);
    HINTERNET hConnect = WinHttpConnect(m_hSession, wHost.c_str(),
                                        static_cast<INTERNET_PORT>(m_config.port), 0);
    if (!hConnect) return "";

    std::wstring wEndpoint = ToWide(endpoint);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        return "";
    }

    const wchar_t* contentType = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                   (LPVOID)body.data(), static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        std::cerr << "[AgentOllamaClient] MakePostRequest: WinHttpSendRequest/Receive failed: " << err << "\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    std::cerr << "[AgentOllamaClient] MakePostRequest: HTTP status " << statusCode << "\n";

    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    std::cerr << "[AgentOllamaClient] MakePostRequest: resp size=" << response.size() << " snippet='"
              << response.substr(0, std::min<size_t>(response.size(), 128)) << "'\n";

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return response;
}

bool AgentOllamaClient::MakeStreamingPost(
    const std::string& endpoint,
    const std::string& body,
    std::function<bool(const std::string& line)> on_line,
    ErrorCallback on_error)
{
    if (!m_hSession) {
        if (on_error) on_error("WinHTTP session not initialized");
        return false;
    }

    std::wstring wHost = ToWide(m_config.host);
    HINTERNET hConnect = WinHttpConnect(m_hSession, wHost.c_str(),
                                        static_cast<INTERNET_PORT>(m_config.port), 0);
    if (!hConnect) {
        if (on_error) on_error("Failed to connect to " + m_config.host);
        return false;
    }

    std::wstring wEndpoint = ToWide(endpoint);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        if (on_error) on_error("Failed to open request");
        return false;
    }

    const wchar_t* contentType = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                   (LPVOID)body.data(), static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()), 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        if (on_error) on_error("HTTP request failed: " + std::to_string(err));
        return false;
    }

    // Stream NDJSON line by line
    std::string lineBuffer;
    char buffer[4096];
    DWORD bytesRead;
    bool success = true;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (m_cancelRequested.load()) {
            success = false;
            break;
        }

        for (DWORD i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\n') {
                if (!lineBuffer.empty()) {
                    bool cont = on_line(lineBuffer);
                    lineBuffer.clear();
                    if (!cont) goto done;
                }
            } else {
                lineBuffer += buffer[i];
            }
        }
    }

    // Process any remaining data
    if (!lineBuffer.empty()) {
        on_line(lineBuffer);
    }

done:
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return success;
}

#else
// POSIX implementation using libcurl or raw sockets
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

// SCAFFOLD_065: AgentOllamaClient and HTTP


static int posix_connect(const std::string& host, int port) {
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) return -1;
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) { freeaddrinfo(result); return -1; }
    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        close(sock); freeaddrinfo(result); return -1;
    }
    freeaddrinfo(result);
    return sock;
}

std::string AgentOllamaClient::MakeGetRequest(const std::string& path) {
    int sock = posix_connect(m_host, m_port);
    if (sock < 0) return "";

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + m_host + "\r\nConnection: close\r\n\r\n";
    send(sock, req.c_str(), req.size(), 0);

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        response += buf;
    }
    close(sock);

    // Strip HTTP headers
    size_t bodyStart = response.find("\r\n\r\n");
    return (bodyStart != std::string::npos) ? response.substr(bodyStart + 4) : response;
}

std::string AgentOllamaClient::MakePostRequest(const std::string& path, const std::string& body) {
    int sock = posix_connect(m_host, m_port);
    if (sock < 0) return "";

    std::string req = "POST " + path + " HTTP/1.1\r\nHost: " + m_host +
        "\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    send(sock, req.c_str(), req.size(), 0);

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        response += buf;
    }
    close(sock);

    size_t bodyStart = response.find("\r\n\r\n");
    return (bodyStart != std::string::npos) ? response.substr(bodyStart + 4) : response;
}

bool AgentOllamaClient::MakeStreamingPost(const std::string& path, const std::string& body,
    std::function<bool(const std::string&)> on_line, ErrorCallback on_error) {
    int sock = posix_connect(m_host, m_port);
    if (sock < 0) {
        if (on_error) on_error("Failed to connect");
        return false;
    }

    std::string req = "POST " + path + " HTTP/1.1\r\nHost: " + m_host +
        "\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;
    send(sock, req.c_str(), req.size(), 0);

    // Skip HTTP headers
    std::string headerBuf;
    char c;
    while (recv(sock, &c, 1, 0) == 1) {
        headerBuf += c;
        if (headerBuf.size() >= 4 && headerBuf.substr(headerBuf.size()-4) == "\r\n\r\n") break;
    }

    // Read body line by line
    std::string lineBuf;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                if (!lineBuf.empty()) {
                    if (!on_line(lineBuf)) { close(sock); return true; }
                    lineBuf.clear();
                }
            } else if (buf[i] != '\r') {
                lineBuf += buf[i];
            }
        }
    }
    if (!lineBuf.empty()) on_line(lineBuf);
    close(sock);
    return true;
}
#endif
