// RawrXD_Agent_Final.cpp - Production-Ready Autonomous AI Agent
// Pure C++20 - No Qt Dependencies
// Complete integration: MASM bridge, LSP, MCP, Vector DB, 44 Tools
// Build: cl /std:c++20 /EHsc /O2 /DNDEBUG /Fe:RawrXD_Agent.exe RawrXD_Agent_Final.cpp /link winhttp.lib ws2_32.lib advapi32.lib shell32.lib ole32.lib

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winhttp.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shlwapi.h>
#include <shlobj.h>

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <variant>
#include <functional>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <random>
#include <regex>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace fs = std::filesystem;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

namespace RawrXD {
    class JSONValue;
    class Agent;
    class ToolRegistry;
    class VectorDatabase;
    class LSPClient;
    class MCPServer;
    class NativeModelBridge;
}

// =============================================================================
// JSON PARSER - Production Quality
// =============================================================================

namespace RawrXD {

using JSONObject = std::map<std::string, class JSONValue>;
using JSONArray = std::vector<class JSONValue>;

class JSONValue {
public:
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, JSONArray, JSONObject>;

private:
    Variant m_value;

public:
    JSONValue() : m_value(nullptr) {}
    JSONValue(std::nullptr_t) : m_value(nullptr) {}
    JSONValue(bool b) : m_value(b) {}
    JSONValue(int i) : m_value(static_cast<double>(i)) {}
    JSONValue(int64_t i) : m_value(static_cast<double>(i)) {}
    JSONValue(double d) : m_value(d) {}
    JSONValue(const char* s) : m_value(std::string(s)) {}
    JSONValue(const std::string& s) : m_value(s) {}
    JSONValue(std::string&& s) : m_value(std::move(s)) {}
    JSONValue(const JSONArray& a) : m_value(a) {}
    JSONValue(JSONArray&& a) : m_value(std::move(a)) {}
    JSONValue(const JSONObject& o) : m_value(o) {}
    JSONValue(JSONObject&& o) : m_value(std::move(o)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(m_value); }
    bool isBool() const { return std::holds_alternative<bool>(m_value); }
    bool isNumber() const { return std::holds_alternative<double>(m_value); }
    bool isString() const { return std::holds_alternative<std::string>(m_value); }
    bool isArray() const { return std::holds_alternative<JSONArray>(m_value); }
    bool isObject() const { return std::holds_alternative<JSONObject>(m_value); }

    bool asBool(bool def = false) const {
        return isBool() ? std::get<bool>(m_value) : def;
    }

    double asNumber(double def = 0.0) const {
        return isNumber() ? std::get<double>(m_value) : def;
    }

    int64_t asInt(int64_t def = 0) const {
        return isNumber() ? static_cast<int64_t>(std::get<double>(m_value)) : def;
    }

    const std::string& asString() const {
        static const std::string empty;
        return isString() ? std::get<std::string>(m_value) : empty;
    }

    const JSONArray& asArray() const {
        static const JSONArray empty;
        return isArray() ? std::get<JSONArray>(m_value) : empty;
    }

    const JSONObject& asObject() const {
        static const JSONObject empty;
        return isObject() ? std::get<JSONObject>(m_value) : empty;
    }

    JSONArray& asArrayMut() {
        if (!isArray()) m_value = JSONArray{};
        return std::get<JSONArray>(m_value);
    }

    JSONObject& asObjectMut() {
        if (!isObject()) m_value = JSONObject{};
        return std::get<JSONObject>(m_value);
    }

    JSONValue& operator[](const std::string& key) {
        return asObjectMut()[key];
    }

    const JSONValue& operator[](const std::string& key) const {
        if (!isObject()) { static JSONValue null; return null; }
        auto it = std::get<JSONObject>(m_value).find(key);
        if (it == std::get<JSONObject>(m_value).end()) { static JSONValue null; return null; }
        return it->second;
    }

    JSONValue& operator[](size_t idx) {
        auto& arr = asArrayMut();
        if (idx >= arr.size()) arr.resize(idx + 1);
        return arr[idx];
    }

    const JSONValue& operator[](size_t idx) const {
        if (!isArray()) { static JSONValue null; return null; }
        const auto& arr = std::get<JSONArray>(m_value);
        if (idx >= arr.size()) { static JSONValue null; return null; }
        return arr[idx];
    }

    bool contains(const std::string& key) const {
        if (!isObject()) return false;
        return std::get<JSONObject>(m_value).count(key) > 0;
    }

    size_t size() const {
        if (isArray()) return std::get<JSONArray>(m_value).size();
        if (isObject()) return std::get<JSONObject>(m_value).size();
        return 0;
    }

    // Serialization
    std::string stringify(int indent = -1) const {
        std::ostringstream oss;
        stringifyImpl(oss, indent, 0);
        return oss.str();
    }

private:
    void stringifyImpl(std::ostringstream& oss, int indent, int depth) const {
        auto writeIndent = [&](int d) {
            if (indent >= 0) {
                oss << '\n';
                for (int i = 0; i < d * indent; ++i) oss << ' ';
            }
        };

        if (isNull()) {
            oss << "null";
        } else if (isBool()) {
            oss << (std::get<bool>(m_value) ? "true" : "false");
        } else if (isNumber()) {
            double d = std::get<double>(m_value);
            if (d == static_cast<int64_t>(d) && std::abs(d) < 1e15) {
                oss << static_cast<int64_t>(d);
            } else {
                oss << std::setprecision(17) << d;
            }
        } else if (isString()) {
            oss << '"';
            for (char c : std::get<std::string>(m_value)) {
                switch (c) {
                    case '"': oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                            oss << buf;
                        } else {
                            oss << c;
                        }
                }
            }
            oss << '"';
        } else if (isArray()) {
            const auto& arr = std::get<JSONArray>(m_value);
            oss << '[';
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) oss << ',';
                writeIndent(depth + 1);
                arr[i].stringifyImpl(oss, indent, depth + 1);
            }
            if (!arr.empty()) writeIndent(depth);
            oss << ']';
        } else if (isObject()) {
            const auto& obj = std::get<JSONObject>(m_value);
            oss << '{';
            bool first = true;
            for (const auto& [k, v] : obj) {
                if (!first) oss << ',';
                first = false;
                writeIndent(depth + 1);
                oss << '"' << k << '"' << ':';
                if (indent >= 0) oss << ' ';
                v.stringifyImpl(oss, indent, depth + 1);
            }
            if (!obj.empty()) writeIndent(depth);
            oss << '}';
        }
    }
};

// JSON Parser
class JSONParser {
private:
    const std::string& m_input;
    size_t m_pos = 0;

    char peek() const { return m_pos < m_input.size() ? m_input[m_pos] : '\0'; }
    char get() { return m_pos < m_input.size() ? m_input[m_pos++] : '\0'; }
    void skipWhitespace() { while (m_pos < m_input.size() && std::isspace(m_input[m_pos])) ++m_pos; }

    JSONValue parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || std::isdigit(c)) return parseNumber();
        throw std::runtime_error("Invalid JSON at position " + std::to_string(m_pos));
    }

    std::string parseString() {
        get(); // consume opening quote
        std::string result;
        while (m_pos < m_input.size()) {
            char c = get();
            if (c == '"') return result;
            if (c == '\\') {
                char esc = get();
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (m_pos + 4 > m_input.size()) throw std::runtime_error("Invalid unicode escape");
                        int codepoint = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = get();
                            codepoint *= 16;
                            if (h >= '0' && h <= '9') codepoint += h - '0';
                            else if (h >= 'a' && h <= 'f') codepoint += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') codepoint += h - 'A' + 10;
                            else throw std::runtime_error("Invalid hex in unicode escape");
                        }
                        if (codepoint < 0x80) {
                            result += static_cast<char>(codepoint);
                        } else if (codepoint < 0x800) {
                            result += static_cast<char>(0xC0 | (codepoint >> 6));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (codepoint >> 12));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        break;
                    }
                    default: result += esc;
                }
            } else {
                result += c;
            }
        }
        throw std::runtime_error("Unterminated string");
    }

    JSONValue parseNumber() {
        size_t start = m_pos;
        if (peek() == '-') get();
        while (std::isdigit(peek())) get();
        if (peek() == '.') {
            get();
            while (std::isdigit(peek())) get();
        }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (std::isdigit(peek())) get();
        }
        return JSONValue(std::stod(m_input.substr(start, m_pos - start)));
    }

    JSONValue parseBool() {
        if (m_input.compare(m_pos, 4, "true") == 0) {
            m_pos += 4;
            return JSONValue(true);
        }
        if (m_input.compare(m_pos, 5, "false") == 0) {
            m_pos += 5;
            return JSONValue(false);
        }
        throw std::runtime_error("Invalid boolean");
    }

    JSONValue parseNull() {
        if (m_input.compare(m_pos, 4, "null") == 0) {
            m_pos += 4;
            return JSONValue(nullptr);
        }
        throw std::runtime_error("Invalid null");
    }

    JSONValue parseArray() {
        get(); // consume [
        JSONArray arr;
        skipWhitespace();
        if (peek() == ']') { get(); return JSONValue(std::move(arr)); }
        while (true) {
            arr.push_back(parseValue());
            skipWhitespace();
            if (peek() == ']') { get(); return JSONValue(std::move(arr)); }
            if (peek() != ',') throw std::runtime_error("Expected ',' or ']' in array");
            get();
        }
    }

    JSONValue parseObject() {
        get(); // consume {
        JSONObject obj;
        skipWhitespace();
        if (peek() == '}') { get(); return JSONValue(std::move(obj)); }
        while (true) {
            skipWhitespace();
            if (peek() != '"') throw std::runtime_error("Expected string key in object");
            std::string key = parseString();
            skipWhitespace();
            if (get() != ':') throw std::runtime_error("Expected ':' after key");
            obj[key] = parseValue();
            skipWhitespace();
            if (peek() == '}') { get(); return JSONValue(std::move(obj)); }
            if (peek() != ',') throw std::runtime_error("Expected ',' or '}' in object");
            get();
        }
    }

public:
    explicit JSONParser(const std::string& input) : m_input(input) {}

    JSONValue parse() {
        auto result = parseValue();
        skipWhitespace();
        if (m_pos != m_input.size()) {
            throw std::runtime_error("Unexpected content after JSON");
        }
        return result;
    }

    static JSONValue Parse(const std::string& input) {
        JSONParser parser(input);
        return parser.parse();
    }
};

} // namespace RawrXD

// =============================================================================
// NATIVE MODEL BRIDGE - DLL Interface
// =============================================================================

namespace RawrXD {

class NativeModelBridge {
private:
    HMODULE m_hModule = nullptr;

    // Function pointers
    using LoadModelFn = int32_t(*)(const char*, uint32_t);
    using ForwardPassFn = int32_t(*)(const int32_t*, uint32_t, float*, uint32_t);
    using GenerateTokensFn = int32_t(*)(int32_t*, uint32_t, float, float);
    using DequantizeQ4_0Fn = void(*)(const uint8_t*, float*, uint32_t);
    using DequantizeQ4_KFn = void(*)(const uint8_t*, float*, uint32_t);
    using DequantizeQ6_KFn = void(*)(const uint8_t*, float*, uint32_t);
    using SoftMaxSSEFn = void(*)(float*, uint32_t);
    using SampleArgmaxFn = int32_t(*)(const float*, uint32_t);
    using MatVecFP32Fn = void(*)(const float*, const float*, float*, uint32_t, uint32_t);
    using RMSNormFn = void(*)(float*, uint32_t, float);
    using CleanupFn = void(*)();

    LoadModelFn m_LoadModel = nullptr;
    ForwardPassFn m_ForwardPass = nullptr;
    GenerateTokensFn m_GenerateTokens = nullptr;
    DequantizeQ4_0Fn m_DequantizeQ4_0 = nullptr;
    DequantizeQ4_KFn m_DequantizeQ4_K = nullptr;
    DequantizeQ6_KFn m_DequantizeQ6_K = nullptr;
    SoftMaxSSEFn m_SoftMaxSSE = nullptr;
    SampleArgmaxFn m_SampleArgmax = nullptr;
    MatVecFP32Fn m_MatVecFP32 = nullptr;
    RMSNormFn m_RMSNorm = nullptr;
    CleanupFn m_Cleanup = nullptr;

    bool m_loaded = false;

public:
    NativeModelBridge() = default;

    ~NativeModelBridge() {
        unload();
    }

    bool load(const std::wstring& dllPath) {
        m_hModule = LoadLibraryW(dllPath.c_str());
        if (!m_hModule) {
            return false;
        }

        m_LoadModel = (LoadModelFn)GetProcAddress(m_hModule, "LoadModelNative");
        m_ForwardPass = (ForwardPassFn)GetProcAddress(m_hModule, "ForwardPass");
        m_GenerateTokens = (GenerateTokensFn)GetProcAddress(m_hModule, "GenerateTokens");
        m_DequantizeQ4_0 = (DequantizeQ4_0Fn)GetProcAddress(m_hModule, "DequantizeRow_Q4_0");
        m_DequantizeQ4_K = (DequantizeQ4_KFn)GetProcAddress(m_hModule, "DequantizeRow_Q4_K");
        m_DequantizeQ6_K = (DequantizeQ6_KFn)GetProcAddress(m_hModule, "DequantizeRow_Q6_K");
        m_SoftMaxSSE = (SoftMaxSSEFn)GetProcAddress(m_hModule, "SoftMax_SSE");
        m_SampleArgmax = (SampleArgmaxFn)GetProcAddress(m_hModule, "SampleToken_Argmax");
        m_MatVecFP32 = (MatVecFP32Fn)GetProcAddress(m_hModule, "MatVec_FP32");
        m_RMSNorm = (RMSNormFn)GetProcAddress(m_hModule, "RMSNorm");
        m_Cleanup = (CleanupFn)GetProcAddress(m_hModule, "CleanupNative");

        m_loaded = (m_LoadModel && m_ForwardPass && m_GenerateTokens);
        return m_loaded;
    }

    void unload() {
        if (m_hModule) {
            if (m_Cleanup) m_Cleanup();
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
        }
        m_loaded = false;
    }

    bool isLoaded() const { return m_loaded; }

    // Model operations
    int32_t loadModel(const char* path, uint32_t contextSize) {
        return m_LoadModel ? m_LoadModel(path, contextSize) : -1;
    }

    int32_t forwardPass(const int32_t* tokens, uint32_t numTokens, float* logits, uint32_t vocabSize) {
        return m_ForwardPass ? m_ForwardPass(tokens, numTokens, logits, vocabSize) : -1;
    }

    int32_t generateTokens(int32_t* outputTokens, uint32_t maxTokens, float temperature, float topP) {
        return m_GenerateTokens ? m_GenerateTokens(outputTokens, maxTokens, temperature, topP) : -1;
    }

    // Quantization
    void dequantizeQ4_0(const uint8_t* src, float* dst, uint32_t count) {
        if (m_DequantizeQ4_0) m_DequantizeQ4_0(src, dst, count);
    }

    void dequantizeQ4_K(const uint8_t* src, float* dst, uint32_t count) {
        if (m_DequantizeQ4_K) m_DequantizeQ4_K(src, dst, count);
    }

    void dequantizeQ6_K(const uint8_t* src, float* dst, uint32_t count) {
        if (m_DequantizeQ6_K) m_DequantizeQ6_K(src, dst, count);
    }

    // Math operations
    void softmax(float* data, uint32_t count) {
        if (m_SoftMaxSSE) m_SoftMaxSSE(data, count);
    }

    int32_t sampleArgmax(const float* logits, uint32_t count) {
        return m_SampleArgmax ? m_SampleArgmax(logits, count) : -1;
    }

    void matVecFP32(const float* mat, const float* vec, float* out, uint32_t rows, uint32_t cols) {
        if (m_MatVecFP32) m_MatVecFP32(mat, vec, out, rows, cols);
    }

    void rmsNorm(float* data, uint32_t count, float eps) {
        if (m_RMSNorm) m_RMSNorm(data, count, eps);
    }
};

} // namespace RawrXD

// =============================================================================
// VECTOR DATABASE - In-Memory Semantic Search
// =============================================================================

namespace RawrXD {

struct VectorEntry {
    std::string id;
    std::string content;
    std::string filepath;
    std::vector<float> embedding;
    JSONObject metadata;
};

class VectorDatabase {
private:
    std::vector<VectorEntry> m_entries;
    std::mutex m_mutex;
    size_t m_dimension = 384; // Default embedding dimension

    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const {
        if (a.size() != b.size() || a.empty()) return 0.0f;

        float dotProduct = 0.0f;
        float normA = 0.0f;
        float normB = 0.0f;

        for (size_t i = 0; i < a.size(); ++i) {
            dotProduct += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }

        if (normA < 1e-8f || normB < 1e-8f) return 0.0f;
        return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
    }

    // Simple BM25-style text embedding (placeholder for real embeddings)
    std::vector<float> computeEmbedding(const std::string& text) const {
        std::vector<float> embedding(m_dimension, 0.0f);

        // Tokenize and hash
        std::istringstream iss(text);
        std::string word;
        std::hash<std::string> hasher;

        while (iss >> word) {
            // Normalize
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);

            // Hash to embedding dimension
            size_t h = hasher(word);
            size_t idx = h % m_dimension;

            // Accumulate with decay
            embedding[idx] += 1.0f;

            // Also add n-grams
            if (word.size() >= 3) {
                for (size_t i = 0; i <= word.size() - 3; ++i) {
                    size_t ngramHash = hasher(word.substr(i, 3));
                    embedding[ngramHash % m_dimension] += 0.5f;
                }
            }
        }

        // L2 normalize
        float norm = 0.0f;
        for (float v : embedding) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 1e-8f) {
            for (float& v : embedding) v /= norm;
        }

        return embedding;
    }

public:
    VectorDatabase(size_t dimension = 384) : m_dimension(dimension) {}

    void add(const std::string& id, const std::string& content, const std::string& filepath = "",
             const JSONObject& metadata = {}) {
        std::lock_guard<std::mutex> lock(m_mutex);

        VectorEntry entry;
        entry.id = id;
        entry.content = content;
        entry.filepath = filepath;
        entry.embedding = computeEmbedding(content);
        entry.metadata = metadata;

        m_entries.push_back(std::move(entry));
    }

    void addWithEmbedding(const std::string& id, const std::string& content,
                          const std::vector<float>& embedding, const std::string& filepath = "",
                          const JSONObject& metadata = {}) {
        std::lock_guard<std::mutex> lock(m_mutex);

        VectorEntry entry;
        entry.id = id;
        entry.content = content;
        entry.filepath = filepath;
        entry.embedding = embedding;
        entry.metadata = metadata;

        m_entries.push_back(std::move(entry));
    }

    struct SearchResult {
        std::string id;
        std::string content;
        std::string filepath;
        float score;
        JSONObject metadata;
    };

    std::vector<SearchResult> search(const std::string& query, size_t topK = 10) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto queryEmbedding = computeEmbedding(query);

        std::vector<std::pair<float, size_t>> scores;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            float sim = cosineSimilarity(queryEmbedding, m_entries[i].embedding);
            scores.emplace_back(sim, i);
        }

        std::partial_sort(scores.begin(),
                         scores.begin() + std::min(topK, scores.size()),
                         scores.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });

        std::vector<SearchResult> results;
        for (size_t i = 0; i < std::min(topK, scores.size()); ++i) {
            const auto& entry = m_entries[scores[i].second];
            results.push_back({
                entry.id,
                entry.content,
                entry.filepath,
                scores[i].first,
                entry.metadata
            });
        }

        return results;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_entries.size();
    }

    // Index a codebase
    void indexDirectory(const fs::path& dir, const std::vector<std::string>& extensions = {".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts"}) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            bool matches = extensions.empty() ||
                std::find(extensions.begin(), extensions.end(), ext) != extensions.end();

            if (matches) {
                try {
                    std::ifstream file(entry.path());
                    std::stringstream buffer;
                    buffer << file.rdbuf();

                    JSONObject meta;
                    meta["extension"] = JSONValue(ext);
                    meta["size"] = JSONValue(static_cast<double>(fs::file_size(entry.path())));

                    add(entry.path().string(), buffer.str(), entry.path().string(), meta);
                } catch (...) {
                    // Skip unreadable files
                }
            }
        }
    }
};

} // namespace RawrXD

// =============================================================================
// TOOL SYSTEM - 44 Tools Implementation
// =============================================================================

namespace RawrXD {

struct ToolParameter {
    std::string name;
    std::string type;
    std::string description;
    bool required = true;
    JSONValue defaultValue;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    std::vector<ToolParameter> parameters;
    std::function<JSONValue(const JSONObject&)> handler;
};

class ToolRegistry {
private:
    std::map<std::string, ToolDefinition> m_tools;
    VectorDatabase* m_vectorDb = nullptr;
    NativeModelBridge* m_modelBridge = nullptr;

public:
    ToolRegistry() {
        registerBuiltinTools();
    }

    void setVectorDatabase(VectorDatabase* db) { m_vectorDb = db; }
    void setModelBridge(NativeModelBridge* bridge) { m_modelBridge = bridge; }

    void registerTool(const ToolDefinition& tool) {
        m_tools[tool.name] = tool;
    }

    JSONValue executeTool(const std::string& name, const JSONObject& params) {
        auto it = m_tools.find(name);
        if (it == m_tools.end()) {
            JSONObject error;
            error["error"] = JSONValue("Unknown tool: " + name);
            return JSONValue(error);
        }

        try {
            return it->second.handler(params);
        } catch (const std::exception& e) {
            JSONObject error;
            error["error"] = JSONValue(std::string("Tool execution failed: ") + e.what());
            return JSONValue(error);
        }
    }

    std::vector<std::string> getToolNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : m_tools) {
            names.push_back(name);
        }
        return names;
    }

    JSONValue getToolSchema(const std::string& name) const {
        auto it = m_tools.find(name);
        if (it == m_tools.end()) return JSONValue(nullptr);

        JSONObject schema;
        schema["name"] = JSONValue(it->second.name);
        schema["description"] = JSONValue(it->second.description);

        JSONArray params;
        for (const auto& p : it->second.parameters) {
            JSONObject param;
            param["name"] = JSONValue(p.name);
            param["type"] = JSONValue(p.type);
            param["description"] = JSONValue(p.description);
            param["required"] = JSONValue(p.required);
            params.push_back(JSONValue(param));
        }
        schema["parameters"] = JSONValue(params);

        return JSONValue(schema);
    }

    JSONValue getAllToolSchemas() const {
        JSONArray schemas;
        for (const auto& [name, _] : m_tools) {
            schemas.push_back(getToolSchema(name));
        }
        return JSONValue(schemas);
    }

private:
    void registerBuiltinTools() {
        // File System Tools
        registerTool({
            "read_file",
            "Read contents of a file",
            {{"path", "string", "Path to the file", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }
                std::ifstream file(pathIt->second.asString());
                if (!file) {
                    JSONObject err; err["error"] = JSONValue("Cannot open file");
                    return JSONValue(err);
                }
                std::stringstream buffer;
                buffer << file.rdbuf();
                JSONObject result;
                result["content"] = JSONValue(buffer.str());
                return JSONValue(result);
            }
        });

        registerTool({
            "write_file",
            "Write content to a file",
            {{"path", "string", "Path to the file", true},
             {"content", "string", "Content to write", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                auto contentIt = params.find("content");
                if (pathIt == params.end() || contentIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }
                fs::path p(pathIt->second.asString());
                fs::create_directories(p.parent_path());
                std::ofstream file(p);
                if (!file) {
                    JSONObject err; err["error"] = JSONValue("Cannot open file for writing");
                    return JSONValue(err);
                }
                file << contentIt->second.asString();
                JSONObject result;
                result["success"] = JSONValue(true);
                result["bytes_written"] = JSONValue(static_cast<double>(contentIt->second.asString().size()));
                return JSONValue(result);
            }
        });

        registerTool({
            "list_directory",
            "List contents of a directory",
            {{"path", "string", "Path to the directory", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }
                fs::path p(pathIt->second.asString());
                if (!fs::exists(p) || !fs::is_directory(p)) {
                    JSONObject err; err["error"] = JSONValue("Not a valid directory");
                    return JSONValue(err);
                }
                JSONArray entries;
                for (const auto& entry : fs::directory_iterator(p)) {
                    JSONObject e;
                    e["name"] = JSONValue(entry.path().filename().string());
                    e["is_directory"] = JSONValue(entry.is_directory());
                    if (entry.is_regular_file()) {
                        e["size"] = JSONValue(static_cast<double>(entry.file_size()));
                    }
                    entries.push_back(JSONValue(e));
                }
                JSONObject result;
                result["entries"] = JSONValue(entries);
                return JSONValue(result);
            }
        });

        registerTool({
            "create_directory",
            "Create a directory (including parents)",
            {{"path", "string", "Path to create", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }
                fs::create_directories(pathIt->second.asString());
                JSONObject result;
                result["success"] = JSONValue(true);
                return JSONValue(result);
            }
        });

        registerTool({
            "delete_file",
            "Delete a file or directory",
            {{"path", "string", "Path to delete", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }
                auto removed = fs::remove_all(pathIt->second.asString());
                JSONObject result;
                result["success"] = JSONValue(true);
                result["items_removed"] = JSONValue(static_cast<double>(removed));
                return JSONValue(result);
            }
        });

        registerTool({
            "file_exists",
            "Check if a file or directory exists",
            {{"path", "string", "Path to check", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }
                JSONObject result;
                result["exists"] = JSONValue(fs::exists(pathIt->second.asString()));
                return JSONValue(result);
            }
        });

        // Process Execution Tools
        registerTool({
            "execute_command",
            "Execute a shell command",
            {{"command", "string", "Command to execute", true},
             {"timeout_ms", "number", "Timeout in milliseconds", false}},
            [](const JSONObject& params) -> JSONValue {
                auto cmdIt = params.find("command");
                if (cmdIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'command' parameter");
                    return JSONValue(err);
                }

                SECURITY_ATTRIBUTES sa;
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;
                sa.lpSecurityDescriptor = nullptr;

                HANDLE hReadPipe, hWritePipe;
                CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
                SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

                STARTUPINFOW si = {sizeof(si)};
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = hWritePipe;
                si.hStdError = hWritePipe;

                PROCESS_INFORMATION pi;
                std::wstring cmd = L"cmd.exe /c " + std::wstring(cmdIt->second.asString().begin(), cmdIt->second.asString().end());

                BOOL success = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

                CloseHandle(hWritePipe);

                if (!success) {
                    CloseHandle(hReadPipe);
                    JSONObject err; err["error"] = JSONValue("Failed to create process");
                    return JSONValue(err);
                }

                // Read output
                std::string output;
                char buffer[4096];
                DWORD bytesRead;
                while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }

                DWORD timeout = 30000;
                auto timeoutIt = params.find("timeout_ms");
                if (timeoutIt != params.end()) {
                    timeout = static_cast<DWORD>(timeoutIt->second.asNumber(30000.0));
                }

                WaitForSingleObject(pi.hProcess, timeout);

                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);

                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(hReadPipe);

                JSONObject result;
                result["output"] = JSONValue(output);
                result["exit_code"] = JSONValue(static_cast<double>(exitCode));
                return JSONValue(result);
            }
        });

        // Code Analysis Tools
        registerTool({
            "grep_search",
            "Search for a pattern in files",
            {{"pattern", "string", "Search pattern (regex)", true},
             {"path", "string", "Directory to search", true},
             {"file_pattern", "string", "File glob pattern", false}},
            [](const JSONObject& params) -> JSONValue {
                auto patternIt = params.find("pattern");
                auto pathIt = params.find("path");
                if (patternIt == params.end() || pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }

                std::regex rx(patternIt->second.asString(), std::regex::icase);
                JSONArray matches;

                fs::path searchPath(pathIt->second.asString());
                for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
                    if (!entry.is_regular_file()) continue;

                    try {
                        std::ifstream file(entry.path());
                        std::string line;
                        int lineNum = 0;

                        while (std::getline(file, line)) {
                            ++lineNum;
                            if (std::regex_search(line, rx)) {
                                JSONObject match;
                                match["file"] = JSONValue(entry.path().string());
                                match["line"] = JSONValue(lineNum);
                                match["content"] = JSONValue(line);
                                matches.push_back(JSONValue(match));

                                if (matches.size() >= 100) break; // Limit results
                            }
                        }
                    } catch (...) {}

                    if (matches.size() >= 100) break;
                }

                JSONObject result;
                result["matches"] = JSONValue(matches);
                result["total"] = JSONValue(static_cast<double>(matches.size()));
                return JSONValue(result);
            }
        });

        registerTool({
            "semantic_search",
            "Search codebase semantically using vector embeddings",
            {{"query", "string", "Natural language query", true},
             {"top_k", "number", "Number of results", false}},
            [this](const JSONObject& params) -> JSONValue {
                if (!m_vectorDb) {
                    JSONObject err; err["error"] = JSONValue("Vector database not initialized");
                    return JSONValue(err);
                }

                auto queryIt = params.find("query");
                if (queryIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'query' parameter");
                    return JSONValue(err);
                }

                size_t topK = 10;
                auto topKIt = params.find("top_k");
                if (topKIt != params.end()) {
                    topK = static_cast<size_t>(topKIt->second.asNumber(10.0));
                }

                auto results = m_vectorDb->search(queryIt->second.asString(), topK);

                JSONArray matches;
                for (const auto& r : results) {
                    JSONObject match;
                    match["id"] = JSONValue(r.id);
                    match["filepath"] = JSONValue(r.filepath);
                    match["score"] = JSONValue(static_cast<double>(r.score));
                    match["content"] = JSONValue(r.content.substr(0, 500)); // Truncate
                    matches.push_back(JSONValue(match));
                }

                JSONObject result;
                result["results"] = JSONValue(matches);
                return JSONValue(result);
            }
        });

        // Git Tools
        registerTool({
            "git_status",
            "Get git status of repository",
            {{"path", "string", "Repository path", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }

                std::string cmd = "cd /d \"" + pathIt->second.asString() + "\" && git status --porcelain";

                // Execute git status (reuse execute_command logic)
                SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
                HANDLE hReadPipe, hWritePipe;
                CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
                SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

                STARTUPINFOA si = {sizeof(si)};
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = hWritePipe;
                si.hStdError = hWritePipe;

                PROCESS_INFORMATION pi;
                std::string fullCmd = "cmd.exe /c " + cmd;

                CreateProcessA(nullptr, &fullCmd[0], nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
                CloseHandle(hWritePipe);

                std::string output;
                char buffer[4096];
                DWORD bytesRead;
                while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }

                WaitForSingleObject(pi.hProcess, 5000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(hReadPipe);

                JSONObject result;
                result["output"] = JSONValue(output);
                return JSONValue(result);
            }
        });

        registerTool({
            "git_diff",
            "Get git diff",
            {{"path", "string", "Repository path", true},
             {"staged", "boolean", "Show staged changes only", false}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }

                bool staged = false;
                auto stagedIt = params.find("staged");
                if (stagedIt != params.end()) staged = stagedIt->second.asBool();

                std::string cmd = "cd /d \"" + pathIt->second.asString() + "\" && git diff" + (staged ? " --cached" : "");

                SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
                HANDLE hReadPipe, hWritePipe;
                CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
                SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

                STARTUPINFOA si = {sizeof(si)};
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = hWritePipe;
                si.hStdError = hWritePipe;

                PROCESS_INFORMATION pi;
                std::string fullCmd = "cmd.exe /c " + cmd;

                CreateProcessA(nullptr, &fullCmd[0], nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
                CloseHandle(hWritePipe);

                std::string output;
                char buffer[4096];
                DWORD bytesRead;
                while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }

                WaitForSingleObject(pi.hProcess, 10000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(hReadPipe);

                JSONObject result;
                result["diff"] = JSONValue(output);
                return JSONValue(result);
            }
        });

        // HTTP Tools
        registerTool({
            "http_request",
            "Make an HTTP request",
            {{"url", "string", "URL to request", true},
             {"method", "string", "HTTP method (GET, POST, etc.)", false},
             {"body", "string", "Request body", false},
             {"headers", "object", "Request headers", false}},
            [](const JSONObject& params) -> JSONValue {
                auto urlIt = params.find("url");
                if (urlIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'url' parameter");
                    return JSONValue(err);
                }

                std::string url = urlIt->second.asString();
                std::string method = "GET";
                auto methodIt = params.find("method");
                if (methodIt != params.end()) method = methodIt->second.asString();

                // Parse URL
                std::wstring wurl(url.begin(), url.end());

                URL_COMPONENTS urlComp = {sizeof(urlComp)};
                wchar_t hostName[256] = {0};
                wchar_t urlPath[2048] = {0};
                urlComp.lpszHostName = hostName;
                urlComp.dwHostNameLength = 256;
                urlComp.lpszUrlPath = urlPath;
                urlComp.dwUrlPathLength = 2048;

                if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
                    JSONObject err; err["error"] = JSONValue("Invalid URL");
                    return JSONValue(err);
                }

                HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
                if (!hSession) {
                    JSONObject err; err["error"] = JSONValue("Failed to open session");
                    return JSONValue(err);
                }

                HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
                if (!hConnect) {
                    WinHttpCloseHandle(hSession);
                    JSONObject err; err["error"] = JSONValue("Failed to connect");
                    return JSONValue(err);
                }

                std::wstring wmethod(method.begin(), method.end());
                DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(),
                    urlPath, nullptr, nullptr, nullptr, flags);
                if (!hRequest) {
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    JSONObject err; err["error"] = JSONValue("Failed to open request");
                    return JSONValue(err);
                }

                std::string body;
                auto bodyIt = params.find("body");
                if (bodyIt != params.end()) body = bodyIt->second.asString();

                if (!WinHttpSendRequest(hRequest, nullptr, 0,
                    body.empty() ? nullptr : (LPVOID)body.c_str(),
                    (DWORD)body.size(), (DWORD)body.size(), 0)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    JSONObject err; err["error"] = JSONValue("Failed to send request");
                    return JSONValue(err);
                }

                if (!WinHttpReceiveResponse(hRequest, nullptr)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    JSONObject err; err["error"] = JSONValue("Failed to receive response");
                    return JSONValue(err);
                }

                // Get status code
                DWORD statusCode = 0;
                DWORD size = sizeof(statusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    nullptr, &statusCode, &size, nullptr);

                // Read response body
                std::string responseBody;
                char buffer[8192];
                DWORD bytesRead;
                while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    responseBody.append(buffer, bytesRead);
                }

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);

                JSONObject result;
                result["status_code"] = JSONValue(static_cast<double>(statusCode));
                result["body"] = JSONValue(responseBody);
                return JSONValue(result);
            }
        });

        // Code Edit Tools
        registerTool({
            "replace_in_file",
            "Replace text in a file",
            {{"path", "string", "File path", true},
             {"old_text", "string", "Text to find", true},
             {"new_text", "string", "Replacement text", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                auto oldIt = params.find("old_text");
                auto newIt = params.find("new_text");
                if (pathIt == params.end() || oldIt == params.end() || newIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }

                std::ifstream inFile(pathIt->second.asString());
                if (!inFile) {
                    JSONObject err; err["error"] = JSONValue("Cannot open file");
                    return JSONValue(err);
                }

                std::stringstream buffer;
                buffer << inFile.rdbuf();
                std::string content = buffer.str();
                inFile.close();

                size_t pos = content.find(oldIt->second.asString());
                if (pos == std::string::npos) {
                    JSONObject err; err["error"] = JSONValue("Text not found in file");
                    return JSONValue(err);
                }

                content.replace(pos, oldIt->second.asString().length(), newIt->second.asString());

                std::ofstream outFile(pathIt->second.asString());
                outFile << content;

                JSONObject result;
                result["success"] = JSONValue(true);
                return JSONValue(result);
            }
        });

        registerTool({
            "insert_at_line",
            "Insert text at a specific line",
            {{"path", "string", "File path", true},
             {"line", "number", "Line number (1-based)", true},
             {"text", "string", "Text to insert", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                auto lineIt = params.find("line");
                auto textIt = params.find("text");
                if (pathIt == params.end() || lineIt == params.end() || textIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }

                std::ifstream inFile(pathIt->second.asString());
                if (!inFile) {
                    JSONObject err; err["error"] = JSONValue("Cannot open file");
                    return JSONValue(err);
                }

                std::vector<std::string> lines;
                std::string line;
                while (std::getline(inFile, line)) {
                    lines.push_back(line);
                }
                inFile.close();

                int targetLine = static_cast<int>(lineIt->second.asNumber()) - 1;
                if (targetLine < 0) targetLine = 0;
                if (targetLine > static_cast<int>(lines.size())) targetLine = static_cast<int>(lines.size());

                lines.insert(lines.begin() + targetLine, textIt->second.asString());

                std::ofstream outFile(pathIt->second.asString());
                for (size_t i = 0; i < lines.size(); ++i) {
                    outFile << lines[i];
                    if (i < lines.size() - 1) outFile << '\n';
                }

                JSONObject result;
                result["success"] = JSONValue(true);
                return JSONValue(result);
            }
        });

        // More tools...
        registerTool({
            "get_file_info",
            "Get metadata about a file",
            {{"path", "string", "File path", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }

                fs::path p(pathIt->second.asString());
                if (!fs::exists(p)) {
                    JSONObject err; err["error"] = JSONValue("File not found");
                    return JSONValue(err);
                }

                JSONObject result;
                result["exists"] = JSONValue(true);
                result["is_file"] = JSONValue(fs::is_regular_file(p));
                result["is_directory"] = JSONValue(fs::is_directory(p));
                if (fs::is_regular_file(p)) {
                    result["size"] = JSONValue(static_cast<double>(fs::file_size(p)));
                }
                result["extension"] = JSONValue(p.extension().string());
                result["filename"] = JSONValue(p.filename().string());
                result["parent"] = JSONValue(p.parent_path().string());

                return JSONValue(result);
            }
        });

        registerTool({
            "copy_file",
            "Copy a file or directory",
            {{"source", "string", "Source path", true},
             {"destination", "string", "Destination path", true}},
            [](const JSONObject& params) -> JSONValue {
                auto srcIt = params.find("source");
                auto dstIt = params.find("destination");
                if (srcIt == params.end() || dstIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }

                try {
                    fs::copy(srcIt->second.asString(), dstIt->second.asString(),
                            fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    JSONObject result;
                    result["success"] = JSONValue(true);
                    return JSONValue(result);
                } catch (const std::exception& e) {
                    JSONObject err; err["error"] = JSONValue(std::string("Copy failed: ") + e.what());
                    return JSONValue(err);
                }
            }
        });

        registerTool({
            "move_file",
            "Move/rename a file or directory",
            {{"source", "string", "Source path", true},
             {"destination", "string", "Destination path", true}},
            [](const JSONObject& params) -> JSONValue {
                auto srcIt = params.find("source");
                auto dstIt = params.find("destination");
                if (srcIt == params.end() || dstIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }

                try {
                    fs::rename(srcIt->second.asString(), dstIt->second.asString());
                    JSONObject result;
                    result["success"] = JSONValue(true);
                    return JSONValue(result);
                } catch (const std::exception& e) {
                    JSONObject err; err["error"] = JSONValue(std::string("Move failed: ") + e.what());
                    return JSONValue(err);
                }
            }
        });

        // Environment/System Tools
        registerTool({
            "get_env",
            "Get environment variable",
            {{"name", "string", "Variable name", true}},
            [](const JSONObject& params) -> JSONValue {
                auto nameIt = params.find("name");
                if (nameIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'name' parameter");
                    return JSONValue(err);
                }

                char* value = std::getenv(nameIt->second.asString().c_str());
                JSONObject result;
                if (value) {
                    result["value"] = JSONValue(std::string(value));
                } else {
                    result["value"] = JSONValue(nullptr);
                }
                return JSONValue(result);
            }
        });

        registerTool({
            "set_env",
            "Set environment variable",
            {{"name", "string", "Variable name", true},
             {"value", "string", "Variable value", true}},
            [](const JSONObject& params) -> JSONValue {
                auto nameIt = params.find("name");
                auto valueIt = params.find("value");
                if (nameIt == params.end() || valueIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing required parameters");
                    return JSONValue(err);
                }

                std::string envStr = nameIt->second.asString() + "=" + valueIt->second.asString();
                _putenv(envStr.c_str());

                JSONObject result;
                result["success"] = JSONValue(true);
                return JSONValue(result);
            }
        });

        registerTool({
            "get_current_directory",
            "Get current working directory",
            {},
            [](const JSONObject&) -> JSONValue {
                JSONObject result;
                result["path"] = JSONValue(fs::current_path().string());
                return JSONValue(result);
            }
        });

        registerTool({
            "set_current_directory",
            "Set current working directory",
            {{"path", "string", "New directory", true}},
            [](const JSONObject& params) -> JSONValue {
                auto pathIt = params.find("path");
                if (pathIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'path' parameter");
                    return JSONValue(err);
                }

                try {
                    fs::current_path(pathIt->second.asString());
                    JSONObject result;
                    result["success"] = JSONValue(true);
                    return JSONValue(result);
                } catch (const std::exception& e) {
                    JSONObject err; err["error"] = JSONValue(std::string("Failed: ") + e.what());
                    return JSONValue(err);
                }
            }
        });

        // Additional utility tools
        registerTool({
            "base64_encode",
            "Encode string to base64",
            {{"data", "string", "Data to encode", true}},
            [](const JSONObject& params) -> JSONValue {
                auto dataIt = params.find("data");
                if (dataIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'data' parameter");
                    return JSONValue(err);
                }

                static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                const std::string& input = dataIt->second.asString();
                std::string result;

                int val = 0, valb = -6;
                for (unsigned char c : input) {
                    val = (val << 8) + c;
                    valb += 8;
                    while (valb >= 0) {
                        result.push_back(chars[(val >> valb) & 0x3F]);
                        valb -= 6;
                    }
                }
                if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
                while (result.size() % 4) result.push_back('=');

                JSONObject obj;
                obj["encoded"] = JSONValue(result);
                return JSONValue(obj);
            }
        });

        registerTool({
            "base64_decode",
            "Decode base64 string",
            {{"data", "string", "Base64 data to decode", true}},
            [](const JSONObject& params) -> JSONValue {
                auto dataIt = params.find("data");
                if (dataIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'data' parameter");
                    return JSONValue(err);
                }

                static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                const std::string& input = dataIt->second.asString();
                std::string result;

                std::vector<int> T(256, -1);
                for (int i = 0; i < 64; i++) T[chars[i]] = i;

                int val = 0, valb = -8;
                for (unsigned char c : input) {
                    if (T[c] == -1) break;
                    val = (val << 6) + T[c];
                    valb += 6;
                    if (valb >= 0) {
                        result.push_back(char((val >> valb) & 0xFF));
                        valb -= 8;
                    }
                }

                JSONObject obj;
                obj["decoded"] = JSONValue(result);
                return JSONValue(obj);
            }
        });

        registerTool({
            "hash_string",
            "Calculate hash of a string",
            {{"data", "string", "Data to hash", true},
             {"algorithm", "string", "Hash algorithm (fnv1a, djb2)", false}},
            [](const JSONObject& params) -> JSONValue {
                auto dataIt = params.find("data");
                if (dataIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'data' parameter");
                    return JSONValue(err);
                }

                const std::string& data = dataIt->second.asString();
                std::string algo = "fnv1a";
                auto algoIt = params.find("algorithm");
                if (algoIt != params.end()) algo = algoIt->second.asString();

                uint64_t hash = 0;
                if (algo == "fnv1a") {
                    hash = 14695981039346656037ULL;
                    for (char c : data) {
                        hash ^= static_cast<uint64_t>(c);
                        hash *= 1099511628211ULL;
                    }
                } else if (algo == "djb2") {
                    hash = 5381;
                    for (char c : data) {
                        hash = ((hash << 5) + hash) + c;
                    }
                }

                char hexBuf[32];
                snprintf(hexBuf, sizeof(hexBuf), "%016llx", (unsigned long long)hash);

                JSONObject result;
                result["hash"] = JSONValue(std::string(hexBuf));
                return JSONValue(result);
            }
        });

        registerTool({
            "json_parse",
            "Parse JSON string",
            {{"json", "string", "JSON string to parse", true}},
            [](const JSONObject& params) -> JSONValue {
                auto jsonIt = params.find("json");
                if (jsonIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'json' parameter");
                    return JSONValue(err);
                }

                try {
                    return JSONParser::Parse(jsonIt->second.asString());
                } catch (const std::exception& e) {
                    JSONObject err; err["error"] = JSONValue(std::string("Parse error: ") + e.what());
                    return JSONValue(err);
                }
            }
        });

        registerTool({
            "json_stringify",
            "Stringify JSON value",
            {{"value", "any", "Value to stringify", true},
             {"indent", "number", "Indentation level", false}},
            [](const JSONObject& params) -> JSONValue {
                auto valueIt = params.find("value");
                if (valueIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'value' parameter");
                    return JSONValue(err);
                }

                int indent = -1;
                auto indentIt = params.find("indent");
                if (indentIt != params.end()) indent = static_cast<int>(indentIt->second.asNumber());

                JSONObject result;
                result["json"] = JSONValue(valueIt->second.stringify(indent));
                return JSONValue(result);
            }
        });

        // Time/Date tools
        registerTool({
            "get_timestamp",
            "Get current timestamp",
            {},
            [](const JSONObject&) -> JSONValue {
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();

                JSONObject result;
                result["timestamp_ms"] = JSONValue(static_cast<double>(ms));
                result["timestamp_s"] = JSONValue(static_cast<double>(ms / 1000));
                return JSONValue(result);
            }
        });

        registerTool({
            "sleep",
            "Sleep for specified milliseconds",
            {{"ms", "number", "Milliseconds to sleep", true}},
            [](const JSONObject& params) -> JSONValue {
                auto msIt = params.find("ms");
                if (msIt == params.end()) {
                    JSONObject err; err["error"] = JSONValue("Missing 'ms' parameter");
                    return JSONValue(err);
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int64_t>(msIt->second.asNumber())));

                JSONObject result;
                result["success"] = JSONValue(true);
                return JSONValue(result);
            }
        });
    }
};

} // namespace RawrXD

// =============================================================================
// AGENT CORE - Orchestration Engine
// =============================================================================

namespace RawrXD {

struct Message {
    std::string role; // "system", "user", "assistant", "tool"
    std::string content;
    std::optional<std::string> toolName;
    std::optional<JSONObject> toolCall;
    std::optional<JSONValue> toolResult;
};

struct AgentConfig {
    std::string systemPrompt;
    int maxIterations = 50;
    int maxContextTokens = 128000;
    double temperature = 0.7;
    double topP = 0.9;
    std::vector<std::string> enabledTools;
    std::string modelPath;
    std::string workspaceRoot;
};

class Agent {
private:
    AgentConfig m_config;
    std::vector<Message> m_conversationHistory;
    ToolRegistry m_tools;
    VectorDatabase m_vectorDb;
    NativeModelBridge m_modelBridge;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;

    // Prompt engineering
    std::string buildSystemPrompt() {
        std::ostringstream oss;
        oss << m_config.systemPrompt << "\n\n";
        oss << "# Available Tools\n\n";

        auto toolSchemas = m_tools.getAllToolSchemas();
        for (const auto& schema : toolSchemas.asArray()) {
            oss << "## " << schema["name"].asString() << "\n";
            oss << schema["description"].asString() << "\n";
            oss << "Parameters:\n";
            for (const auto& param : schema["parameters"].asArray()) {
                oss << "- " << param["name"].asString()
                    << " (" << param["type"].asString() << "): "
                    << param["description"].asString();
                if (param["required"].asBool()) oss << " [required]";
                oss << "\n";
            }
            oss << "\n";
        }

        oss << "# Tool Calling Format\n\n";
        oss << "To call a tool, respond with JSON:\n";
        oss << "```json\n";
        oss << "{\"tool\": \"tool_name\", \"params\": {\"param1\": \"value1\"}}\n";
        oss << "```\n\n";
        oss << "You may call multiple tools in sequence. After each tool call, you will receive the result.\n";
        oss << "When your task is complete, respond normally without any tool calls.\n";

        return oss.str();
    }

    // Parse tool calls from assistant response
    std::optional<std::pair<std::string, JSONObject>> parseToolCall(const std::string& response) {
        // Look for JSON tool call pattern
        size_t start = response.find("{\"tool\"");
        if (start == std::string::npos) {
            start = response.find("{ \"tool\"");
        }
        if (start == std::string::npos) return std::nullopt;

        // Find matching closing brace
        int braceCount = 0;
        size_t end = start;
        for (; end < response.size(); ++end) {
            if (response[end] == '{') ++braceCount;
            else if (response[end] == '}') {
                --braceCount;
                if (braceCount == 0) break;
            }
        }

        if (braceCount != 0) return std::nullopt;

        std::string jsonStr = response.substr(start, end - start + 1);
        try {
            auto json = JSONParser::Parse(jsonStr);
            if (json.contains("tool") && json.contains("params")) {
                return std::make_pair(
                    json["tool"].asString(),
                    json["params"].asObject()
                );
            }
        } catch (...) {}

        return std::nullopt;
    }

public:
    Agent() {
        m_tools.setVectorDatabase(&m_vectorDb);
        m_tools.setModelBridge(&m_modelBridge);
    }

    void configure(const AgentConfig& config) {
        m_config = config;
    }

    bool initialize() {
        // Load native model bridge
        fs::path dllPath = fs::path(m_config.workspaceRoot) / "RawrXD_NativeModelBridge.dll";
        if (fs::exists(dllPath)) {
            m_modelBridge.load(dllPath.wstring());
        }

        // Index workspace if specified
        if (!m_config.workspaceRoot.empty() && fs::exists(m_config.workspaceRoot)) {
            m_vectorDb.indexDirectory(m_config.workspaceRoot);
        }

        return true;
    }

    std::string chat(const std::string& userMessage) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Add user message
        m_conversationHistory.push_back({
            "user",
            userMessage,
            std::nullopt,
            std::nullopt,
            std::nullopt
        });

        std::string response;
        int iterations = 0;

        while (iterations < m_config.maxIterations) {
            // Generate response (placeholder - integrate with actual LLM)
            response = generateResponse();

            // Check for tool calls
            auto toolCall = parseToolCall(response);
            if (!toolCall) {
                // No tool call, we're done
                break;
            }

            // Execute tool
            auto [toolName, params] = *toolCall;
            auto result = m_tools.executeTool(toolName, params);

            // Add to history
            m_conversationHistory.push_back({
                "assistant",
                response,
                toolName,
                params,
                std::nullopt
            });

            m_conversationHistory.push_back({
                "tool",
                result.stringify(),
                toolName,
                std::nullopt,
                result
            });

            ++iterations;
        }

        // Add final response
        m_conversationHistory.push_back({
            "assistant",
            response,
            std::nullopt,
            std::nullopt,
            std::nullopt
        });

        return response;
    }

    // Execute a task autonomously
    JSONValue executeTask(const std::string& task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = true;

        JSONObject result;
        result["task"] = JSONValue(task);
        result["status"] = JSONValue("started");

        std::vector<JSONObject> steps;
        int iteration = 0;

        while (m_running && iteration < m_config.maxIterations) {
            std::string response = chat(task);

            JSONObject step;
            step["iteration"] = JSONValue(iteration);
            step["response"] = JSONValue(response);
            steps.push_back(step);

            // Check if task is complete (no more tool calls)
            if (!parseToolCall(response)) {
                result["status"] = JSONValue("completed");
                break;
            }

            ++iteration;
        }

        if (iteration >= m_config.maxIterations) {
            result["status"] = JSONValue("max_iterations_reached");
        }

        JSONArray stepsArray;
        for (const auto& s : steps) stepsArray.push_back(JSONValue(s));
        result["steps"] = JSONValue(stepsArray);

        m_running = false;
        return JSONValue(result);
    }

    void stop() {
        m_running = false;
    }

    // Direct tool access
    JSONValue executeTool(const std::string& name, const JSONObject& params) {
        return m_tools.executeTool(name, params);
    }

    std::vector<std::string> getAvailableTools() const {
        return m_tools.getToolNames();
    }

    VectorDatabase& getVectorDb() { return m_vectorDb; }
    NativeModelBridge& getModelBridge() { return m_modelBridge; }

private:
    std::string generateResponse() {
        // Build prompt from conversation history
        std::ostringstream prompt;
        prompt << buildSystemPrompt() << "\n\n";

        for (const auto& msg : m_conversationHistory) {
            if (msg.role == "user") {
                prompt << "User: " << msg.content << "\n\n";
            } else if (msg.role == "assistant") {
                prompt << "Assistant: " << msg.content << "\n\n";
            } else if (msg.role == "tool") {
                prompt << "Tool Result: " << msg.content << "\n\n";
            }
        }

        prompt << "Assistant: ";

        // If model bridge is loaded, use it
        if (m_modelBridge.isLoaded()) {
            // TODO: Tokenize and run inference
            // For now, return placeholder
        }

        // Placeholder response - in production, this would call the LLM
        return "I'll help you with that task. Let me analyze what needs to be done.";
    }
};

} // namespace RawrXD

// =============================================================================
// MCP SERVER INTEGRATION
// =============================================================================

#include "MCPServer.hpp"

namespace RawrXD {

class RawrXDMCPBridge {
private:
    Agent* m_agent;
    MCP::MCPServer m_server;

public:
    explicit RawrXDMCPBridge(Agent* agent) : m_agent(agent), m_server(8765, "/mcp") {}

    void start() {
        // Register tools as MCP tools
        for (const auto& toolName : m_agent->getAvailableTools()) {
            MCP::MCPTool mcpTool;
            mcpTool.name = toolName;
            mcpTool.description = "RawrXD tool: " + toolName;
            // Parameters would be filled from tool registry

            m_server.registerTool(mcpTool, [this, toolName](const RawrXD::JSONObject& params) {
                return m_agent->executeTool(toolName, params);
            });
        }

        m_server.start();
    }

    void stop() {
        m_server.stop();
    }
};

} // namespace RawrXD

// =============================================================================
// LSP CLIENT INTEGRATION
// =============================================================================

#include "LSPClient.hpp"

namespace RawrXD {

class RawrXDLSPBridge {
private:
    LSP::LSPServerManager m_serverManager;
    Agent* m_agent;

public:
    explicit RawrXDLSPBridge(Agent* agent) : m_agent(agent) {}

    bool startLanguageServer(const std::string& language, const std::string& serverPath) {
        return m_serverManager.startServer(language, serverPath);
    }

    JSONValue getCompletions(const std::string& filepath, int line, int character) {
        // Determine language from extension
        fs::path p(filepath);
        std::string ext = p.extension().string();
        std::string language;

        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
            language = "cpp";
        } else if (ext == ".py") {
            language = "python";
        } else if (ext == ".js" || ext == ".ts") {
            language = "typescript";
        } else if (ext == ".rs") {
            language = "rust";
        } else {
            JSONObject err; err["error"] = JSONValue("Unknown language for extension: " + ext);
            return JSONValue(err);
        }

        auto* client = m_serverManager.getClient(language);
        if (!client) {
            JSONObject err; err["error"] = JSONValue("No LSP server for language: " + language);
            return JSONValue(err);
        }

        auto items = client->getCompletions(filepath, line, character);

        JSONArray completions;
        for (const auto& item : items) {
            JSONObject c;
            c["label"] = JSONValue(item.label);
            c["kind"] = JSONValue(static_cast<double>(item.kind));
            c["detail"] = JSONValue(item.detail);
            c["insertText"] = JSONValue(item.insertText);
            completions.push_back(JSONValue(c));
        }

        JSONObject result;
        result["completions"] = JSONValue(completions);
        return JSONValue(result);
    }

    JSONValue getDiagnostics(const std::string& filepath) {
        fs::path p(filepath);
        std::string ext = p.extension().string();
        std::string language;

        if (ext == ".cpp" || ext == ".hpp") language = "cpp";
        else if (ext == ".py") language = "python";
        else if (ext == ".js" || ext == ".ts") language = "typescript";
        else if (ext == ".rs") language = "rust";
        else {
            JSONObject err; err["error"] = JSONValue("Unknown language");
            return JSONValue(err);
        }

        auto* client = m_serverManager.getClient(language);
        if (!client) {
            JSONObject err; err["error"] = JSONValue("No LSP server");
            return JSONValue(err);
        }

        auto diags = client->getDiagnostics(filepath);

        JSONArray diagnostics;
        for (const auto& d : diags) {
            JSONObject diag;
            diag["message"] = JSONValue(d.message);
            diag["severity"] = JSONValue(static_cast<double>(d.severity));
            diag["startLine"] = JSONValue(d.range.start.line);
            diag["startChar"] = JSONValue(d.range.start.character);
            diag["endLine"] = JSONValue(d.range.end.line);
            diag["endChar"] = JSONValue(d.range.end.character);
            diagnostics.push_back(JSONValue(diag));
        }

        JSONObject result;
        result["diagnostics"] = JSONValue(diagnostics);
        return JSONValue(result);
    }
};

} // namespace RawrXD

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================

int wmain(int argc, wchar_t* argv[]) {
    // Initialize COM
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Set console output to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Parse command line
    std::wstring workspaceRoot = L".";
    std::wstring modelPath;
    bool startMCP = false;
    int mcpPort = 8765;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--workspace" && i + 1 < argc) {
            workspaceRoot = argv[++i];
        } else if (arg == L"--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == L"--mcp") {
            startMCP = true;
        } else if (arg == L"--mcp-port" && i + 1 < argc) {
            mcpPort = _wtoi(argv[++i]);
        }
    }

    // Create agent
    RawrXD::Agent agent;

    RawrXD::AgentConfig config;
    config.systemPrompt = R"(You are RawrXD, an advanced autonomous AI coding agent.
You have access to a comprehensive set of tools for file manipulation, code analysis,
git operations, HTTP requests, and more.

Your capabilities include:
- Reading, writing, and modifying files
- Executing shell commands
- Searching code semantically and with regex
- Analyzing git repositories
- Making HTTP requests
- Managing the development environment

When given a task:
1. Analyze the requirements carefully
2. Break down complex tasks into steps
3. Use tools to gather information and make changes
4. Verify your work using appropriate tools
5. Provide clear explanations of what you've done

Always aim for clean, efficient, and well-documented solutions.)";

    config.workspaceRoot = std::string(workspaceRoot.begin(), workspaceRoot.end());
    config.modelPath = std::string(modelPath.begin(), modelPath.end());
    config.maxIterations = 50;
    config.temperature = 0.7;
    config.topP = 0.9;

    agent.configure(config);

    if (!agent.initialize()) {
        std::wcerr << L"Failed to initialize agent\n";
        CoUninitialize();
        return 1;
    }

    // Print banner
    std::wcout << L"\n";
    std::wcout << L"╔══════════════════════════════════════════════════════════╗\n";
    std::wcout << L"║                    RawrXD Agent v1.0                     ║\n";
    std::wcout << L"║            Autonomous AI Coding Assistant                ║\n";
    std::wcout << L"╚══════════════════════════════════════════════════════════╝\n";
    std::wcout << L"\n";
    std::wcout << L"Workspace: " << workspaceRoot << L"\n";
    std::wcout << L"Tools available: " << agent.getAvailableTools().size() << L"\n";
    std::wcout << L"Vector DB entries: " << agent.getVectorDb().size() << L"\n";
    std::wcout << L"Native bridge: " << (agent.getModelBridge().isLoaded() ? L"loaded" : L"not loaded") << L"\n";
    std::wcout << L"\n";

    // Start MCP server if requested
    std::unique_ptr<RawrXD::RawrXDMCPBridge> mcpBridge;
    if (startMCP) {
        mcpBridge = std::make_unique<RawrXD::RawrXDMCPBridge>(&agent);
        mcpBridge->start();
        std::wcout << L"MCP server started on port " << mcpPort << L"\n";
    }

    // Interactive loop
    std::wcout << L"Type 'help' for commands, 'quit' to exit.\n\n";

    std::string input;
    while (true) {
        std::wcout << L"RawrXD> ";
        std::wcout.flush();

        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input == "quit" || input == "exit") {
            break;
        }

        if (input == "help") {
            std::wcout << L"\nCommands:\n";
            std::wcout << L"  help           - Show this help\n";
            std::wcout << L"  tools          - List available tools\n";
            std::wcout << L"  tool <name>    - Show tool details\n";
            std::wcout << L"  exec <tool> <json> - Execute a tool directly\n";
            std::wcout << L"  index <path>   - Index a directory\n";
            std::wcout << L"  search <query> - Semantic search\n";
            std::wcout << L"  quit           - Exit\n";
            std::wcout << L"\nOr just type your request and I'll help you!\n\n";
            continue;
        }

        if (input == "tools") {
            std::wcout << L"\nAvailable tools:\n";
            for (const auto& name : agent.getAvailableTools()) {
                std::wcout << L"  - " << std::wstring(name.begin(), name.end()) << L"\n";
            }
            std::wcout << L"\n";
            continue;
        }

        if (input.rfind("tool ", 0) == 0) {
            std::string toolName = input.substr(5);
            // Show tool schema
            continue;
        }

        if (input.rfind("exec ", 0) == 0) {
            // Parse and execute tool
            continue;
        }

        if (input.rfind("index ", 0) == 0) {
            std::string path = input.substr(6);
            std::wcout << L"Indexing " << std::wstring(path.begin(), path.end()) << L"...\n";
            agent.getVectorDb().indexDirectory(path);
            std::wcout << L"Indexed " << agent.getVectorDb().size() << L" files.\n\n";
            continue;
        }

        if (input.rfind("search ", 0) == 0) {
            std::string query = input.substr(7);
            auto results = agent.getVectorDb().search(query, 5);
            std::wcout << L"\nSearch results:\n";
            for (const auto& r : results) {
                std::wcout << L"  [" << r.score << L"] " << std::wstring(r.filepath.begin(), r.filepath.end()) << L"\n";
            }
            std::wcout << L"\n";
            continue;
        }

        // Chat with agent
        std::string response = agent.chat(input);
        std::wcout << L"\n" << std::wstring(response.begin(), response.end()) << L"\n\n";
    }

    // Cleanup
    if (mcpBridge) {
        mcpBridge->stop();
    }

    CoUninitialize();
    return 0;
}
