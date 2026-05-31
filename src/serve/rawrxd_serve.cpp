// ============================================================================
// rawrxd_serve.cpp — Ollama-compatible HTTP server (Win32 httpapi)
// ============================================================================
// Production implementation.  Uses http.sys via the HTTP Server API v2 for
// kernel-mode HTTP processing.  Streaming responses use chunked transfer
// encoding to emit NDJSON lines identical to Ollama's wire format.
// ============================================================================
#include "rawrxd_serve.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>

#pragma comment(lib, "httpapi.lib")

namespace RawrXD {
namespace Serve {

// ============================================================================
// Minimal JSON writer — avoids nlohmann dependency for the serve binary
// ============================================================================
namespace json {

static std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Tiny JSON object builder
class Obj {
    std::ostringstream m_ss;
    bool m_first = true;
public:
    Obj() { m_ss << '{'; }
    Obj& kv(const char* k, const std::string& v) {
        sep(); m_ss << '"' << k << "\":\"" << escape(v) << '"';
        return *this;
    }
    Obj& kv(const char* k, int64_t v) {
        sep(); m_ss << '"' << k << "\":" << v;
        return *this;
    }
    Obj& kv(const char* k, uint64_t v) {
        sep(); m_ss << '"' << k << "\":" << v;
        return *this;
    }
    Obj& kv(const char* k, double v) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6g", v);
        sep(); m_ss << '"' << k << "\":" << buf;
        return *this;
    }
    Obj& kv(const char* k, bool v) {
        sep(); m_ss << '"' << k << "\":" << (v ? "true" : "false");
        return *this;
    }
    Obj& kvRaw(const char* k, const std::string& rawJson) {
        sep(); m_ss << '"' << k << "\":" << rawJson;
        return *this;
    }
    std::string build() { m_ss << '}'; return m_ss.str(); }
private:
    void sep() { if (!m_first) m_ss << ','; m_first = false; }
};

} // namespace json

// ============================================================================
// Minimal JSON reader — parse body for generate/chat requests
// ============================================================================
namespace jsonread {

// Find value for a given key in a flat JSON object (no nesting in arrays)
static std::string findString(const std::string& body, const char* key) {
    // Pattern: "key":"value" or "key": "value"
    std::string needle = std::string("\"") + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return {};

    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};

    // Skip whitespace
    pos++;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t'))
        pos++;

    if (pos >= body.size() || body[pos] != '"') return {};
    pos++; // skip opening quote

    std::string result;
    while (pos < body.size() && body[pos] != '"') {
        if (body[pos] == '\\' && pos + 1 < body.size()) {
            pos++;
            switch (body[pos]) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"':  result += '"';  break;
                default:   result += body[pos]; break;
            }
        } else {
            result += body[pos];
        }
        pos++;
    }
    return result;
}

static bool findBool(const std::string& body, const char* key, bool def) {
    std::string needle = std::string("\"") + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return def;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < body.size() && body[pos] == ' ') pos++;
    if (pos < body.size() && body[pos] == 't') return true;
    if (pos < body.size() && body[pos] == 'f') return false;
    return def;
}

static int findInt(const std::string& body, const char* key, int def) {
    std::string needle = std::string("\"") + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return def;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < body.size() && body[pos] == ' ') pos++;
    if (pos >= body.size()) return def;
    bool neg = false;
    if (body[pos] == '-') { neg = true; pos++; }
    int val = 0;
    while (pos < body.size() && body[pos] >= '0' && body[pos] <= '9') {
        val = val * 10 + (body[pos] - '0');
        pos++;
    }
    return neg ? -val : val;
}

static float findFloat(const std::string& body, const char* key, float def) {
    std::string needle = std::string("\"") + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return def;
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < body.size() && body[pos] == ' ') pos++;
    // Extract numeric substring
    size_t start = pos;
    while (pos < body.size() && (body[pos] == '.' || body[pos] == '-' ||
           (body[pos] >= '0' && body[pos] <= '9') || body[pos] == 'e' || body[pos] == 'E'))
        pos++;
    if (pos == start) return def;
    return std::strtof(body.c_str() + start, nullptr);
}

// Extract messages array from chat body — simplified parser
static std::vector<ChatMessage> findMessages(const std::string& body) {
    std::vector<ChatMessage> msgs;

    auto arrStart = body.find("\"messages\"");
    if (arrStart == std::string::npos) return msgs;

    arrStart = body.find('[', arrStart);
    if (arrStart == std::string::npos) return msgs;

    // Walk through objects in the array
    size_t pos = arrStart + 1;
    while (pos < body.size()) {
        auto objStart = body.find('{', pos);
        if (objStart == std::string::npos) break;

        auto objEnd = body.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = body.substr(objStart, objEnd - objStart + 1);
        ChatMessage m;
        m.role    = findString(obj, "role");
        m.content = findString(obj, "content");
        if (!m.role.empty())
            msgs.push_back(std::move(m));

        pos = objEnd + 1;
        if (body.find(']', pos) < body.find('{', pos))
            break;
    }
    return msgs;
}

} // namespace jsonread

// ============================================================================
// ISO 8601 timestamp
// ============================================================================
static std::string isoTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    time_t tt = static_cast<time_t>(epoch);
    struct tm utc;
#ifdef _MSC_VER
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

// ============================================================================
// RawrXDServer
// ============================================================================

RawrXDServer::RawrXDServer() = default;

RawrXDServer::~RawrXDServer() {
    stop();
}

bool RawrXDServer::start(const ServeConfig& cfg, InferenceBackend backend) {
    if (m_running.load()) return false;

    m_config  = cfg;
    m_backend = std::move(backend);

    // Setup model registry
    for (auto& d : cfg.modelDirs)
        m_registry.addSearchPath(d);
    m_registry.addSearchPath(ModelRegistry::defaultModelDir());
    m_registry.scan();

    // Initialize HTTP Server API
    HTTPAPI_VERSION ver = HTTPAPI_VERSION_2;
    ULONG ret = HttpInitialize(ver, HTTP_INITIALIZE_SERVER, nullptr);
    if (ret != NO_ERROR) {
        fprintf(stderr, "HttpInitialize failed: %lu\n", ret);
        return false;
    }

    // Create server session
    ret = HttpCreateServerSession(ver, &m_sessionId, 0);
    if (ret != NO_ERROR) {
        fprintf(stderr, "HttpCreateServerSession failed: %lu\n", ret);
        HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        return false;
    }

    // Create URL group
    ret = HttpCreateUrlGroup(m_sessionId, &m_urlGroupId, 0);
    if (ret != NO_ERROR) {
        fprintf(stderr, "HttpCreateUrlGroup failed: %lu\n", ret);
        HttpCloseServerSession(m_sessionId);
        HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        return false;
    }

    // Create request queue
    ret = HttpCreateRequestQueue(ver, L"RawrXDServe", nullptr, 0,
                                  &m_reqQueue);
    if (ret != NO_ERROR) {
        fprintf(stderr, "HttpCreateRequestQueue failed: %lu\n", ret);
        HttpCloseUrlGroup(m_urlGroupId);
        HttpCloseServerSession(m_sessionId);
        HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        return false;
    }

    // Bind URL group to request queue
    HTTP_BINDING_INFO binding = {};
    binding.Flags.Present = 1;
    binding.RequestQueueHandle = m_reqQueue;
    ret = HttpSetUrlGroupProperty(m_urlGroupId,
                                   HttpServerBindingProperty,
                                   &binding, sizeof(binding));
    if (ret != NO_ERROR) {
        fprintf(stderr, "HttpSetUrlGroupProperty (binding) failed: %lu\n", ret);
        HttpCloseRequestQueue(m_reqQueue);
        HttpCloseUrlGroup(m_urlGroupId);
        HttpCloseServerSession(m_sessionId);
        HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        return false;
    }

    // Add URL prefix
    std::wstring prefix = L"http://" +
        std::wstring(cfg.host.begin(), cfg.host.end()) +
        L":" + std::to_wstring(cfg.port) + L"/";

    ret = HttpAddUrlToUrlGroup(m_urlGroupId, prefix.c_str(), 0, 0);
    if (ret != NO_ERROR) {
        fprintf(stderr, "HttpAddUrlToUrlGroup failed: %lu (prefix: http://%s:%u/)\n",
                ret, cfg.host.c_str(), cfg.port);
        HttpCloseRequestQueue(m_reqQueue);
        HttpCloseUrlGroup(m_urlGroupId);
        HttpCloseServerSession(m_sessionId);
        HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        return false;
    }

    m_running = true;
    m_listenerThread = std::thread(&RawrXDServer::requestLoop, this);

    printf("RawrXD serve listening on http://%s:%u\n", cfg.host.c_str(), cfg.port);
    printf("Models discovered: %zu\n", m_registry.models().size());

    return true;
}

void RawrXDServer::stop() {
    if (!m_running.exchange(false)) return;

    if (m_reqQueue) {
        HttpShutdownRequestQueue(m_reqQueue);
    }
    if (m_listenerThread.joinable())
        m_listenerThread.join();

    if (m_urlGroupId)  HttpCloseUrlGroup(m_urlGroupId);
    if (m_sessionId)   HttpCloseServerSession(m_sessionId);
    if (m_reqQueue)    HttpCloseRequestQueue(m_reqQueue);

    m_urlGroupId = 0;
    m_sessionId  = 0;
    m_reqQueue   = nullptr;

    HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
}

// ============================================================================
// Request loop — runs on listener thread
// ============================================================================

void RawrXDServer::requestLoop() {
    std::vector<BYTE> buffer(REQ_BUF_SIZE + sizeof(HTTP_REQUEST));
    auto* req = reinterpret_cast<HTTP_REQUEST*>(buffer.data());

    while (m_running.load()) {
        memset(buffer.data(), 0, buffer.size());

        ULONG bytesReturned = 0;
        ULONG ret = HttpReceiveHttpRequest(
            m_reqQueue, HTTP_NULL_ID, 0,
            req, static_cast<ULONG>(buffer.size()),
            &bytesReturned, nullptr);

        if (ret == NO_ERROR) {
            handleRequest(req);
        } else if (ret == ERROR_MORE_DATA) {
            // Request larger than buffer — reallocate
            buffer.resize(bytesReturned + 1024);
            req = reinterpret_cast<HTTP_REQUEST*>(buffer.data());
            // Re-receive with same request ID
            ret = HttpReceiveHttpRequest(
                m_reqQueue, req->RequestId, 0,
                req, static_cast<ULONG>(buffer.size()),
                &bytesReturned, nullptr);
            if (ret == NO_ERROR)
                handleRequest(req);
        } else if (ret == ERROR_CONNECTION_INVALID || ret == ERROR_OPERATION_ABORTED) {
            // Server shutting down
            break;
        }
    }
}

// ============================================================================
// Route dispatch
// ============================================================================

void RawrXDServer::handleRequest(HTTP_REQUEST* req) {
    // Extract URL path
    std::wstring wpath(req->CookedUrl.pAbsPath, req->CookedUrl.AbsPathLength / sizeof(WCHAR));
    std::string path(wpath.begin(), wpath.end());

    // Strip query string
    auto qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);

    // Route
    if (path == "/api/tags" || path == "/api/tags/") {
        handleApiTags(req);
    } else if (path == "/api/generate" || path == "/api/generate/") {
        handleApiGenerate(req);
    } else if (path == "/api/chat" || path == "/api/chat/") {
        handleApiChat(req);
    } else if (path == "/api/show" || path == "/api/show/") {
        handleApiShow(req);
    } else if (path == "/api/delete" || path == "/api/delete/") {
        handleApiDelete(req);
    } else if (path == "/api/ps" || path == "/api/ps/") {
        handleApiPs(req);
    } else if (path == "/" || path == "/api" || path == "/api/") {
        // Health / root
        sendResponse(req, 200, "application/json",
                     "{\"status\":\"ok\",\"engine\":\"rawrxd\"}");
    } else {
        handleNotFound(req);
    }
}

// ============================================================================
// GET /api/tags — list models
// ============================================================================

void RawrXDServer::handleApiTags(HTTP_REQUEST* req) {
    sendResponse(req, 200, "application/json", buildTagsJson());
}

std::string RawrXDServer::buildTagsJson() const {
    auto& models = m_registry.models();

    std::ostringstream ss;
    ss << "{\"models\":[";
    for (size_t i = 0; i < models.size(); i++) {
        auto& m = models[i];
        if (i > 0) ss << ',';
        ss << '{'
           << "\"name\":\"" << json::escape(m.name) << "\""
           << ",\"model\":\"" << json::escape(m.name) << "\""
           << ",\"size\":" << m.fileSizeBytes
           << ",\"digest\":\"" << json::escape(m.path) << "\""
           << ",\"details\":{"
           << "\"format\":\"gguf\""
           << ",\"family\":\"" << json::escape(m.architecture) << "\""
           << ",\"parameter_size\":\"" << m.paramCount << "\""
           << ",\"quantization_level\":\"" << json::escape(m.quantization) << "\""
           << "}"
           << "}";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// POST /api/generate — generate completion
// ============================================================================

void RawrXDServer::handleApiGenerate(HTTP_REQUEST* req) {
    std::string body = readRequestBody(req);
    if (body.empty()) {
        sendResponse(req, 400, "application/json",
                     "{\"error\":\"empty request body\"}");
        return;
    }

    auto genReq = parseGenerateRequest(body);

    // Ensure model is loaded
    {
        std::lock_guard<std::mutex> lk(m_inferenceMu);
        if (!m_backend.isLoaded || !m_backend.isLoaded() ||
            m_currentModel != genReq.model)
        {
            auto* entry = m_registry.find(genReq.model);
            if (!entry) {
                sendResponse(req, 404, "application/json",
                             "{\"error\":\"model not found\"}");
                return;
            }
            if (m_backend.loadModel && !m_backend.loadModel(entry->path)) {
                sendResponse(req, 500, "application/json",
                             "{\"error\":\"failed to load model\"}");
                return;
            }
            m_currentModel = genReq.model;
        }
    }

    if (!genReq.stream) {
        // Non-streaming: accumulate full response
        std::string fullResp;
        if (m_backend.generate) {
            std::lock_guard<std::mutex> lk(m_inferenceMu);
            fullResp = m_backend.generate(genReq, [](const std::string&, bool){});
        }
        std::string j = json::Obj()
            .kv("model", genReq.model)
            .kv("response", fullResp)
            .kv("done", true)
            .kv("created_at", isoTimestamp())
            .build();
        sendResponse(req, 200, "application/x-ndjson", j);
        return;
    }

    // Streaming response — send initial headers then stream chunks
    HTTP_RESPONSE response = {};
    response.StatusCode = 200;
    response.pReason = "OK";
    response.ReasonLength = 2;

    // Content-Type header
    HTTP_KNOWN_HEADER& ctHeader = response.Headers.KnownHeaders[HttpHeaderContentType];
    ctHeader.pRawValue = "application/x-ndjson";
    ctHeader.RawValueLength = 20;

    // Transfer-Encoding: chunked
    HTTP_KNOWN_HEADER& teHeader = response.Headers.KnownHeaders[HttpHeaderTransferEncoding];
    teHeader.pRawValue = "chunked";
    teHeader.RawValueLength = 7;

    ULONG ret = HttpSendHttpResponse(m_reqQueue, req->RequestId,
                                      HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
                                      &response, nullptr, nullptr,
                                      nullptr, 0, nullptr, nullptr);
    if (ret != NO_ERROR) return;

    HTTP_REQUEST_ID requestId = req->RequestId;
    if (m_backend.generate) {
        std::lock_guard<std::mutex> lk(m_inferenceMu);
        m_backend.generate(genReq,
            [this, requestId, &genReq](const std::string& token, bool done) {
                std::string chunk = buildGenerateChunk(genReq.model, token, done) + "\n";
                // Send as entity body chunk
                HTTP_DATA_CHUNK dataChunk = {};
                dataChunk.DataChunkType = HttpDataChunkFromMemory;
                dataChunk.FromMemory.pBuffer = (PVOID)chunk.data();
                dataChunk.FromMemory.BufferLength = static_cast<ULONG>(chunk.size());

                ULONG flags = done ? 0 : HTTP_SEND_RESPONSE_FLAG_MORE_DATA;
                HttpSendResponseEntityBody(m_reqQueue, requestId, flags,
                                           1, &dataChunk, nullptr,
                                           nullptr, 0, nullptr, nullptr);
            });
    }
}

// ============================================================================
// POST /api/chat — chat completion
// ============================================================================

void RawrXDServer::handleApiChat(HTTP_REQUEST* req) {
    std::string body = readRequestBody(req);
    if (body.empty()) {
        sendResponse(req, 400, "application/json",
                     "{\"error\":\"empty request body\"}");
        return;
    }

    auto chatReq = parseChatRequest(body);

    // Build prompt from messages
    std::string combinedPrompt;
    for (auto& m : chatReq.messages) {
        if (!combinedPrompt.empty()) combinedPrompt += "\n";
        if (m.role == "system") {
            combinedPrompt += m.content;
        } else if (m.role == "user") {
            combinedPrompt += "User: " + m.content;
        } else if (m.role == "assistant") {
            combinedPrompt += "Assistant: " + m.content;
        }
    }
    combinedPrompt += "\nAssistant: ";
    chatReq.prompt = combinedPrompt;

    // Ensure model
    {
        std::lock_guard<std::mutex> lk(m_inferenceMu);
        if (!m_backend.isLoaded || !m_backend.isLoaded() ||
            m_currentModel != chatReq.model)
        {
            auto* entry = m_registry.find(chatReq.model);
            if (!entry) {
                sendResponse(req, 404, "application/json",
                             "{\"error\":\"model not found\"}");
                return;
            }
            if (m_backend.loadModel && !m_backend.loadModel(entry->path)) {
                sendResponse(req, 500, "application/json",
                             "{\"error\":\"failed to load model\"}");
                return;
            }
            m_currentModel = chatReq.model;
        }
    }

    if (!chatReq.stream) {
        std::string fullResp;
        if (m_backend.generate) {
            std::lock_guard<std::mutex> lk(m_inferenceMu);
            fullResp = m_backend.generate(chatReq, [](const std::string&, bool){});
        }
        std::string j = json::Obj()
            .kv("model", chatReq.model)
            .kvRaw("message", json::Obj()
                .kv("role", "assistant")
                .kv("content", fullResp).build())
            .kv("done", true)
            .kv("created_at", isoTimestamp())
            .build();
        sendResponse(req, 200, "application/x-ndjson", j);
        return;
    }

    // Streaming
    HTTP_RESPONSE response = {};
    response.StatusCode = 200;
    response.pReason = "OK";
    response.ReasonLength = 2;

    HTTP_KNOWN_HEADER& ctHeader = response.Headers.KnownHeaders[HttpHeaderContentType];
    ctHeader.pRawValue = "application/x-ndjson";
    ctHeader.RawValueLength = 20;

    HTTP_KNOWN_HEADER& teHeader = response.Headers.KnownHeaders[HttpHeaderTransferEncoding];
    teHeader.pRawValue = "chunked";
    teHeader.RawValueLength = 7;

    ULONG ret = HttpSendHttpResponse(m_reqQueue, req->RequestId,
                                      HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
                                      &response, nullptr, nullptr,
                                      nullptr, 0, nullptr, nullptr);
    if (ret != NO_ERROR) return;

    HTTP_REQUEST_ID requestId = req->RequestId;
    if (m_backend.generate) {
        std::lock_guard<std::mutex> lk(m_inferenceMu);
        m_backend.generate(chatReq,
            [this, requestId, &chatReq](const std::string& token, bool done) {
                std::string chunk = buildChatChunk(chatReq.model, token, done) + "\n";
                HTTP_DATA_CHUNK dataChunk = {};
                dataChunk.DataChunkType = HttpDataChunkFromMemory;
                dataChunk.FromMemory.pBuffer = (PVOID)chunk.data();
                dataChunk.FromMemory.BufferLength = static_cast<ULONG>(chunk.size());

                ULONG flags = done ? 0 : HTTP_SEND_RESPONSE_FLAG_MORE_DATA;
                HttpSendResponseEntityBody(m_reqQueue, requestId, flags,
                                           1, &dataChunk, nullptr,
                                           nullptr, 0, nullptr, nullptr);
            });
    }
}

// ============================================================================
// POST /api/show — model info
// ============================================================================

void RawrXDServer::handleApiShow(HTTP_REQUEST* req) {
    std::string body = readRequestBody(req);
    std::string name = jsonread::findString(body, "name");
    if (name.empty()) name = jsonread::findString(body, "model");

    auto* entry = m_registry.find(name);
    if (!entry) {
        sendResponse(req, 404, "application/json",
                     "{\"error\":\"model not found\"}");
        return;
    }
    sendResponse(req, 200, "application/json", buildShowJson(*entry));
}

std::string RawrXDServer::buildShowJson(const ModelEntry& e) const {
    return json::Obj()
        .kv("modelfile", std::string("FROM ") + e.path)
        .kv("parameters", std::string("arch ") + e.architecture)
        .kvRaw("details", json::Obj()
            .kv("format", "gguf")
            .kv("family", e.architecture)
            .kv("parameter_size", std::to_string(e.paramCount))
            .kv("quantization_level", e.quantization)
            .build())
        .kv("model_info", e.name)
        .build();
}

// ============================================================================
// DELETE /api/delete — remove model
// ============================================================================

void RawrXDServer::handleApiDelete(HTTP_REQUEST* req) {
    std::string body = readRequestBody(req);
    std::string name = jsonread::findString(body, "name");
    if (name.empty()) name = jsonread::findString(body, "model");

    if (name.empty()) {
        sendResponse(req, 400, "application/json",
                     "{\"error\":\"name required\"}");
        return;
    }

    if (m_registry.remove(name)) {
        sendResponse(req, 200, "application/json", "{\"status\":\"success\"}");
    } else {
        sendResponse(req, 404, "application/json",
                     "{\"error\":\"model not found or delete failed\"}");
    }
}

// ============================================================================
// GET /api/ps — running models
// ============================================================================

void RawrXDServer::handleApiPs(HTTP_REQUEST* req) {
    sendResponse(req, 200, "application/json", buildPsJson());
}

std::string RawrXDServer::buildPsJson() const {
    std::ostringstream ss;
    ss << "{\"models\":[";
    if (!m_currentModel.empty() && m_backend.isLoaded && m_backend.isLoaded()) {
        auto* entry = m_registry.find(m_currentModel);
        ss << '{'
           << "\"name\":\"" << json::escape(m_currentModel) << "\""
           << ",\"model\":\"" << json::escape(m_currentModel) << "\""
           << ",\"size\":" << (entry ? entry->fileSizeBytes : 0)
           << ",\"digest\":\"" << (entry ? json::escape(entry->path) : "") << "\""
           << "}";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// 404
// ============================================================================

void RawrXDServer::handleNotFound(HTTP_REQUEST* req) {
    sendResponse(req, 404, "application/json", "{\"error\":\"not found\"}");
}

// ============================================================================
// HTTP helpers
// ============================================================================

std::string RawrXDServer::readRequestBody(HTTP_REQUEST* req) {
    std::string body;

    // First, check for entity body in the initial receive
    if (req->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS) {
        // Need to read entity body chunks
        char buf[4096];
        ULONG bytesRead = 0;
        while (true) {
            ULONG ret = HttpReceiveRequestEntityBody(
                m_reqQueue, req->RequestId, 0,
                buf, sizeof(buf), &bytesRead, nullptr);
            if (ret == NO_ERROR && bytesRead > 0) {
                body.append(buf, bytesRead);
            } else if (ret == ERROR_HANDLE_EOF) {
                break;
            } else {
                break;
            }
        }
    }

    // Also check for entity chunks already in buffer
    for (USHORT i = 0; i < req->EntityChunkCount; i++) {
        auto& chunk = req->pEntityChunks[i];
        if (chunk.DataChunkType == HttpDataChunkFromMemory) {
            body.append(static_cast<char*>(chunk.FromMemory.pBuffer),
                       chunk.FromMemory.BufferLength);
        }
    }

    return body;
}

void RawrXDServer::sendResponse(HTTP_REQUEST* req, int statusCode,
                                 const std::string& contentType,
                                 const std::string& body) {
    HTTP_RESPONSE response = {};
    response.StatusCode = static_cast<USHORT>(statusCode);
    response.pReason = "OK";
    response.ReasonLength = 2;

    if (statusCode == 404) { response.pReason = "Not Found"; response.ReasonLength = 9; }
    if (statusCode == 400) { response.pReason = "Bad Request"; response.ReasonLength = 11; }
    if (statusCode == 500) { response.pReason = "Error"; response.ReasonLength = 5; }

    HTTP_KNOWN_HEADER& ctHeader = response.Headers.KnownHeaders[HttpHeaderContentType];
    ctHeader.pRawValue = contentType.c_str();
    ctHeader.RawValueLength = static_cast<USHORT>(contentType.size());

    HTTP_DATA_CHUNK dataChunk = {};
    dataChunk.DataChunkType = HttpDataChunkFromMemory;
    dataChunk.FromMemory.pBuffer = (PVOID)body.data();
    dataChunk.FromMemory.BufferLength = static_cast<ULONG>(body.size());

    response.EntityChunkCount = 1;
    response.pEntityChunks = &dataChunk;

    HttpSendHttpResponse(m_reqQueue, req->RequestId, 0,
                          &response, nullptr, nullptr,
                          nullptr, 0, nullptr, nullptr);
}

// ============================================================================
// JSON chunk builders (Ollama wire format)
// ============================================================================

std::string RawrXDServer::buildGenerateChunk(const std::string& model,
                                              const std::string& token,
                                              bool done) const {
    return json::Obj()
        .kv("model", model)
        .kv("created_at", isoTimestamp())
        .kv("response", token)
        .kv("done", done)
        .build();
}

std::string RawrXDServer::buildChatChunk(const std::string& model,
                                          const std::string& token,
                                          bool done) const {
    auto msgObj = json::Obj()
        .kv("role", "assistant")
        .kv("content", token)
        .build();

    return json::Obj()
        .kv("model", model)
        .kv("created_at", isoTimestamp())
        .kvRaw("message", msgObj)
        .kv("done", done)
        .build();
}

// ============================================================================
// Request parsers
// ============================================================================

GenerateRequest RawrXDServer::parseGenerateRequest(const std::string& body) const {
    GenerateRequest r;
    r.model        = jsonread::findString(body, "model");
    r.prompt       = jsonread::findString(body, "prompt");
    r.system       = jsonread::findString(body, "system");
    r.stream       = jsonread::findBool(body, "stream", true);
    r.temperature  = jsonread::findFloat(body, "temperature", 0.7f);
    r.num_predict  = jsonread::findInt(body, "num_predict", 512);
    r.top_p        = jsonread::findFloat(body, "top_p", 0.9f);
    r.top_k        = jsonread::findInt(body, "top_k", 40);
    r.repeat_penalty = jsonread::findFloat(body, "repeat_penalty", 1.1f);
    r.seed         = jsonread::findInt(body, "seed", -1);
    return r;
}

GenerateRequest RawrXDServer::parseChatRequest(const std::string& body) const {
    GenerateRequest r;
    r.model    = jsonread::findString(body, "model");
    r.messages = jsonread::findMessages(body);
    r.stream   = jsonread::findBool(body, "stream", true);
    r.temperature  = jsonread::findFloat(body, "temperature", 0.7f);
    r.num_predict  = jsonread::findInt(body, "num_predict", 512);
    r.top_p        = jsonread::findFloat(body, "top_p", 0.9f);
    r.top_k        = jsonread::findInt(body, "top_k", 40);
    r.repeat_penalty = jsonread::findFloat(body, "repeat_penalty", 1.1f);
    r.seed         = jsonread::findInt(body, "seed", -1);
    return r;
}

} // namespace Serve
} // namespace RawrXD
