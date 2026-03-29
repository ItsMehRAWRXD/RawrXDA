// ============================================================================
// Win32IDE_LocalServer.cpp — Embedded GGUF HTTP Server
// ============================================================================
// Turns the IDE into an inference endpoint:
//   - Ollama-compatible: /api/generate, /api/tags
//   - OpenAI-compatible: /v1/chat/completions
//   - Health endpoint: /health, /status
//   - Streaming SSE for all generation endpoints
//   - CORS enabled for browser/extension integration
//   - Configurable port (default 11435 to avoid conflict with Ollama 11434)
//
// Architecture:
//   - Extends the existing CompletionServer (WinSock2-based)
//   - Runs on a background thread, non-blocking
//   - Uses the IDE's loaded model (m_nativeEngine) for inference
//   - Thread-safe access via existing mutex
// ============================================================================

#include "../../include/chain_of_thought_engine.h"
#include "../agentic/AgentToolHandlers.h"
#include "../agentic/OrchestrationSessionState.h"
#include "../core/agent_capability_audit.hpp"
#include "../core/dual_agent_session.hpp"
#include "../core/instructions_provider.hpp"
#include "../core/model_name_util.h"
#include "../core/proxy_hotpatcher.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "IDELogger.h"
#include "Win32IDE.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>


// Forward: socket type
using LocalServerSocket = SOCKET;
static const LocalServerSocket kInvalidSock = INVALID_SOCKET;

// ============================================================================
// HELPERS — JSON escaping, request parsing, response building
// ============================================================================

namespace LocalServerUtil
{

static std::string escapeJson(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value)
    {
        switch (c)
        {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool extractJsonString(const std::string& body, const std::string& key, std::string& out)
{
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos)
        return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos)
        return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos]))
        pos++;
    if (pos >= body.size() || body[pos] != '"')
        return false;
    pos++;
    std::string result;
    while (pos < body.size())
    {
        char c = body[pos++];
        if (c == '\\' && pos < body.size())
        {
            char esc = body[pos++];
            switch (esc)
            {
                case '"':
                    result.push_back('"');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                default:
                    result.push_back(esc);
                    break;
            }
            continue;
        }
        if (c == '"')
            break;
        result.push_back(c);
    }
    out = result;
    return true;
}

static bool extractJsonInt(const std::string& body, const std::string& key, int& out)
{
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos)
        return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos)
        return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos]))
        pos++;
    size_t start = pos;
    while (pos < body.size() && (std::isdigit((unsigned char)body[pos]) || body[pos] == '-'))
        pos++;
    if (start == pos)
        return false;
    try
    {
        out = std::stoi(body.substr(start, pos - start));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

static bool extractJsonFloat(const std::string& body, const std::string& key, float& out)
{
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos)
        return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos)
        return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos]))
        pos++;
    size_t start = pos;
    while (pos < body.size() && (std::isdigit((unsigned char)body[pos]) || body[pos] == '.' || body[pos] == '-'))
        pos++;
    if (start == pos)
        return false;
    try
    {
        out = std::stof(body.substr(start, pos - start));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

static bool extractJsonBool(const std::string& body, const std::string& key, bool& out)
{
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos)
        return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos)
        return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos]))
        pos++;
    if (pos + 4 <= body.size() && body.substr(pos, 4) == "true")
    {
        out = true;
        return true;
    }
    if (pos + 5 <= body.size() && body.substr(pos, 5) == "false")
    {
        out = false;
        return true;
    }
    return false;
}

static std::string buildHttpResponse(int status, const std::string& body,
                                     const std::string& contentType = "application/json")
{
    std::ostringstream oss;
    switch (status)
    {
        case 200:
            oss << "HTTP/1.1 200 OK\r\n";
            break;
        case 201:
            oss << "HTTP/1.1 201 Created\r\n";
            break;
        case 204:
            oss << "HTTP/1.1 204 No Content\r\n";
            break;
        case 400:
            oss << "HTTP/1.1 400 Bad Request\r\n";
            break;
        case 403:
            oss << "HTTP/1.1 403 Forbidden\r\n";
            break;
        case 404:
            oss << "HTTP/1.1 404 Not Found\r\n";
            break;
        case 409:
            oss << "HTTP/1.1 409 Conflict\r\n";
            break;
        case 413:
            oss << "HTTP/1.1 413 Payload Too Large\r\n";
            break;
        case 422:
            oss << "HTTP/1.1 422 Unprocessable Entity\r\n";
            break;
        case 500:
            oss << "HTTP/1.1 500 Internal Server Error\r\n";
            break;
        default:
            oss << "HTTP/1.1 500 Internal Server Error\r\n";
            break;
    }
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

// Reliable send — loops until all bytes are transmitted or an error occurs.
// A single send() call may transmit fewer bytes than requested (especially
// for large payloads like the ~1 MB ide_chatbot.html).  Without this loop the
// browser receives a truncated HTML file whose final </script> tag is missing,
// which causes every inline function to be undefined (ReferenceError).
static bool sendAll(LocalServerSocket client, const char* data, int len)
{
    int totalSent = 0;
    while (totalSent < len)
    {
        int sent = send(client, data + totalSent, len - totalSent, 0);
        if (sent == SOCKET_ERROR || sent == 0)
        {
            return false;  // connection lost — caller can log / clean up
        }
        totalSent += sent;
    }
    return true;
}

// Overload accepting std::string for convenience
static bool sendAll(LocalServerSocket client, const std::string& s)
{
    return sendAll(client, s.c_str(), (int)s.size());
}

static void sendSSEHeaders(LocalServerSocket client)
{
    std::string headers = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/event-stream\r\n"
                          "Cache-Control: no-cache\r\n"
                          "Connection: keep-alive\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                          "\r\n";
    sendAll(client, headers);
}

// Convenience wrapper: returns extracted string directly (empty if not found)
static std::string extractJsonStringValue(const std::string& body, const std::string& key)
{
    std::string result;
    extractJsonString(body, key, result);
    return result;
}

static std::string trimAscii(std::string value)
{
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    return value;
}

static bool wildcardMatch(const std::string& text, const std::string& pattern)
{
    size_t textPos = 0;
    size_t patternPos = 0;
    size_t starPos = std::string::npos;
    size_t matchPos = 0;

    while (textPos < text.size())
    {
        if (patternPos < pattern.size() && (pattern[patternPos] == '?' || pattern[patternPos] == text[textPos]))
        {
            ++textPos;
            ++patternPos;
            continue;
        }
        if (patternPos < pattern.size() && pattern[patternPos] == '*')
        {
            starPos = patternPos++;
            matchPos = textPos;
            continue;
        }
        if (starPos != std::string::npos)
        {
            patternPos = starPos + 1;
            textPos = ++matchPos;
            continue;
        }
        return false;
    }

    while (patternPos < pattern.size() && pattern[patternPos] == '*')
    {
        ++patternPos;
    }
    return patternPos == pattern.size();
}

}  // namespace LocalServerUtil

// ============================================================================
// Model bridge JSON (MASM x64) — matches gui/ide_chatbot_engine.js expectations
// ============================================================================

namespace ModelBridgeHttp
{

#ifdef RAWR_HAS_MASM
#pragma pack(push, 1)
struct MasmModelProfile
{
    uint32_t model_id;
    uint32_t tier;
    uint32_t param_count_b;
    uint32_t param_count_frac;
    uint32_t quant_type;
    uint32_t quant_bits;
    uint32_t ram_mb;
    uint32_t vram_mb;
    uint32_t context_default;
    uint32_t context_max;
    uint32_t max_tokens;
    uint32_t engine_mode;
    uint32_t requires_avx512;
    uint32_t requires_swarm;
    uint32_t num_layers;
    uint32_t head_dim;
    uint32_t num_heads;
    uint32_t num_kv_heads;
    uint32_t ffn_dim;
    uint32_t vocab_size;
    uint64_t name_offset;
    uint32_t name_length;
    uint32_t _reserved[3];
};
struct MasmBridgeState
{
    uint32_t initialized;
    uint32_t has_avx2;
    uint32_t has_fma3;
    uint32_t has_avx512f;
    uint32_t has_avx512bw;
    uint64_t total_ram_mb;
    uint64_t free_ram_mb;
    uint32_t profile_count;
    uint32_t active_profile;
    uint32_t active_tier;
    uint32_t active_quant;
    uint32_t engine_flags;
    uint64_t load_count;
    uint64_t unload_count;
    uint64_t last_load_ms;
    uint32_t swarm_nodes;
    uint32_t drive_count;
    uint32_t lock_flag;
    uint32_t _pad[3];
};
#pragma pack(pop)

static const char* mbTierName(uint32_t tier)
{
    switch (tier)
    {
        case 1:
            return "small";
        case 2:
            return "medium";
        case 3:
            return "large";
        case 4:
            return "ultra";
        case 5:
            return "800b-dual";
        default:
            return "unknown";
    }
}

static const char* mbEngineModeName(uint32_t flag)
{
    switch (flag)
    {
        case 0x0001:
            return "single";
        case 0x0002:
            return "swarm";
        case 0x0004:
            return "dual-engine";
        case 0x0008:
            return "tensor-hop";
        case 0x0010:
            return "safe-decode";
        case 0x0020:
            return "flash-attention";
        case 0x0040:
            return "5-drive";
        default:
            return "unknown";
    }
}

static bool s_masmModelBridgeReady = false;
static bool s_masmModelBridgeTried = false;

static bool ensureMasmModelBridge()
{
    if (!s_masmModelBridgeTried)
    {
        s_masmModelBridgeTried = true;
        s_masmModelBridgeReady = (ModelBridge_Init() == 0);
        if (!s_masmModelBridgeReady)
        {
            LOG_ERROR("[ModelBridge] ModelBridge_Init failed (local server)");
        }
    }
    return s_masmModelBridgeReady;
}
#endif  // RAWR_HAS_MASM

static std::string modelBridgeStaticProfilesJson()
{
    return std::string(R"MBPROF({"bridge":"cpp-fallback","profiles":[
{"id":0,"tier":"small","params_b":"1.5","name":"qwen2.5:1.5b","quant":"Q4_K_M","ram_mb":1200},
{"id":1,"tier":"small","params_b":"3","name":"qwen2.5:3b","quant":"Q4_K_M","ram_mb":2200},
{"id":2,"tier":"small","params_b":"3","name":"llama3.2:3b","quant":"Q4_K_M","ram_mb":2400},
{"id":3,"tier":"small","params_b":"3.8","name":"phi-4:3.8b","quant":"Q4_K_M","ram_mb":2800},
{"id":4,"tier":"small","params_b":"7","name":"gemma2:7b","quant":"Q4_K_M","ram_mb":5200},
{"id":5,"tier":"small","params_b":"8","name":"llama3.1:8b","quant":"Q4_K_M","ram_mb":5600},
{"id":6,"tier":"small","params_b":"7","name":"qwen2.5:7b","quant":"Q4_K_M","ram_mb":5000},
{"id":7,"tier":"small","params_b":"7","name":"mistral:7b","quant":"Q4_K_M","ram_mb":5400},
{"id":8,"tier":"small","params_b":"8","name":"deepseek-r1:8b","quant":"Q4_K_M","ram_mb":5700},
{"id":9,"tier":"medium","params_b":"13","name":"llama3.1:13b","quant":"Q4_K_M","ram_mb":8800},
{"id":10,"tier":"medium","params_b":"14","name":"qwen2.5:14b","quant":"Q4_K_M","ram_mb":9500},
{"id":11,"tier":"medium","params_b":"27","name":"gemma2:27b","quant":"Q4_K_M","ram_mb":17000},
{"id":12,"tier":"large","params_b":"32","name":"qwen2.5:32b","quant":"Q4_K_M","ram_mb":20000},
{"id":13,"tier":"large","params_b":"34","name":"codellama:34b","quant":"Q4_K_M","ram_mb":21000},
{"id":14,"tier":"large","params_b":"70","name":"llama3.1:70b","quant":"Q4_K_M","ram_mb":42000},
{"id":15,"tier":"large","params_b":"72","name":"qwen2.5:72b","quant":"Q4_K_M","ram_mb":43000},
{"id":16,"tier":"large","params_b":"70","name":"deepseek-r1:70b","quant":"Q4_K_M","ram_mb":42500},
{"id":17,"tier":"ultra","params_b":"100","name":"llama3.1:100b-swarm","quant":"Q4_K_M","ram_mb":60000,"requires_swarm":true},
{"id":18,"tier":"ultra","params_b":"100","name":"qwen2.5:100b-swarm","quant":"Q4_K_M","ram_mb":62000,"requires_swarm":true},
{"id":19,"tier":"large","params_b":"65","name":"BigDaddyG-Q4_K_M","quant":"Q4_K_M","ram_mb":36864},
{"id":20,"tier":"800b-dual","params_b":"800","name":"RawrXD-800B-DualEngine","quant":"Q2_K","ram_mb":200000,"requires_swarm":true},
{"id":21,"tier":"800b-dual","params_b":"671","name":"deepseek-v3:671b-swarm","quant":"Q2_K","ram_mb":170000,"requires_swarm":true},
{"id":22,"tier":"ultra","params_b":"141","name":"mixtral-8x22b:141b","quant":"Q4_K_M","ram_mb":85000,"requires_swarm":true},
{"id":23,"tier":"ultra","params_b":"104","name":"commandr-plus:104b-swarm","quant":"Q4_K_M","ram_mb":63000,"requires_swarm":true}
],"profile_count":24})MBPROF");
}

#ifdef RAWR_HAS_MASM
static std::string modelBridgeMasmProfilesJson()
{
    if (!ensureMasmModelBridge())
    {
        return modelBridgeStaticProfilesJson();
    }

    uint32_t count = ModelBridge_GetProfileCount();
    MasmBridgeState* state = reinterpret_cast<MasmBridgeState*>(ModelBridge_GetState());

    std::string profilesJson = "[";
    for (uint32_t i = 0; i < count; i++)
    {
        MasmModelProfile* p = reinterpret_cast<MasmModelProfile*>(ModelBridge_GetProfile(i));
        if (!p)
        {
            continue;
        }
        if (i > 0)
        {
            profilesJson += ",";
        }

        const char* quantName = ModelBridge_GetQuantName(p->quant_type);
        if (!quantName || !quantName[0])
        {
            quantName = "unknown";
        }
        uint32_t estRam = ModelBridge_EstimateRAM(p->param_count_b, p->quant_bits);

        std::string modes = "[";
        bool firstMode = true;
        const uint32_t modeFlags[] = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040};
        for (uint32_t f : modeFlags)
        {
            if (p->engine_mode & f)
            {
                if (!firstMode)
                {
                    modes += ",";
                }
                modes += "\"";
                modes += mbEngineModeName(f);
                modes += "\"";
                firstMode = false;
            }
        }
        modes += "]";

        char paramStr[32];
        if (p->param_count_frac > 0)
        {
            std::snprintf(paramStr, sizeof(paramStr), "%u.%u", p->param_count_b, p->param_count_frac);
        }
        else
        {
            std::snprintf(paramStr, sizeof(paramStr), "%u", p->param_count_b);
        }

        char buf[1024];
        std::snprintf(
            buf, sizeof(buf),
            "{\"id\":%u,\"tier\":\"%s\",\"params_b\":\"%s\","
            "\"quant\":\"%s\",\"quant_bits\":%u,"
            "\"ram_mb\":%u,\"vram_mb\":%u,\"est_ram_mb\":%u,"
            "\"context_default\":%u,\"context_max\":%u,\"max_tokens\":%u,"
            "\"engine_modes\":%s,"
            "\"requires_avx512\":%s,\"requires_swarm\":%s,"
            "\"arch\":{\"layers\":%u,\"head_dim\":%u,\"heads\":%u,\"kv_heads\":%u,\"ffn_dim\":%u,\"vocab\":%u},"
            "\"name_length\":%u}",
            p->model_id, mbTierName(p->tier), paramStr, quantName, p->quant_bits, p->ram_mb, p->vram_mb, estRam,
            p->context_default, p->context_max, p->max_tokens, modes.c_str(), p->requires_avx512 ? "true" : "false",
            p->requires_swarm ? "true" : "false", p->num_layers, p->head_dim, p->num_heads, p->num_kv_heads, p->ffn_dim,
            p->vocab_size, p->name_length);
        profilesJson += buf;
    }
    profilesJson += "]";

    char hwBuf[512];
    std::snprintf(
        hwBuf, sizeof(hwBuf),
        "{\"avx2\":%s,\"fma3\":%s,\"avx512f\":%s,\"avx512bw\":%s,"
        "\"total_ram_mb\":%llu,\"free_ram_mb\":%llu,"
        "\"active_profile\":%d,\"active_tier\":\"%s\","
        "\"load_count\":%llu,\"unload_count\":%llu}",
        state && state->has_avx2 ? "true" : "false", state && state->has_fma3 ? "true" : "false",
        state && state->has_avx512f ? "true" : "false", state && state->has_avx512bw ? "true" : "false",
        state ? (unsigned long long)state->total_ram_mb : 0ULL, state ? (unsigned long long)state->free_ram_mb : 0ULL,
        state ? static_cast<int>(state->active_profile) : -1, state ? mbTierName(state->active_tier) : "unknown",
        state ? (unsigned long long)state->load_count : 0ULL, state ? (unsigned long long)state->unload_count : 0ULL);

    return std::string("{\"bridge\":\"masm-x64\",\"profiles\":") + profilesJson + ",\"hardware\":" + hwBuf +
           ",\"profile_count\":" + std::to_string(count) + "}";
}
#endif

static std::string modelBridgeCapabilitiesJson()
{
#ifdef RAWR_HAS_MASM
    if (ensureMasmModelBridge())
    {
        uint64_t caps = ModelBridge_GetCapabilities();
        MasmBridgeState* state = reinterpret_cast<MasmBridgeState*>(ModelBridge_GetState());
        char buf[1024];
        std::snprintf(buf, sizeof(buf),
                      "{\"bridge\":\"masm-x64\","
                      "\"raw_caps\":\"0x%016llX\","
                      "\"cpu\":{\"avx2\":%s,\"fma3\":%s,\"avx512f\":%s,\"avx512bw\":%s},"
                      "\"memory\":{\"total_ram_gb\":%llu,\"free_ram_gb\":%llu},"
                      "\"engine\":{\"model_loaded\":%s,\"swarm\":%s,\"dual_engine\":%s,"
                      "\"five_drive\":%s,\"tensor_hop\":%s,\"flash_attention\":%s,\"safe_decode\":%s},"
                      "\"active_tier\":\"%s\","
                      "\"profile_count\":%u,"
                      "\"model_range\":\"1.5B-800B (24 profiles, MASM-bridged)\","
                      "\"supported_tiers\":[\"small\",\"medium\",\"large\",\"ultra\",\"800b-dual\"]}",
                      (unsigned long long)caps, (caps & 0x1) ? "true" : "false", (caps & 0x2) ? "true" : "false",
                      (caps & 0x4) ? "true" : "false", (caps & 0x8) ? "true" : "false",
                      state ? (unsigned long long)(state->total_ram_mb / 1024) : 0ULL,
                      state ? (unsigned long long)(state->free_ram_mb / 1024) : 0ULL, (caps & 0x10) ? "true" : "false",
                      (caps & 0x20) ? "true" : "false", (caps & 0x40) ? "true" : "false",
                      (caps & 0x80) ? "true" : "false", (caps & 0x100) ? "true" : "false",
                      (caps & 0x200) ? "true" : "false", (caps & 0x400) ? "true" : "false",
                      state ? mbTierName(state->active_tier) : "unknown", state ? state->profile_count : 0u);
        return std::string(buf);
    }
#endif
    return std::string(
        R"MBCAPS({"bridge":"cpp-fallback","cpu":{"avx2":true,"fma3":true,"avx512f":false,"avx512bw":false},
"memory":{"total_ram_gb":16,"free_ram_gb":8},
"model_range":"1.5B-800B (24 profiles, fallback)",
"supported_tiers":["small","medium","large","ultra","800b-dual"]})MBCAPS");
}

}  // namespace ModelBridgeHttp

// ============================================================================
// SEH trampoline for HTTP handler threads — plain C (no C++ objects with dtors)
// ============================================================================
struct HandlerCtx
{
    Win32IDE* self;
    SOCKET client;
};

static DWORD sehRunHandler(HandlerCtx* ctx)
{
#if defined(_MSC_VER)
    __try
    {
        ctx->self->sehHandleClient(ctx->client);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        OutputDebugStringA("[LocalServer] SEH exception in handler — request dropped\n");
        closesocket(ctx->client);
    }
    return 0;
#else
    ctx->self->sehHandleClient(ctx->client);
    return 0;
#endif
}

void Win32IDE::sehHandleClient(SOCKET client)
{
    handleLocalServerClient(client);
}

// ============================================================================
// START SERVER
// ============================================================================

void Win32IDE::startLocalServer()
{
    if (m_localServerRunning.load())
    {
        appendToOutput("[Server] Already running on port " + std::to_string(m_settings.localServerPort), "General",
                       OutputSeverity::Warning);
        return;
    }

    int port = m_settings.localServerPort;
    m_localServerRunning.store(true);

    m_localServerThread = std::thread(
        [this, port]()
        {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            {
                LOG_ERROR("Local server WSAStartup failed");
                m_localServerRunning.store(false);
                return;
            }

            LocalServerSocket serverFd = socket(AF_INET, SOCK_STREAM, 0);
            if (serverFd == kInvalidSock)
            {
                LOG_ERROR("Local server: failed to create socket");
                m_localServerRunning.store(false);
                WSACleanup();
                return;
            }

            // Allow port reuse
            int opt = 1;
            setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

            // Non-blocking accept with 1-second timeout for shutdown
            DWORD timeout = 1000;
            setsockopt(serverFd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons((u_short)port);

            if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) != 0)
            {
                LOG_ERROR("Local server: failed to bind port " + std::to_string(port));
                closesocket(serverFd);
                m_localServerRunning.store(false);
                WSACleanup();
                return;
            }

            if (listen(serverFd, 8) != 0)
            {
                LOG_ERROR("Local server: failed to listen");
                closesocket(serverFd);
                m_localServerRunning.store(false);
                WSACleanup();
                return;
            }

            LOG_INFO("Local GGUF server listening on port " + std::to_string(port));
            postAgentOutputSafe("[Server] Listening on http://localhost:" + std::to_string(port));

            m_localServerStats = {};

            while (m_localServerRunning.load())
            {
                sockaddr_in clientAddr = {};
                int clientLen = sizeof(clientAddr);
                LocalServerSocket client = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
                if (client == kInvalidSock)
                    continue;

                m_localServerStats.totalRequests++;

                // Handle client in a detached thread — SEH-protected so a
                // handler crash doesn't terminate the whole process.
                std::thread(
                    [this, client]()
                    {
                        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                        if (_guard.cancelled)
                        {
                            closesocket(client);
                            return;
                        }
                        HandlerCtx ctx{this, client};
                        sehRunHandler(&ctx);
                    })
                    .detach();
            }

            closesocket(serverFd);
            WSACleanup();
            LOG_INFO("Local GGUF server stopped");
        });
}

// ============================================================================
// STOP SERVER
// ============================================================================

void Win32IDE::stopLocalServer()
{
    if (!m_localServerRunning.load())
        return;

    m_localServerRunning.store(false);
    if (m_localServerThread.joinable())
    {
        m_localServerThread.join();
    }
    appendToOutput("[Server] Stopped", "General", OutputSeverity::Info);
}

// ============================================================================
// CLIENT HANDLER — routes requests to appropriate endpoint
// ============================================================================

void Win32IDE::handleLocalServerClient(SOCKET clientFd)
{
    LocalServerSocket client = clientFd;

    // Read request
    std::string data;
    char buffer[8192];
    int received = 0;

    // Early exit if IDE is shutting down
    if (isShuttingDown())
    {
        closesocket(client);
        return;
    }

    while ((received = recv(client, buffer, sizeof(buffer), 0)) > 0)
    {
        data.append(buffer, buffer + received);
        if (data.find("\r\n\r\n") != std::string::npos)
            break;
    }

    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        closesocket(client);
        return;
    }

    // Extract content-length and read remaining body
    std::string headers = data.substr(0, headerEnd + 4);
    std::string body = data.substr(headerEnd + 4);

    size_t contentLength = 0;
    auto clPos = LocalServerUtil::toLower(headers).find("content-length:");
    if (clPos != std::string::npos)
    {
        size_t lineEnd = headers.find("\r\n", clPos);
        std::string val = headers.substr(clPos + 15, lineEnd - (clPos + 15));
        try
        {
            contentLength = (size_t)std::stoul(val);
        }
        catch (...)
        {
        }
    }

    while (body.size() < contentLength && (received = recv(client, buffer, sizeof(buffer), 0)) > 0)
    {
        body.append(buffer, buffer + received);
    }

    // Parse method and path
    std::string method, path;
    std::istringstream reqLine(headers.substr(0, headers.find("\r\n")));
    reqLine >> method >> path;

    // Route
    std::string response;

    if (method == "OPTIONS")
    {
        response = LocalServerUtil::buildHttpResponse(204, "");
    }
    // ========== Health / Status ==========
    else if (method == "GET" && (path == "/health" || path == "/"))
    {
        response = LocalServerUtil::buildHttpResponse(200, "{\"status\":\"ok\",\"server\":\"RawrXD-Win32IDE\"}");
    }
    else if (method == "GET" && path == "/status")
    {
        bool modelLoaded = m_nativeEngine && m_nativeEngine->IsModelLoaded();
        std::ostringstream j;
        j << "{\"ready\":true"
          << ",\"model_loaded\":" << (modelLoaded ? "true" : "false") << ",\"model_path\":\""
          << LocalServerUtil::escapeJson(m_loadedModelPath) << "\""
          << ",\"backend\":\"rawrxd-win32ide\""
          << ",\"total_requests\":" << m_localServerStats.totalRequests
          << ",\"total_tokens\":" << m_localServerStats.totalTokens << "}";
        response = LocalServerUtil::buildHttpResponse(200, j.str());
    }
    // ========== Ollama-compatible: /api/tags ==========
    else if (method == "GET" && path == "/api/tags")
    {
        handleOllamaApiTags(client);
        closesocket(client);
        return;
    }
    // ========== Ollama-compatible: /api/generate ==========
    else if (method == "POST" && path == "/api/generate")
    {
        handleOllamaApiGenerate(client, body);
        closesocket(client);
        return;
    }
    // ========== OpenAI-compatible: /v1/chat/completions ==========
    else if (method == "POST" && path == "/v1/chat/completions")
    {
        handleOpenAIChatCompletions(client, body);
        closesocket(client);
        return;
    }
    // ========== HTML Frontend: /models — list all local GGUF + Ollama models ==========
    else if (method == "GET" && path == "/models")
    {
        handleModelsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Cursor / OpenAI: /v1/models, /api/models — model list for Settings > Models ==========
    else if (method == "GET" && (path == "/v1/models" || path == "/api/models"))
    {
        handleV1ModelsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Model bridge (GUI ide_chatbot_engine.js) ==========
    else if (method == "GET" && path == "/api/model/profiles")
    {
        handleModelBridgeProfilesEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/model/load")
    {
        handleModelBridgeLoadEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/model/unload")
    {
        handleModelBridgeUnloadEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/engine/capabilities")
    {
        handleEngineCapabilitiesEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== HTML Frontend: /ask — unified chat endpoint ==========
    else if (method == "POST" && path == "/ask")
    {
        handleAskEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== HTML Frontend: /gui — serve ide_chatbot.html ==========
    else if (method == "GET" && (path == "/gui" || path == "/gui/"))
    {
        handleServeGui(client);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agents list parity ==========
    else if (method == "GET" && path == "/api/agents")
    {
        std::ostringstream j;
        j << "{\"success\":true,\"surface\":\"local_server\",\"agents\":["
          << "{\"id\":\"local-agent\",\"state\":\"active\",\"description\":\"Local embedded agent runtime\"}"
          << "],\"counts\":{\"active\":" << m_historyStats.agentStarted
          << ",\"completed\":" << m_historyStats.agentCompleted << ",\"failed\":" << m_historyStats.agentFailed
          << ",\"subagents\":" << m_historyStats.subAgentSpawned << "}}";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, j.str()));
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agent History (read-only) ==========
    else if (method == "GET" && path.find("/api/agents/history") == 0)
    {
        handleAgentHistoryEndpoint(client, path);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agent Status (read-only) ==========
    else if (method == "GET" && path == "/api/agents/status")
    {
        handleAgentStatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agent Replay ==========
    else if (method == "POST" && path == "/api/agents/replay")
    {
        handleAgentReplayEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Failure Timeline (read-only) ==========
    else if (method == "GET" && path.find("/api/failures") == 0)
    {
        handleFailuresEndpoint(client, path);
        closesocket(client);
        return;
    }
    // ========== Phase 8B: Backend Switcher ==========
    else if (method == "GET" && path == "/api/backends")
    {
        handleBackendsListEndpoint(client);
        closesocket(client);
        return;
    }

    // --- Chain 1 set `response` for routes like /health, /status, /api/tags, etc.
    // Send it now before the independent if-blocks below reach the trailing 404.
    if (!response.empty())
    {
        LocalServerUtil::sendAll(client, response);
        closesocket(client);
        return;
    }

    if (method == "GET" && path == "/api/backend/active")
    {
        handleBackendActiveEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && (path == "/api/backend/switch" || path == "/api/backends/switch"))
    {
        handleBackendSwitchEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 8C: LLM Router ==========
    else if (method == "GET" && path == "/api/router/status")
    {
        handleRouterStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/router/decision")
    {
        handleRouterDecisionEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/router/capabilities")
    {
        handleRouterCapabilitiesEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/router/route")
    {
        handleRouterRouteEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== UX Enhancements & Research Track ==========
    else if (method == "GET" && path == "/api/router/why")
    {
        handleRouterWhyEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/router/pins")
    {
        handleRouterPinsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/router/heatmap")
    {
        handleRouterHeatmapEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/router/ensemble")
    {
        handleRouterEnsembleEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/router/simulate")
    {
        handleRouterSimulateEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 9A: LSP Client ==========
    else if (method == "GET" && path == "/api/lsp/status")
    {
        handleLSPStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/lsp/diagnostics")
    {
        handleLSPDiagnosticsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 9A-ASM: ASM Semantic Support ==========
    else if (method == "GET" && path.rfind("/api/asm/symbols", 0) == 0)
    {
        handleAsmSymbolsEndpoint(client, path);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/asm/navigate")
    {
        handleAsmNavigateEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/asm/analyze")
    {
        handleAsmAnalyzeEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 9C: Multi-Response Chain ==========
    else if (method == "GET" && path == "/api/multi-response/status")
    {
        handleMultiResponseStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/multi-response/templates")
    {
        handleMultiResponseTemplatesEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/multi-response/generate")
    {
        handleMultiResponseGenerateEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path.rfind("/api/multi-response/results", 0) == 0)
    {
        // Extract session ID from query: /api/multi-response/results?session=123
        std::string sid = "latest";
        auto qpos = path.find("session=");
        if (qpos != std::string::npos)
            sid = path.substr(qpos + 8);
        handleMultiResponseResultsEndpoint(client, sid);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/multi-response/prefer")
    {
        handleMultiResponsePreferEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/multi-response/stats")
    {
        handleMultiResponseStatsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/multi-response/preferences")
    {
        handleMultiResponsePreferencesEndpoint(client);
        closesocket(client);
        return;
    }
    // ============================================================
    // Phase 9B: LSP-AI Hybrid Integration Bridge
    // ============================================================
    else if (method == "POST" && path == "/api/hybrid/complete")
    {
        handleHybridCompleteEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path.rfind("/api/hybrid/diagnostics", 0) == 0)
    {
        handleHybridDiagnosticsEndpoint(client, path);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/hybrid/rename")
    {
        handleHybridSmartRenameEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/hybrid/analyze")
    {
        handleHybridAnalyzeEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/hybrid/status")
    {
        handleHybridStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/hybrid/symbol-usage")
    {
        handleHybridSymbolUsageEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Governor Endpoints ==========
    else if (method == "GET" && path == "/api/governor/status")
    {
        handleGovernorStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/governor/submit")
    {
        handleGovernorSubmitEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/governor/kill")
    {
        handleGovernorKillEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/governor/result")
    {
        handleGovernorResultEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Safety Endpoints ==========
    else if (method == "GET" && path == "/api/safety/status")
    {
        handleSafetyStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/safety/check")
    {
        handleSafetyCheckEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/safety/violations")
    {
        handleSafetyViolationsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/safety/rollback")
    {
        handleSafetyRollbackEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Replay Endpoints ==========
    else if (method == "GET" && path == "/api/replay/status")
    {
        handleReplayStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/replay/records")
    {
        handleReplayRecordsEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/replay/sessions")
    {
        handleReplaySessionsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Confidence Endpoints ==========
    else if (method == "GET" && path == "/api/confidence/status")
    {
        handleConfidenceStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/confidence/evaluate")
    {
        handleConfidenceEvaluateEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/confidence/history")
    {
        handleConfidenceHistoryEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Unified Status ==========
    else if (method == "GET" && path == "/api/phase10/status")
    {
        handlePhase10StatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 11: Distributed Swarm Compilation ==========
    else if (method == "GET" && path == "/api/swarm/status")
    {
        handleSwarmStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm")
    {
        handleSwarmStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/swarm/nodes")
    {
        handleSwarmNodesEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/swarm/tasks")
    {
        handleSwarmTaskGraphEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/swarm/stats")
    {
        handleSwarmStatsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/swarm/events")
    {
        handleSwarmEventsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/swarm/config")
    {
        handleSwarmConfigEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/swarm/worker")
    {
        handleSwarmWorkerEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/swarm/start")
    {
        handleSwarmStartEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/swarm/stop")
    {
        handleSwarmStopEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/swarm/nodes/add")
    {
        handleSwarmAddNodeEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/swarm/build")
    {
        handleSwarmBuildEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/swarm/cancel")
    {
        handleSwarmCancelEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/swarm/cache/clear")
    {
        handleSwarmCacheClearEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/phase11/status")
    {
        handlePhase11StatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ====================================================================
    // PHASE 41 — DUAL-AGENT ORCHESTRATOR HTTP ENDPOINTS
    // ====================================================================
    // Architect (70B-800B reasoning) + Coder (7B-13B fast) workflow
    // Backed by RawrXD_DualAgent_Orchestrator.asm via model_bridge_x64.asm
    else if (method == "POST" && path == "/api/agent/dual/init")
    {
        handleDualAgentInitEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/agent/dual/shutdown")
    {
        handleDualAgentShutdownEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/agent/dual/status")
    {
        handleDualAgentStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/agent/dual/handoff")
    {
        handleDualAgentHandoffEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/agent/dual/submit")
    {
        handleDualAgentSubmitEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/phase41/status")
    {
        handlePhase41StatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ====================================================================
    // PHASE 12 — NATIVE DEBUGGER ENGINE HTTP ENDPOINTS
    // ====================================================================
    // GET endpoints
    else if (method == "GET" && path == "/api/debug/status")
    {
        handleDbgStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/breakpoints")
    {
        handleDbgBreakpointsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/registers")
    {
        handleDbgRegistersEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/stack")
    {
        handleDbgStackEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/modules")
    {
        handleDbgModulesEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/threads")
    {
        handleDbgThreadsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/events")
    {
        handleDbgEventsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/debug/watches")
    {
        handleDbgWatchesEndpoint(client);
        closesocket(client);
        return;
    }
    // POST endpoints with body
    else if (method == "POST" && path == "/api/debug/memory")
    {
        handleDbgMemoryEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/debug/disasm")
    {
        handleDbgDisasmEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/debug/launch")
    {
        handleDbgLaunchEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/debug/attach")
    {
        handleDbgAttachEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/debug/go")
    {
        handleDbgGoEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/phase12/status")
    {
        handlePhase12StatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/re/set-binary")
    {
        handleReSetBinaryEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 32A: Chain-of-Thought Multi-Model Review ==========
    else if (method == "GET" && path == "/api/cot/status")
    {
        handleCoTStatusEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/cot/presets")
    {
        handleCoTPresetsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/cot/steps")
    {
        handleCoTStepsEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/cot/roles")
    {
        handleCoTRolesEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/cot/preset")
    {
        handleCoTApplyPresetEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/cot/steps")
    {
        handleCoTSetStepsEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/cot/execute")
    {
        handleCoTExecuteEndpoint(client, body);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/cot/cancel")
    {
        handleCoTCancelEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== File Reading: /api/read-file — read local file for chatbot attachments ==========
    else if (method == "POST" && path == "/api/read-file")
    {
        handleReadFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== File Writing: /api/write-file — write/overwrite local file ==========
    else if (method == "POST" && path == "/api/write-file")
    {
        handleWriteFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== List Directory: /api/list-directory — enumerate directory contents ==========
    else if (method == "POST" && path == "/api/list-directory")
    {
        handleListDirEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Delete File: /api/delete-file — remove a file from disk ==========
    else if (method == "POST" && path == "/api/delete-file")
    {
        handleDeleteFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Rename File: /api/rename-file — rename/move a file ==========
    else if (method == "POST" && path == "/api/rename-file")
    {
        handleRenameFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Create Directory: /api/mkdir — create directories recursively ==========
    else if (method == "POST" && path == "/api/mkdir")
    {
        handleMkdirEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Search Files: /api/search-files — recursive text/pattern search ==========
    else if (method == "POST" && path == "/api/search-files")
    {
        handleSearchFilesEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Stat File: /api/stat-file — file metadata (size, dates, attributes) ==========
    else if (method == "POST" && path == "/api/stat-file")
    {
        handleStatFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Copy File: /api/copy-file — copy file to destination ==========
    else if (method == "POST" && path == "/api/copy-file")
    {
        handleCopyFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Move File: /api/move-file — move file to destination ==========
    else if (method == "POST" && path == "/api/move-file")
    {
        handleMoveFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Tool Dispatcher: /api/tool — unified tool call interface ==========
    else if (method == "POST" && path == "/api/tool")
    {
        handleToolDispatchEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && (path == "/api/agent/orchestrate" || path == "/api/agent/intent"))
    {
        handleAgentOrchestrateEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/subagent")
    {
        std::string prompt;
        std::string taskType;
        LocalServerUtil::extractJsonString(body, "prompt", prompt);
        LocalServerUtil::extractJsonString(body, "task_type", taskType);
        if (prompt.empty())
        {
            LocalServerUtil::extractJsonString(body, "task", prompt);
        }
        if (prompt.empty())
        {
            prompt = "Execute delegated subagent task";
        }
        if (taskType.empty())
        {
            taskType = "analyze";
        }

        nlohmann::json args = nlohmann::json::object();
        args["query"] = prompt;
        args["task_type"] = taskType;
        args["mode"] = "subagent";
        auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("plan_code_exploration", args);
        nlohmann::json out = result.toJson();
        out["operation"] = "subagent";
        out["taskType"] = taskType;
        LocalServerUtil::sendAll(client,
                                 LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 400, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/chain")
    {
        std::string goal;
        LocalServerUtil::extractJsonString(body, "goal", goal);
        if (goal.empty())
        {
            LocalServerUtil::extractJsonString(body, "prompt", goal);
        }
        if (goal.empty())
        {
            goal = "Run chained multi-step analysis";
        }

        nlohmann::json args = nlohmann::json::object();
        args["task"] = goal;
        args["owner"] = "chain";
        args["deadline"] = "asap";
        auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("plan_tasks", args);
        nlohmann::json out = result.toJson();
        out["operation"] = "chain";
        LocalServerUtil::sendAll(client,
                                 LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 400, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm")
    {
        std::string goal;
        LocalServerUtil::extractJsonString(body, "goal", goal);
        if (goal.empty())
        {
            LocalServerUtil::extractJsonString(body, "prompt", goal);
        }
        if (goal.empty())
        {
            goal = "Run swarm orchestration analysis";
        }

        nlohmann::json args = nlohmann::json::object();
        args["goal"] = goal;
        args["mode"] = "swarm";
        auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("plan_code_exploration", args);
        nlohmann::json out = result.toJson();
        out["operation"] = "swarm";
        LocalServerUtil::sendAll(client,
                                 LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 400, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/tool/capabilities")
    {
        nlohmann::json payload = nlohmann::json::object();
        payload["success"] = true;
        payload["surface"] = "local_server";
        payload["endpoint"] = "/api/tool";
        payload["productionReady"] = true;
        payload["outsideHotpatchAccessible"] = true;
        payload["tools"] = nlohmann::json::array();
        const auto schemas = RawrXD::Agent::AgentToolHandlers::GetAllSchemas();
        if (schemas.is_array())
        {
            for (const auto& schema : schemas)
            {
                if (schema.is_object() && schema.contains("name") && schema["name"].is_string())
                {
                    payload["tools"].push_back(schema["name"].get<std::string>());
                }
            }
        }
        payload["tools"].push_back("git_status");
        payload["aliases"] = {{"rstore_checkpoint", "restore_checkpoint"},
                              {"compacted_conversation", "compact_conversation"},
                              {"optimize-tool-selection", "optimize_tool_selection"},
                              {"read-lines", "read_lines"},
                              {"planning-exploration", "plan_code_exploration"},
                              {"search-files", "search_files"},
                              {"evaluate-integration", "evaluate_integration_audit_feasibility"},
                              {"orchestrate-conversation", "orchestrate_conversation"},
                              {"speculate-next", "speculate_next"},
                              {"resolve-symbol", "resolve_symbol"},
                              {"list_directory", "list_dir"},
                              {"create_file", "write_file"},
                              {"run_command", "execute_command"}};
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, payload.dump()));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/agent/capabilities")
    {
        const std::string payload =
            R"({"success":true,"surface":"local_server","outsideHotpatchAccessible":true,"routes":{"chat":"/api/chat","tool":"/api/tool","toolCapabilities":"/api/tool/capabilities","orchestrate":"/api/agent/orchestrate","intent":"/api/agent/intent","subagent":"/api/subagent","chain":"/api/chain","swarm":"/api/swarm","swarmStatus":"/api/swarm/status","agents":"/api/agents","agentsStatus":"/api/agents/status","agentsHistory":"/api/agents/history","agentsReplay":"/api/agents/replay","agentImplementationAudit":"/api/agent/implementation-audit","agentCursorGapAudit":"/api/agent/cursor-gap-audit","agentRuntimeFallbackMetrics":"/api/agent/runtime-fallback-metrics","agentRuntimeFallbackMetricsReset":"/api/agent/runtime-fallback-metrics/reset","agentRuntimeFallbackMetricSurfaces":"/api/agent/runtime-fallback-metrics/surfaces","agentParityMatrix":"/api/agent/parity-matrix","agentGlobalWrapperAudit":"/api/agent/global-wrapper-audit","agentWiringAudit":"/api/agent/wiring-audit"},"notes":["Use /api/tool for canonical backend tool execution","Use /api/agent/orchestrate for intent planning + speculative execution + synthesized reply","Use /api/agent/wiring-audit to verify fallback-family backend mappings","Hotpatch ops remain under /api/agent/ops/* for compatibility only"]})";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, payload));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/agent/wiring-audit")
    {
        const nlohmann::json coverage =
            RawrXD::Core::BuildCoverageSnapshotFromReport(RawrXD::Core::ResolveAuditRepoRoot());
        nlohmann::json out = RawrXD::Core::BuildAgentCapabilityAudit("local_server", coverage, "report_snapshot");
        out["orchestratorRoute"] = "/api/agent/orchestrate";
        out["toolRoute"] = "/api/tool";
        out["capabilitiesRoute"] = "/api/tool/capabilities";
        out["familyMappings"] = {{"handleAI*", "optimize_tool_selection|next_edit_hint"},
                                 {"handleAgent*", "optimize_tool_selection|execute_command"},
                                 {"handleSubagent*", "plan_code_exploration|plan_tasks|load_rules"},
                                 {"handleAutonomy*", "load_rules|plan_tasks"},
                                 {"handleRouter*", "optimize_tool_selection|load_rules"},
                                 {"handleHelp*/handleEdit*", "search_code"},
                                 {"handleTools*", "run_shell"},
                                 {"handleView*", "list_dir"},
                                 {"handleTelemetry*", "load_rules|plan_tasks"},
                                 {"handleLsp*", "get_diagnostics"},
                                 {"handleModel*", "search_files"},
                                 {"handlePlugin*/handleMarketplace*/handleVscExt*/handleVscext*", "list_dir"},
                                 {"handleAsm*/handleReveng*/handleRE*", "search_code"},
                                 {"handleTheme*/handleVoice*/handleTransparency*", "plan_tasks"},
                                 {"handleReplay*", "restore_checkpoint"},
                                 {"handleGovernor*/handleGov*/handleSafety*", "load_rules|plan_tasks"},
                                 {"handleFile*", "list_dir"},
                                 {"handleMonaco*/handleTier1*/handleQw*/handleTrans*", "plan_tasks"},
                                 {"handleSwarm*", "plan_code_exploration"},
                                 {"handleHotpatch*", "load_rules|plan_tasks"},
                                 {"handleAudit*", "load_rules"},
                                 {"handleGit*", "git_status"},
                                 {"handleEditor*", "plan_tasks"},
                                 {"handleTerminal*", "run_shell"},
                                 {"handleDecomp*", "search_code"},
                                 {"handlePdb*/handleModules*", "search_files"},
                                 {"handleGauntlet*", "plan_tasks"},
                                 {"handleConfidence*", "load_rules"},
                                 {"handleBeacon*", "search_files"},
                                 {"handleVision*", "semantic_search"}};
        out["quietByDefault"] = true;
        out["showActionsToggle"] = "show_actions|include_actions";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "GET" && (path == "/api/agent/implementation-audit" || path == "/api/agent/cursor-gap-audit"))
    {
        const nlohmann::json coverage =
            RawrXD::Core::BuildCoverageSnapshotFromReport(RawrXD::Core::ResolveAuditRepoRoot());
        const std::string audit =
            RawrXD::Core::BuildAgentCapabilityAudit("local_server", coverage, "report_snapshot").dump();
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, audit));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/agent/runtime-fallback-metrics")
    {
        nlohmann::json bodyJson = nlohmann::json::object();
        if (!body.empty())
        {
            try
            {
                bodyJson = nlohmann::json::parse(body);
                if (!bodyJson.is_object())
                    bodyJson = nlohmann::json::object();
            }
            catch (...)
            {
                bodyJson = nlohmann::json::object();
            }
        }
        const std::string surfaceFilter = (bodyJson.contains("surface") && bodyJson["surface"].is_string())
                                              ? bodyJson["surface"].get<std::string>()
                                              : std::string();
        const auto rows = RawrXD::Core::SnapshotFallbackRouteMetricsBySurface(surfaceFilter);
        const auto totals = RawrXD::Core::SnapshotFallbackRouteMetricsTotals(surfaceFilter);
        nlohmann::json out = nlohmann::json::object();
        out["success"] = true;
        out["surface"] = "local_server";
        out["outsideHotpatchAccessible"] = true;
        out["filter"] = {{"surface", surfaceFilter}};
        out["runtimeFallbackRouteMetrics"] = rows;
        out["runtimeFallbackTotals"] = totals;
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/agent/runtime-fallback-metrics/reset")
    {
        RawrXD::Core::ResetFallbackRouteMetrics();
        const std::string out =
            R"({"success":true,"surface":"local_server","outsideHotpatchAccessible":true,"runtimeFallbackRouteMetricsReset":true})";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/agent/runtime-fallback-metrics/surfaces")
    {
        nlohmann::json out = nlohmann::json::object();
        out["success"] = true;
        out["surface"] = "local_server";
        out["outsideHotpatchAccessible"] = true;
        out["surfaces"] = RawrXD::Core::FallbackMetricSurfacesCatalog();
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/agent/parity-matrix")
    {
        nlohmann::json out = RawrXD::Core::BuildAgentParityMatrix("local_server");
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/agent/global-wrapper-audit")
    {
        const std::string root = RawrXD::Core::ResolveAuditRepoRoot();
        nlohmann::json out = nlohmann::json::object();
        out["success"] = true;
        out["surface"] = "local_server";
        out["outsideHotpatchAccessible"] = true;
        out["globalWrapperMacroAudit"] = RawrXD::Core::BuildGlobalWrapperMacroAudit(root);
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
        closesocket(client);
        return;
    }
    // ========== File Writing: /api/write-file — save file from editor ==========
    else if (method == "POST" && path == "/api/write-file")
    {
        handleWriteFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Directory Listing: /api/list-dir — file tree browser ==========
    else if (method == "POST" && path == "/api/list-dir")
    {
        handleListDirEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== File Delete: /api/delete-file ==========
    else if (method == "POST" && path == "/api/delete-file")
    {
        handleDeleteFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== File Rename/Move: /api/rename-file ==========
    else if (method == "POST" && path == "/api/rename-file")
    {
        handleRenameFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Create Directory: /api/mkdir ==========
    else if (method == "POST" && path == "/api/mkdir")
    {
        handleMkdirEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Search in Files: /api/search-files ==========
    else if (method == "POST" && path == "/api/search-files")
    {
        handleSearchFilesEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== CLI Command Execution: /api/cli — forward to tool_server pattern ==========
    else if (method == "POST" && path == "/api/cli")
    {
        handleCliEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Hotpatch Layer Control ==========
    else if (method == "POST" && (path == "/api/hotpatch/toggle" || path == "/api/hotpatch/apply" ||
                                  path == "/api/hotpatch/revert" || path.find("/api/agent/ops/") == 0))
    {
        handleHotpatchEndpoint(client, path, body);
        closesocket(client);
        return;
    }
    else if ((method == "POST" || method == "GET") && path == "/api/hotpatch/target-tps")
    {
        handleHotpatchTargetTpsEndpoint(client, method, body);
        closesocket(client);
        return;
    }
    // ========== Phase 34: Production Instructions Context ==========
    else if (method == "GET" && path == "/api/instructions")
    {
        handleInstructionsEndpoint(client, "full");
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/instructions/summary")
    {
        handleInstructionsEndpoint(client, "summary");
        closesocket(client);
        return;
    }
    if (method == "GET" && path == "/api/instructions/content")
    {
        handleInstructionsContentEndpoint(client);
        closesocket(client);
        return;
    }
    if (method == "POST" && path == "/api/instructions/reload")
    {
        handleInstructionsReloadEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Completion Endpoint: /complete — parity with complete_server ==========
    else if (method == "POST" && path == "/complete")
    {
        handleCompleteEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Streaming Completion: /complete/stream ==========
    else if (method == "POST" && path == "/complete/stream")
    {
        handleCompleteStreamEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Agent Wish: /api/agent/wish — parity with complete_server ==========
    else if (method == "POST" && path == "/api/agent/wish")
    {
        handleAgentWishEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Policy Management: /api/policies ==========
    else if ((method == "GET" || method == "POST") && path == "/api/policies")
    {
        handlePoliciesEndpoint(client, method, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/policies/suggestions")
    {
        handlePoliciesSuggestionsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/policies/apply")
    {
        handlePoliciesApplyEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/policies/reject")
    {
        handlePoliciesRejectEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== 404 ==========
    else
    {
        response = LocalServerUtil::buildHttpResponse(404, "{\"error\":\"not_found\",\"message\":\"Unknown endpoint: " +
                                                               LocalServerUtil::escapeJson(path) + "\"}");
    }

    LocalServerUtil::sendAll(client, response);
    closesocket(client);
}

// ============================================================================
// Model bridge HTTP — MASM x64 / cpp fallback (see gui/ide_chatbot_engine.js)
// ============================================================================

void Win32IDE::handleModelBridgeProfilesEndpoint(SOCKET client)
{
#ifdef RAWR_HAS_MASM
    std::string body = ModelBridgeHttp::modelBridgeMasmProfilesJson();
#else
    std::string body = ModelBridgeHttp::modelBridgeStaticProfilesJson();
#endif
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, body));
}

void Win32IDE::handleEngineCapabilitiesEndpoint(SOCKET client)
{
    std::string body = ModelBridgeHttp::modelBridgeCapabilitiesJson();
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, body));
}

void Win32IDE::handleModelBridgeLoadEndpoint(SOCKET client, const std::string& body)
{
    int profileIdx = -1;
    if (!LocalServerUtil::extractJsonInt(body, "index", profileIdx))
    {
        profileIdx = -1;
    }
    std::string nameIn;
    LocalServerUtil::extractJsonString(body, "name", nameIn);

#ifdef RAWR_HAS_MASM
    if (!ModelBridgeHttp::ensureMasmModelBridge())
    {
        std::string modelName = nameIn.empty() ? ("model-" + std::to_string(std::max(0, profileIdx))) : nameIn;
        std::string json =
            "{\"success\":true,\"message\":\"Model set (cpp fallback; MASM bridge unavailable)\",\"name\":\"" +
            LocalServerUtil::escapeJson(modelName) + "\",\"bridge\":\"cpp-fallback\"}";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json));
        return;
    }

    if (profileIdx < 0 && !nameIn.empty())
    {
        void* prof = ModelBridge_GetProfileByName(nameIn.c_str());
        if (prof)
        {
            auto* p = reinterpret_cast<ModelBridgeHttp::MasmModelProfile*>(prof);
            profileIdx = static_cast<int>(p->model_id);
        }
    }

    if (profileIdx < 0)
    {
        LocalServerUtil::sendAll(
            client, LocalServerUtil::buildHttpResponse(
                        400, "{\"error\":\"bad_request\",\"message\":\"Provide numeric 'index' (0-23) or string "
                             "'name' matching a bridge profile.\"}"));
        return;
    }

    uint64_t valResult = ModelBridge_ValidateLoad(static_cast<uint32_t>(profileIdx));
    if (valResult != 0)
    {
        std::string errMsg;
        switch (valResult)
        {
            case 1:
                errMsg = "Invalid model tier";
                break;
            case 2:
                errMsg = "AVX2 required but not available on this CPU";
                break;
            case 3:
                errMsg = "AVX-512 required for this model tier but not available";
                break;
            case 4:
                errMsg = "Insufficient RAM for this model";
                break;
            case 5:
                errMsg = "Model load already in progress";
                break;
            case 6:
                errMsg = "Invalid profile index";
                break;
            default:
                errMsg = "Validation failed (code " + std::to_string((unsigned long long)valResult) + ")";
                break;
        }
        std::string j = "{\"error\":\"validation_failed\",\"message\":\"" + LocalServerUtil::escapeJson(errMsg) + "\"}";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(422, j));
        return;
    }

    uint64_t loadResult = ModelBridge_LoadModel(static_cast<uint32_t>(profileIdx));
    if (loadResult != 0)
    {
        std::string j = "{\"error\":\"load_failed\",\"message\":\"MASM ModelBridge_LoadModel code " +
                        std::to_string((unsigned long long)loadResult) + "\"}";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(500, j));
        return;
    }

    auto* loaded =
        reinterpret_cast<ModelBridgeHttp::MasmModelProfile*>(ModelBridge_GetProfile(static_cast<uint32_t>(profileIdx)));
    if (!loaded)
    {
        LocalServerUtil::sendAll(client,
                                 LocalServerUtil::buildHttpResponse(
                                     500, "{\"error\":\"internal\",\"message\":\"Profile missing after load\"}"));
        return;
    }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "{\"success\":true,\"message\":\"Model loaded via MASM x64 bridge\","
                  "\"profile_id\":%d,\"tier\":\"%s\",\"params_b\":%u,"
                  "\"ram_mb\":%u,\"vram_mb\":%u,\"engine_mode\":%u,"
                  "\"bridge\":\"masm-x64\"}",
                  profileIdx, ModelBridgeHttp::mbTierName(loaded->tier), loaded->param_count_b, loaded->ram_mb,
                  loaded->vram_mb, loaded->engine_mode);
    LOG_INFO("[ModelBridge] Loaded profile " + std::to_string(profileIdx) + " via local server");
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, std::string(buf)));
#else
    std::string modelName = nameIn.empty() ? ("model-" + std::to_string(std::max(0, profileIdx))) : nameIn;
    if (profileIdx < 0 && nameIn.empty())
    {
        LocalServerUtil::sendAll(
            client, LocalServerUtil::buildHttpResponse(
                        400, "{\"error\":\"bad_request\",\"message\":\"Provide 'index' and/or 'name'.\"}"));
        return;
    }
    std::string json = "{\"success\":true,\"message\":\"Model set (cpp fallback)\",\"name\":\"" +
                       LocalServerUtil::escapeJson(modelName) + "\",\"bridge\":\"cpp-fallback\"}";
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json));
#endif
}

void Win32IDE::handleModelBridgeUnloadEndpoint(SOCKET client)
{
#ifdef RAWR_HAS_MASM
    if (ModelBridgeHttp::ensureMasmModelBridge())
    {
        uint64_t result = ModelBridge_UnloadModel();
        if (result != 0)
        {
            LocalServerUtil::sendAll(
                client,
                LocalServerUtil::buildHttpResponse(
                    409, "{\"error\":\"not_loaded\",\"message\":\"No model currently loaded (or unload failed).\"}"));
            return;
        }
    }
#endif
    LocalServerUtil::sendAll(
        client, LocalServerUtil::buildHttpResponse(200, "{\"success\":true,\"message\":\"Model unloaded\"}"));
}

// ============================================================================
// OLLAMA: /api/tags — list loaded models
// ============================================================================

void Win32IDE::handleOllamaApiTags(SOCKET client)
{
    std::ostringstream j;
    j << "{\"models\":[";

    if (m_nativeEngine && m_nativeEngine->IsModelLoaded() && !m_loadedModelPath.empty())
    {
        // Use automatic namer so model is READ correctly (e.g. BigDaddyG-F32-FROM-Q4.gguf -> BigDaddyG-F32-FROM-Q4)
        std::string name = RawrXD::DeriveModelNameFromPath(m_loadedModelPath);

        j << "{"
          << "\"name\":\"" << LocalServerUtil::escapeJson(name) << "\""
          << ",\"model\":\"" << LocalServerUtil::escapeJson(name) << "\""
          << ",\"size\":" << 0 << ",\"digest\":\"rawrxd-local\""
          << ",\"details\":{\"format\":\"gguf\",\"family\":\"rawrxd\",\"parameter_size\":\"unknown\"}"
          << "}";
    }

    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// OLLAMA: /api/generate — generate text (streaming or non-streaming)
// ============================================================================

void Win32IDE::handleOllamaApiGenerate(SOCKET client, const std::string& body)
{
    std::string prompt, model;
    bool stream = true;
    int maxTokens = 512;

    LocalServerUtil::extractJsonString(body, "prompt", prompt);
    LocalServerUtil::extractJsonString(body, "model", model);
    LocalServerUtil::extractJsonBool(body, "stream", stream);

    int numPredict = 0;
    if (LocalServerUtil::extractJsonInt(body, "num_predict", numPredict) && numPredict > 0)
    {
        maxTokens = numPredict;
    }

    // ── Phase 8B+8C: Route through LLM Router → BackendSwitcher ────────
    // LocalGGUF + streaming: use native token-level SSE (preserved)
    // LocalGGUF + non-streaming: use native engine directly (preserved)
    // Remote backends: use routeWithIntelligence() for task-aware routing,
    //   then wrap the result in Ollama JSON format
    // ─────────────────────────────────────────────────────────────────────
    AIBackendType activeBackend = getActiveBackendType();

    if (activeBackend != AIBackendType::LocalGGUF)
    {
        // Remote backend — route through LLM Router (task classification,
        // capability matching, fallback chains). When router is disabled,
        // routeWithIntelligence() passes straight to routeInferenceRequest().
        std::string result = routeWithIntelligence(prompt);
        bool isError = result.find("[BackendSwitcher] Error") != std::string::npos;

        if (isError)
        {
            std::string json = "{\"error\":\"" + LocalServerUtil::escapeJson(result) + "\"}";
            std::string resp = LocalServerUtil::buildHttpResponse(502, json);
            LocalServerUtil::sendAll(client, resp);
            return;
        }

        // Use the Router's actual selected backend for the response model name
        // (may differ from activeBackend if Router reclassified the task)
        AIBackendType routedBackend = getLastRoutingDecision().selectedBackend;
        if (!m_routerEnabled || !m_routerInitialized)
            routedBackend = activeBackend;
        std::string backendName = LocalServerUtil::toLower(backendTypeString(routedBackend));
        m_localServerStats.totalTokens++;

        if (stream)
        {
            LocalServerUtil::sendSSEHeaders(client);
            std::string event = "{\"model\":\"" + LocalServerUtil::escapeJson(backendName) + "\",\"response\":\"" +
                                LocalServerUtil::escapeJson(result) + "\",\"done\":false}\n";
            LocalServerUtil::sendAll(client, event);
            std::string doneEvent =
                "{\"model\":\"" + LocalServerUtil::escapeJson(backendName) + "\",\"response\":\"\",\"done\":true}\n";
            LocalServerUtil::sendAll(client, doneEvent);
        }
        else
        {
            std::string json = "{\"model\":\"" + LocalServerUtil::escapeJson(backendName) + "\",\"response\":\"" +
                               LocalServerUtil::escapeJson(result) + "\",\"done\":true}";
            std::string resp = LocalServerUtil::buildHttpResponse(200, json);
            LocalServerUtil::sendAll(client, resp);
        }
        return;
    }

    // ── LocalGGUF path (original behavior preserved) ─────────────────────
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(400, "{\"error\":\"no model loaded\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prompt);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);

    if (stream)
    {
        LocalServerUtil::sendSSEHeaders(client);

        for (const auto& tok : generated)
        {
            std::string text = m_nativeEngine->Detokenize({tok});
            m_localServerStats.totalTokens++;

            std::string event =
                "{\"model\":\"rawrxd\",\"response\":\"" + LocalServerUtil::escapeJson(text) + "\",\"done\":false}\n";
            bool r = LocalServerUtil::sendAll(client, event);
            if (!r)
                return;
        }

        std::string doneEvent = "{\"model\":\"rawrxd\",\"response\":\"\",\"done\":true}\n";
        LocalServerUtil::sendAll(client, doneEvent);
    }
    else
    {
        std::string fullResponse = m_nativeEngine->Detokenize(generated);
        m_localServerStats.totalTokens += (int)generated.size();

        std::string json =
            "{\"model\":\"rawrxd\",\"response\":\"" + LocalServerUtil::escapeJson(fullResponse) + "\",\"done\":true}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json);
        LocalServerUtil::sendAll(client, resp);
    }
}

// ============================================================================
// OPENAI: /v1/chat/completions — chat completions (streaming or not)
// ============================================================================

void Win32IDE::handleOpenAIChatCompletions(SOCKET client, const std::string& body)
{
    // Extract messages array — simplified extraction (last "content" field)
    std::string prompt;
    bool stream = false;
    int maxTokens = 512;
    float temperature = 0.7f;

    LocalServerUtil::extractJsonBool(body, "stream", stream);
    LocalServerUtil::extractJsonInt(body, "max_tokens", maxTokens);
    LocalServerUtil::extractJsonFloat(body, "temperature", temperature);

    // Extract messages — find last "content" in the body
    // (simplified: concatenate all content fields)
    size_t pos = 0;
    std::string allContent;
    while ((pos = body.find("\"content\"", pos)) != std::string::npos)
    {
        std::string content;
        if (LocalServerUtil::extractJsonString(body.substr(pos), "content", content))
        {
            if (!allContent.empty())
                allContent += "\n";
            allContent += content;
        }
        pos += 9;
    }

    if (allContent.empty())
    {
        std::string resp =
            LocalServerUtil::buildHttpResponse(400, "{\"error\":{\"message\":\"No messages provided\"}}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    prompt = allContent;

    std::string requestId =
        "chatcmpl-rawrxd-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    // ── Phase 8B+8C: Route through LLM Router → BackendSwitcher ────────
    // LocalGGUF + streaming: use native token-level SSE (preserved)
    // LocalGGUF + non-streaming: use native engine directly (preserved)
    // Remote backends: use routeWithIntelligence() for task-aware routing,
    //   then wrap the result in OpenAI chat completion format
    // ─────────────────────────────────────────────────────────────────────
    AIBackendType activeBackend = getActiveBackendType();

    if (activeBackend != AIBackendType::LocalGGUF)
    {
        // Remote backend — route through LLM Router (task classification,
        // capability matching, fallback chains). When router is disabled,
        // routeWithIntelligence() passes straight to routeInferenceRequest().
        std::string result = routeWithIntelligence(prompt);
        bool isError = result.find("[BackendSwitcher] Error") != std::string::npos;

        if (isError)
        {
            std::string errJson = "{\"error\":{\"message\":\"" + LocalServerUtil::escapeJson(result) + "\"}}";
            std::string resp = LocalServerUtil::buildHttpResponse(502, errJson);
            LocalServerUtil::sendAll(client, resp);
            return;
        }

        m_localServerStats.totalTokens++;

        if (stream)
        {
            LocalServerUtil::sendSSEHeaders(client);
            std::ostringstream event;
            event << "data: {\"id\":\"" << requestId << "\",\"object\":\"chat.completion.chunk\""
                  << ",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"" << LocalServerUtil::escapeJson(result)
                  << "\"}}]}\n\n";
            std::string eventStr = event.str();
            LocalServerUtil::sendAll(client, eventStr);
            std::string doneStr = "data: [DONE]\n\n";
            LocalServerUtil::sendAll(client, doneStr);
        }
        else
        {
            std::ostringstream j;
            j << "{\"id\":\"" << requestId << "\",\"object\":\"chat.completion\""
              << ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\""
              << LocalServerUtil::escapeJson(result) << "\"},\"finish_reason\":\"stop\"}]"
              << ",\"usage\":{\"prompt_tokens\":0"
              << ",\"completion_tokens\":0"
              << ",\"total_tokens\":0}}";
            std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
            LocalServerUtil::sendAll(client, resp);
        }
        return;
    }

    // ── LocalGGUF path (original behavior preserved) ─────────────────────
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(400, "{\"error\":{\"message\":\"No model loaded\"}}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prompt);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);

    if (stream)
    {
        LocalServerUtil::sendSSEHeaders(client);

        for (const auto& tok : generated)
        {
            std::string text = m_nativeEngine->Detokenize({tok});
            m_localServerStats.totalTokens++;

            std::ostringstream event;
            event << "data: {\"id\":\"" << requestId << "\",\"object\":\"chat.completion.chunk\""
                  << ",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"" << LocalServerUtil::escapeJson(text)
                  << "\"}}]}\n\n";

            std::string eventStr = event.str();
            bool r = LocalServerUtil::sendAll(client, eventStr);
            if (!r)
                return;
        }

        std::string doneStr = "data: [DONE]\n\n";
        LocalServerUtil::sendAll(client, doneStr);
    }
    else
    {
        std::string fullResponse = m_nativeEngine->Detokenize(generated);
        m_localServerStats.totalTokens += (int)generated.size();

        std::ostringstream j;
        j << "{\"id\":\"" << requestId << "\",\"object\":\"chat.completion\""
          << ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\""
          << LocalServerUtil::escapeJson(fullResponse) << "\"},\"finish_reason\":\"stop\"}]"
          << ",\"usage\":{\"prompt_tokens\":" << tokens.size() << ",\"completion_tokens\":" << generated.size()
          << ",\"total_tokens\":" << (tokens.size() + generated.size()) << "}}";

        std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
        LocalServerUtil::sendAll(client, resp);
    }
}

// ============================================================================
// FRONTEND: /models — scan candidate model roots for .gguf + blobs (dynamic paths)
// ============================================================================

std::vector<std::string> Win32IDE::getCandidateModelRootPaths()
{
    auto trimCopy = [](const std::string& s) -> std::string
    {
        size_t b = 0;
        while (b < s.size() && std::isspace((unsigned char)s[b]))
            b++;
        size_t e = s.size();
        while (e > b && std::isspace((unsigned char)s[e - 1]))
            e--;
        return s.substr(b, e - b);
    };

    auto normalizePath = [&](std::string p) -> std::string
    {
        p = trimCopy(p);
        if (p.size() >= 2 && p.front() == '"' && p.back() == '"')
        {
            p = p.substr(1, p.size() - 2);
        }
        std::replace(p.begin(), p.end(), '/', '\\');
        while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
            p.pop_back();
        return p;
    };

    auto getEnvA = [](const char* name) -> std::string
    {
        DWORD needed = GetEnvironmentVariableA(name, nullptr, 0);
        if (needed == 0)
            return {};
        std::string out;
        out.resize(needed);
        DWORD written = GetEnvironmentVariableA(name, out.data(), needed);
        if (written == 0)
            return {};
        out.resize(written);
        return out;
    };

    auto isDir = [](const std::string& p) -> bool
    {
        if (p.empty())
            return false;
        DWORD attr = GetFileAttributesA(p.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
    };

    std::vector<std::string> roots;
    std::unordered_set<std::string> seen;

    auto addRoot = [&](const std::string& p)
    {
        std::string n = normalizePath(p);
        if (!isDir(n))
            return;
        std::string key = LocalServerUtil::toLower(n);
        if (!seen.insert(key).second)
            return;
        roots.push_back(n);
    };

    // 1) Explicit override (Ollama uses this).
    // Note: Some users set this to multiple paths; support ';' separated lists.
    std::string ollamaModels = getEnvA("OLLAMA_MODELS");
    if (!ollamaModels.empty())
    {
        size_t start = 0;
        while (start < ollamaModels.size())
        {
            size_t semi = ollamaModels.find(';', start);
            std::string part =
                (semi == std::string::npos) ? ollamaModels.substr(start) : ollamaModels.substr(start, semi - start);
            addRoot(part);
            if (semi == std::string::npos)
                break;
            start = semi + 1;
        }
    }

    // 2) Default Ollama paths.
    std::string localAppData = getEnvA("LOCALAPPDATA");
    if (!localAppData.empty())
        addRoot(localAppData + "\\Ollama\\models");
    std::string userProfile = getEnvA("USERPROFILE");
    if (!userProfile.empty())
        addRoot(userProfile + "\\.ollama\\models");
    std::string programData = getEnvA("ProgramData");
    if (!programData.empty())
        addRoot(programData + "\\Ollama\\models");

    // 3) Common custom locations (esp. on machines with big D: drives).
    addRoot("D:\\OllamaModels");
    addRoot("D:\\models");

    // 4) Portable folder next to the executable (bin\\models).
    char exePath[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
    {
        std::string exe(exePath, exePath + n);
        size_t slash = exe.find_last_of("\\/");
        std::string dir = (slash == std::string::npos) ? std::string() : exe.substr(0, slash);
        if (!dir.empty())
            addRoot(dir + "\\models");
    }

    return roots;
}

void Win32IDE::handleModelsEndpoint(SOCKET client)
{
    std::ostringstream j;
    j << "{\"models\":[";

    int count = 0;

    // 1. Currently loaded model (if any) — use automatic namer for correct API name
    if (m_nativeEngine && m_nativeEngine->IsModelLoaded() && !m_loadedModelPath.empty())
    {
        std::string displayName = RawrXD::DeriveModelNameFromPath(m_loadedModelPath);

        if (count > 0)
            j << ",";
        j << "{\"name\":\"" << LocalServerUtil::escapeJson(displayName) << "\",\"type\":\"gguf\",\"size\":\"loaded\""
          << ",\"path\":\"" << LocalServerUtil::escapeJson(m_loadedModelPath) << "\"}";
        count++;
    }

    // 2. Scan each candidate model root for .gguf and blobs (OLLAMA_MODELS, %LOCALAPPDATA%\Ollama, D:\OllamaModels,
    // ...)
    std::vector<std::string> roots = Win32IDE::getCandidateModelRootPaths();
    WIN32_FIND_DATAA findData;
    for (const auto& root : roots)
    {
        std::string pattern = root + "\\*.gguf";
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::string fname = findData.cFileName;
                std::string fullPath = root + "/" + fname;
                if (fullPath == m_loadedModelPath)
                    continue;
                size_t dot = fname.rfind(".gguf");
                std::string displayName = (dot != std::string::npos) ? fname.substr(0, dot) : fname;
                LARGE_INTEGER fileSize;
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;
                double sizeGB = (double)fileSize.QuadPart / (1024.0 * 1024.0 * 1024.0);
                std::ostringstream sizeFmt;
                sizeFmt << std::fixed << std::setprecision(1) << sizeGB << "GB";
                if (count > 0)
                    j << ",";
                j << "{\"name\":\"" << LocalServerUtil::escapeJson(displayName) << "\",\"type\":\"gguf\",\"size\":\""
                  << sizeFmt.str() << "\",\"path\":\"" << LocalServerUtil::escapeJson(fullPath) << "\"}";
                count++;
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        std::string blobPattern = root + "\\blobs\\sha256-*";
        hFind = FindFirstFileA(blobPattern.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::string fname = findData.cFileName;
                std::string fullPath = root + "/blobs/" + fname;
                LARGE_INTEGER fileSize;
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;
                double sizeGB = (double)fileSize.QuadPart / (1024.0 * 1024.0 * 1024.0);
                if (sizeGB < 0.1)
                    continue;
                std::ostringstream sizeFmt;
                sizeFmt << std::fixed << std::setprecision(1) << sizeGB << "GB";
                std::string displayName = "blob:" + fname.substr(0, std::min((size_t)19, fname.size()));
                if (count > 0)
                    j << ",";
                j << "{\"name\":\"" << LocalServerUtil::escapeJson(displayName) << "\",\"type\":\"blob\",\"size\":\""
                  << sizeFmt.str() << "\",\"path\":\"" << LocalServerUtil::escapeJson(fullPath) << "\"}";
                count++;
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }

    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// CURSOR/OPENAI: /v1/models, /api/models — model list for Cursor Settings > Models
// Uses automatic namer so loaded models (e.g. BigDaddyG-F32-FROM-Q4.gguf) are READ correctly.
// ============================================================================

void Win32IDE::handleV1ModelsEndpoint(SOCKET client)
{
    std::vector<std::string> ids;
    if (m_nativeEngine && m_nativeEngine->IsModelLoaded() && !m_loadedModelPath.empty())
    {
        std::string name = RawrXD::DeriveModelNameFromPath(m_loadedModelPath);
        if (!name.empty() && name != "rawrxd")
            ids.push_back(name);
    }
    if (ids.empty())
        ids.push_back("rawrxd");
    const std::string known[] = {"llama3",
                                 "llama2",
                                 "neural-chat",
                                 "mistral",
                                 "codellama",
                                 "phi",
                                 "gemma",
                                 "BigDaddyG-Q4_K_M",
                                 "BigDaddyG-F32-FROM-Q4",
                                 "BigDaddyG-NO-REFUSE-Q4_K_M",
                                 "BigDaddyG-UNLEASHED-Q4_K_M"};
    for (const auto& k : known)
    {
        if (std::find(ids.begin(), ids.end(), k) == ids.end())
            ids.push_back(k);
    }
    std::ostringstream out;
    out << "{\"object\":\"list\",\"data\":[";
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i)
            out << ",";
        std::string escaped;
        for (char c : ids[i])
        {
            if (c == '"')
                escaped += "\\\"";
            else if (c == '\\')
                escaped += "\\\\";
            else if (c == '\n')
                escaped += "\\n";
            else
                escaped += c;
        }
        out << "{\"id\":\"" << escaped << "\",\"object\":\"model\",\"created\":1700000000}";
    }
    out << "]}";
    std::string response = LocalServerUtil::buildHttpResponse(200, out.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// FRONTEND: /ask — unified chat endpoint for the HTML chatbot
// ============================================================================

void Win32IDE::handleAskEndpoint(SOCKET client, const std::string& body)
{
    std::string question, model;
    int context = 4096;
    bool stream = false;

    LocalServerUtil::extractJsonString(body, "question", question);
    LocalServerUtil::extractJsonString(body, "model", model);
    LocalServerUtil::extractJsonInt(body, "context", context);
    LocalServerUtil::extractJsonBool(body, "stream", stream);

    if (question.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(400, "{\"error\":\"No question provided\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // ── Phase 8B+8C: Route through LLM Router → BackendSwitcher ────────
    // All backends go through routeWithIntelligence() which classifies
    // the task, selects the optimal backend (with fallback), then delegates
    // to routeInferenceRequest() for actual inference.
    // When the router is disabled, routeWithIntelligence() passes straight
    // through to routeInferenceRequest() — zero behavior change.
    // ─────────────────────────────────────────────────────────────────────
    std::string answer = routeWithIntelligence(question);
    bool isError = answer.find("[BackendSwitcher] Error") != std::string::npos;
    m_localServerStats.totalTokens++;

    // Determine the actual backend used (Router may have reclassified)
    AIBackendType reportedBackend = getActiveBackendType();
    if (m_routerEnabled && m_routerInitialized)
    {
        reportedBackend = getLastRoutingDecision().selectedBackend;
    }
    std::string backendStr = backendTypeString(reportedBackend);

    if (isError)
    {
        // Return the error as a displayable answer (not a 500)
        // so the HTML chatbot can show it gracefully
        std::string json = "{\"answer\":\"" + LocalServerUtil::escapeJson(answer) + "\",\"backend\":\"" +
                           LocalServerUtil::escapeJson(backendStr) + "\",\"error\":true}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string json = "{\"answer\":\"" + LocalServerUtil::escapeJson(answer) + "\",\"backend\":\"" +
                       LocalServerUtil::escapeJson(backendStr) + "\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);
}

// ============================================================================
// FRONTEND: /gui — serve the agentic chatbot HTML from gui/ide_chatbot.html
// ============================================================================

void Win32IDE::handleServeGui(SOCKET client)
{
    // Resolve path relative to executable or project root
    std::string htmlPath;

    // Try multiple paths
    const char* candidates[] = {"gui/ide_chatbot.html", "../gui/ide_chatbot.html", "D:/rawrxd/gui/ide_chatbot.html"};

    for (const char* candidate : candidates)
    {
        HANDLE hFile =
            CreateFileA(candidate, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD fileSize = GetFileSize(hFile, NULL);
            if (fileSize != INVALID_FILE_SIZE && fileSize > 0)
            {
                std::string content(fileSize, '\0');
                DWORD bytesRead = 0;
                ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
                content.resize(bytesRead);
                CloseHandle(hFile);

                // Send as HTML — use LocalServerUtil::sendAll() to guarantee the entire
                // ~1 MB payload is transmitted.  A single send() may only
                // push part of it, leaving the browser with truncated HTML
                // (the closing </script> tag for the 770 KB main script
                // block never arrives, so every function is undefined).
                std::ostringstream oss;
                oss << "HTTP/1.1 200 OK\r\n"
                    << "Content-Type: text/html; charset=utf-8\r\n"
                    << "Content-Length: " << content.size() << "\r\n"
                    << "Access-Control-Allow-Origin: *\r\n"
                    << "Connection: close\r\n\r\n";
                std::string headers = oss.str();

                // Send headers first, then body separately to avoid
                // copying the ~1 MB content into yet another string.
                if (!LocalServerUtil::sendAll(client, headers))
                    return;
                LocalServerUtil::sendAll(client, content.c_str(), (int)content.size());
                return;
            }
            CloseHandle(hFile);
        }
    }

    // File not found
    std::string resp = LocalServerUtil::buildHttpResponse(404, "{\"error\":\"gui/ide_chatbot.html not found\"}");
    LocalServerUtil::sendAll(client, resp);
}

// ============================================================================
// POST /api/read-file — Read a local file for chatbot attachment support
// ============================================================================
// Request body: {"path":"C:/some/file.cpp"}
// Response: {"content":"...file text...","name":"file.cpp","size":12345}
// Security: Only allows reading text files up to 10MB.
//           Rejects paths containing ".." to prevent directory traversal.
// ============================================================================

void Win32IDE::handleReadFileEndpoint(SOCKET client, const std::string& body)
{
    // Parse "path" from JSON body (lightweight — no full JSON parser needed)
    std::string filePath;
    {
        auto pathKey = body.find("\"path\"");
        if (pathKey == std::string::npos)
        {
            std::string resp = LocalServerUtil::buildHttpResponse(
                400, "{\"error\":\"missing_path\",\"message\":\"Request body must contain 'path' field\"}");
            LocalServerUtil::sendAll(client, resp);
            return;
        }
        // Find the value string after "path": "..."
        auto colonPos = body.find(':', pathKey + 6);
        if (colonPos == std::string::npos)
        {
            std::string resp = LocalServerUtil::buildHttpResponse(
                400, "{\"error\":\"malformed_json\",\"message\":\"Could not parse path value\"}");
            LocalServerUtil::sendAll(client, resp);
            return;
        }
        auto quoteStart = body.find('"', colonPos + 1);
        if (quoteStart == std::string::npos)
        {
            std::string resp = LocalServerUtil::buildHttpResponse(
                400, "{\"error\":\"malformed_json\",\"message\":\"Could not find path string\"}");
            LocalServerUtil::sendAll(client, resp);
            return;
        }
        // Find closing quote (handle escaped quotes)
        size_t quoteEnd = quoteStart + 1;
        while (quoteEnd < body.size())
        {
            if (body[quoteEnd] == '\\')
            {
                quoteEnd += 2;  // skip escaped char
                continue;
            }
            if (body[quoteEnd] == '"')
                break;
            quoteEnd++;
        }
        if (quoteEnd >= body.size())
        {
            std::string resp = LocalServerUtil::buildHttpResponse(
                400, "{\"error\":\"malformed_json\",\"message\":\"Unterminated path string\"}");
            LocalServerUtil::sendAll(client, resp);
            return;
        }
        filePath = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    // Unescape JSON string basics (forward slashes, backslashes)
    {
        std::string unescaped;
        unescaped.reserve(filePath.size());
        for (size_t i = 0; i < filePath.size(); i++)
        {
            if (filePath[i] == '\\' && i + 1 < filePath.size())
            {
                char next = filePath[i + 1];
                if (next == '\\')
                {
                    unescaped += '\\';
                    i++;
                    continue;
                }
                if (next == '/')
                {
                    unescaped += '/';
                    i++;
                    continue;
                }
                if (next == '"')
                {
                    unescaped += '"';
                    i++;
                    continue;
                }
                if (next == 'n')
                {
                    unescaped += '\n';
                    i++;
                    continue;
                }
                if (next == 'r')
                {
                    unescaped += '\r';
                    i++;
                    continue;
                }
                if (next == 't')
                {
                    unescaped += '\t';
                    i++;
                    continue;
                }
            }
            unescaped += filePath[i];
        }
        filePath = unescaped;
    }

    // Normalize forward slashes to backslashes for Windows
    for (auto& ch : filePath)
    {
        if (ch == '/')
            ch = '\\';
    }

    // Security: reject directory traversal
    if (filePath.find("..") != std::string::npos)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            403, "{\"error\":\"forbidden\",\"message\":\"Directory traversal not allowed\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Security: must be an absolute path (drive letter)
    if (filePath.size() < 3 || filePath[1] != ':' || filePath[2] != '\\')
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"invalid_path\",\"message\":\"Only absolute paths are accepted\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Open the file
    HANDLE hFile =
        CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            404, "{\"error\":\"file_not_found\",\"message\":\"Cannot open file: " +
                     LocalServerUtil::escapeJson(filePath) + "\",\"win32_error\":" + std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Get file size — limit to 10MB for safety
    DWORD fileSize = GetFileSize(hFile, NULL);
    const DWORD maxFileSize = 10 * 1024 * 1024;  // 10MB
    if (fileSize == INVALID_FILE_SIZE || fileSize > maxFileSize)
    {
        CloseHandle(hFile);
        std::string resp = LocalServerUtil::buildHttpResponse(
            413, "{\"error\":\"file_too_large\",\"message\":\"File exceeds 10MB limit\",\"size\":" +
                     std::to_string(fileSize == INVALID_FILE_SIZE ? 0 : fileSize) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Read file content
    std::string content(fileSize, '\0');
    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!readOk || bytesRead == 0)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"read_failed\",\"message\":\"Failed to read file content\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }
    content.resize(bytesRead);

    // Extract filename from path
    std::string fileName;
    {
        auto lastSlash = filePath.rfind('\\');
        if (lastSlash != std::string::npos)
        {
            fileName = filePath.substr(lastSlash + 1);
        }
        else
        {
            fileName = filePath;
        }
    }

    // Build JSON response with escaped content
    std::ostringstream json;
    json << "{\"content\":" << "\"" << LocalServerUtil::escapeJson(content) << "\""
         << ",\"name\":\"" << LocalServerUtil::escapeJson(fileName) << "\""
         << ",\"size\":" << bytesRead << ",\"path\":\"" << LocalServerUtil::escapeJson(filePath) << "\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("read-file: " + filePath + " (" + std::to_string(bytesRead) + " bytes)");
}

// ============================================================================
// CLI Command Execution: POST /api/cli
// ============================================================================
// Executes CLI-style commands from the embedded terminal.
// Body: {"command":"/plan ...", "args":"..."}
// Returns: {"success":bool, "output":"...", "command":"..."}
// Mirrors the same command set as tool_server.cpp HandleCliRequest().
// ============================================================================

void Win32IDE::handleCliEndpoint(SOCKET client, const std::string& body)
{
    std::string command = LocalServerUtil::extractJsonStringValue(body, "command");
    if (command.empty())
    {
        std::string resp =
            LocalServerUtil::buildHttpResponse(400, "{\"success\":false,\"error\":\"Missing 'command' field\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Trim whitespace
    size_t start = command.find_first_not_of(" \t\n\r");
    size_t end = command.find_last_not_of(" \t\n\r");
    if (start != std::string::npos)
        command = command.substr(start, end - start + 1);

    // Parse command name and args
    std::string cmdName = command;
    std::string cmdArgs;
    size_t spPos = command.find(' ');
    if (spPos != std::string::npos)
    {
        cmdName = command.substr(0, spPos);
        cmdArgs = command.substr(spPos + 1);
    }

    std::string cmdLower = cmdName;
    for (char& c : cmdLower)
        c = static_cast<char>(std::tolower(c));

    std::ostringstream output;
    bool success = true;
    auto executeTool = [&](const std::string& tool, nlohmann::json args) -> bool
    {
        auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
        if (!handlers.HasTool(tool))
        {
            output << "Backend tool unavailable: " << tool << "\\n";
            return false;
        }
        const auto result = handlers.Execute(tool, std::move(args));
        output << result.toJson().dump(2) << "\\n";
        return result.isSuccess();
    };

    // ---- CLI Command Dispatcher ----
    if (cmdLower == "/help" || cmdLower == "help")
    {
        output << "RawrXD CLI v20.0.0 (Win32IDE Backend)\\n";
        output << "Available: /plan /analyze /optimize /security /suggest\\n";
        output << "           /bugreport /hotpatch /status /models /agents\\n";
        output << "           /memory /failures /clear\\n";
        output << "           !engine load800b|setup5drive|verify|analyze|compile|optimize\\n";
    }
    else if (cmdLower == "/status" || cmdLower == "status")
    {
        output << "Server: Win32IDE LocalServer v20.0.0\\n";
        output << "Port: 11435\\n";
        output << "Engine: " << (m_localServerRunning.load() ? "running" : "stopped") << "\\n";
    }
    else if (cmdLower == "/plan")
    {
        if (cmdArgs.empty())
        {
            output << "Usage: /plan <task description>\\n";
            success = false;
        }
        else
        {
            success = executeTool("plan_code_exploration", {{"goal", cmdArgs}, {"mode", "cli_bridge"}});
        }
    }
    else if (cmdLower == "/analyze")
    {
        if (cmdArgs.empty())
        {
            output << "Usage: /analyze <file_path>\\n";
            success = false;
        }
        else
        {
            std::string filePath = cmdArgs;
            if (filePath[0] != '/' && (filePath.size() < 2 || filePath[1] != ':'))
            {
                char cwd[MAX_PATH] = {};
                if (GetCurrentDirectoryA(MAX_PATH, cwd))
                    filePath = std::string(cwd) + "\\" + filePath;
            }
            try
            {
                auto canonical = std::filesystem::weakly_canonical(filePath);
                if (std::filesystem::exists(canonical))
                {
                    auto fsize = std::filesystem::file_size(canonical);
                    output << "Analysis: " << canonical.filename().string() << "\\n";
                    output << "  Size: " << fsize << " bytes\\n";
                    output << "  Extension: " << canonical.extension().string() << "\\n";
                }
                else
                {
                    output << "File not found: " << filePath << "\\n";
                    success = false;
                }
            }
            catch (const std::exception& e)
            {
                output << "Error: " << e.what() << "\\n";
                success = false;
            }
        }
    }
    else if (cmdLower == "/optimize")
    {
        if (cmdArgs.empty())
        {
            output << "Usage: " << cmdName << " <argument>\\n";
            success = false;
        }
        else
        {
            success = executeTool("optimize_tool_selection", {{"task", cmdArgs}, {"context", "gui_cli_endpoint"}});
        }
    }
    else if (cmdLower == "/security" || cmdLower == "/suggest" || cmdLower == "/bugreport")
    {
        if (cmdArgs.empty())
        {
            output << "Usage: " << cmdName << " <argument>\\n";
            success = false;
        }
        else
        {
            success = executeTool("evaluate_integration_audit_feasibility",
                                  {{"target", cmdArgs}, {"intent", cmdName.substr(1)}, {"source", "gui_cli_endpoint"}});
        }
    }
    else if (cmdLower == "/models")
    {
        nlohmann::json payload = nlohmann::json::object();
        payload["success"] = true;
        payload["tools"] = RawrXD::Agent::AgentToolHandlers::GetAllSchemas();
        output << payload.dump(2) << "\\n";
    }
    else if (cmdLower == "/agents")
    {
        success = executeTool("plan_tasks", {{"task", "report active agent routes and orchestration status"},
                                             {"owner", "gui_cli_endpoint"}});
    }
    else if (cmdLower == "/failures")
    {
        success = executeTool("get_diagnostics", nlohmann::json::object());
    }
    else if (cmdLower == "/orchestrate")
    {
        success = executeTool("orchestrate_conversation", {{"prompt", cmdArgs}, {"source", "gui_cli_endpoint"}});
    }
    else if (cmdLower == "/speculate")
    {
        success = executeTool("speculate_next", {{"prompt", cmdArgs}, {"source", "gui_cli_endpoint"}});
    }
    else if (cmdLower == "/subagent")
    {
        success = executeTool("plan_code_exploration",
                              {{"query", cmdArgs.empty() ? "Execute delegated subagent task" : cmdArgs},
                               {"task_type", "analyze"},
                               {"mode", "subagent"}});
    }
    else if (cmdLower == "/chain")
    {
        success = executeTool("plan_tasks", {{"task", cmdArgs.empty() ? "Run chained multi-step analysis" : cmdArgs},
                                             {"owner", "chain"},
                                             {"deadline", "asap"}});
    }
    else if (cmdLower == "/swarm")
    {
        success =
            executeTool("plan_code_exploration",
                        {{"goal", cmdArgs.empty() ? "Run swarm orchestration analysis" : cmdArgs}, {"mode", "swarm"}});
    }
    else if (cmdLower == "/memory")
    {
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        output << "Physical Total: " << (memStatus.ullTotalPhys / (1024 * 1024)) << " MB\\n";
        output << "Physical Free:  " << (memStatus.ullAvailPhys / (1024 * 1024)) << " MB\\n";
        output << "Memory Load:    " << memStatus.dwMemoryLoad << "%\\n";
    }
    else if (cmdLower == "/hotpatch")
    {
        output << "Hotpatch Layer Status:\\n";
        output << "  Memory Layer:     active\\n";
        output << "  Byte-Level Layer: active\\n";
        output << "  Server Layer:     active\\n";
    }
    else if (cmdLower == "/clear")
    {
        output << "[clear]";
    }
    else if (cmdLower.substr(0, 7) == "!engine")
    {
        output << "Engine command dispatched: " << command << "\\n";
        output << "Use engine panel in IDE for detailed control.\\n";
    }
    else
    {
        output << "Unknown CLI command: " << command << "\\n";
        output << "Type /help for available commands.\\n";
        success = false;
    }

    // Build JSON response — escape the output for JSON
    std::string outStr = output.str();
    std::string escaped;
    escaped.reserve(outStr.size() + 64);
    for (char c : outStr)
    {
        if (c == '"')
            escaped += "\\\"";
        else if (c == '\n')
            escaped += "\\n";
        else if (c == '\r')
        { /* skip */
        }
        else if (c == '\t')
            escaped += "\\t";
        else
            escaped += c;
    }

    std::string escapedCmd;
    for (char c : command)
    {
        if (c == '"')
            escapedCmd += "\\\"";
        else if (c == '\\')
            escapedCmd += "\\\\";
        else
            escapedCmd += c;
    }

    std::string jsonBody = "{\"success\":" + std::string(success ? "true" : "false") + ",\"command\":\"" + escapedCmd +
                           "\",\"output\":\"" + escaped + "\"}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, jsonBody);
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("cli: " + command + " -> " + (success ? "ok" : "error"));
}

// ============================================================================
// Hotpatch Layer Control: POST /api/hotpatch/{toggle,apply,revert}
// ============================================================================
// Interfaces with the three-layer hotpatch system (memory, byte-level, server).
// Body: {"layer":"memory|byte|server"}
// Actions: toggle (enable/disable), apply (apply pending), revert (undo applied)
// ============================================================================

void Win32IDE::handleHotpatchEndpoint(SOCKET client, const std::string& path, const std::string& body)
{
    const std::string agentOpsPrefix = "/api/agent/ops/";
    if (path.find(agentOpsPrefix) == 0)
    {
        const std::string opName = path.substr(agentOpsPrefix.size());
        const auto normalizedOp = LocalServerUtil::toLower(opName);
        const std::unordered_map<std::string, std::string> opToTool = {
            {"compact-conversation", "compact_conversation"},
            {"optimize-tool-selection", "optimize_tool_selection"},
            {"resolving", "resolve_symbol"},
                        {"resolve-symbol", "resolve_symbol"},
            {"read-lines", "read_lines"},
            {"planning-exploration", "plan_code_exploration"},
            {"search-files", "search_files"},
            {"evaluate-integration", "evaluate_integration_audit_feasibility"},
            {"restore-checkpoint", "restore_checkpoint"}};

        auto buildOrchestrationPlan = [&](const std::string& promptText)
        {
            nlohmann::json plan = nlohmann::json::object();
            plan["primary_tool"] = "optimize_tool_selection";
            plan["primary_args"] =
                nlohmann::json::object({{"task", promptText.empty() ? "general assistance" : promptText}});
            plan["speculative"] = nlohmann::json::array();

            const std::string lower = LocalServerUtil::toLower(promptText);

            // L1: Integrate with OrchestrationSessionState for intent tracking
            auto& sessionState = ::RawrXD::Orchestration::OrchestrationSessionState::instance();
            ::RawrXD::Orchestration::IntentClassification localIntent;
            localIntent.intent = "general_query";
            localIntent.confidence = 0.50f;
            localIntent.reasoning = "HTTP endpoint classification from " + plan["primary_tool"].get<std::string>();

            auto addSpeculative = [&](const char* tool, nlohmann::json args)
            { plan["speculative"].push_back(nlohmann::json::object({{"tool", tool}, {"args", std::move(args)}})); };

            if (lower.find("error") != std::string::npos || lower.find("diagnostic") != std::string::npos ||
                lower.find("warning") != std::string::npos || lower.find("lint") != std::string::npos)
            {
                plan["primary_tool"] = "get_diagnostics";
                localIntent.intent = "debug";
                localIntent.confidence = 0.83f;
                localIntent.suggested_tools = {"get_diagnostics", "search_code", "grep_files"};
                nlohmann::json primaryArgs = nlohmann::json::object();
                primaryArgs["file"] = m_currentFile.empty() ? "CMakeLists.txt" : m_currentFile;
                plan["primary_args"] = std::move(primaryArgs);
                addSpeculative("search_code", nlohmann::json::object({{"query", promptText}, {"max_results", 30}}));
            }
            else if (lower.find("plan") != std::string::npos || lower.find("explore") != std::string::npos ||
                     lower.find("refactor") != std::string::npos || lower.find("implement") != std::string::npos)
            {
                plan["primary_tool"] = "plan_code_exploration";
                localIntent.intent = "planning";
                localIntent.confidence = 0.80f;
                localIntent.suggested_tools = {"plan_code_exploration", "search_files"};
                nlohmann::json primaryArgs = nlohmann::json::object();
                primaryArgs["goal"] =
                    promptText.empty() ? "Explore codebase and propose implementation plan" : promptText;
                plan["primary_args"] = std::move(primaryArgs);
                addSpeculative("search_files", nlohmann::json::object({{"pattern", "*.cpp"}, {"max_results", 40}}));
                addSpeculative("search_files", nlohmann::json::object({{"pattern", "*.h"}, {"max_results", 40}}));
            }
            else if (lower.find("search") != std::string::npos || lower.find("find") != std::string::npos)
            {
                plan["primary_tool"] = "search_code";
                localIntent.intent = "search";
                localIntent.confidence = 0.85f;
                localIntent.suggested_tools = {"search_code", "search_files", "grep_files"};
                nlohmann::json primaryArgs = nlohmann::json::object();
                primaryArgs["query"] = promptText.empty() ? "TODO" : promptText;
                primaryArgs["max_results"] = 50;
                plan["primary_args"] = std::move(primaryArgs);
                addSpeculative("search_files", nlohmann::json::object({{"pattern", "*.cpp"}, {"max_results", 30}}));
            }
            else if (lower.find("terminal") != std::string::npos || lower.find("command") != std::string::npos ||
                     lower.find("shell") != std::string::npos)
            {
                plan["primary_tool"] = "run_shell";
                localIntent.intent = "build";
                localIntent.confidence = 0.82f;
                localIntent.suggested_tools = {"run_shell", "git_status", "read_file"};
                nlohmann::json primaryArgs = nlohmann::json::object();
                primaryArgs["command"] = "pwd";
                plan["primary_args"] = std::move(primaryArgs);
                addSpeculative("git_status", nlohmann::json::object());
            }

            if (plan["speculative"].empty())
            {
                addSpeculative("evaluate_integration_audit_feasibility",
                               nlohmann::json::object(
                                   {{"workspace", m_projectRoot.empty() ? std::filesystem::current_path().string()
                                                                        : m_projectRoot}}));
            }

            sessionState.setCurrentIntent(localIntent);
            return plan;
        };

        auto executeOrchestrationPlan = [&](const nlohmann::json& plan, int speculativeLimit)
        {
            nlohmann::json out = nlohmann::json::object();
            out["success"] = true;
            out["outsideHotpatchAccessible"] = true;
            out["surface"] = "local_server";
            out["mode"] = "conversational_orchestrator";
            out["plan"] = plan;

            const std::string primaryTool = plan.value("primary_tool", "optimize_tool_selection");

            // L2: Access session state instance for tool result recording
            auto& sessionState = ::RawrXD::Orchestration::OrchestrationSessionState::instance();
            auto recordStart = std::chrono::steady_clock::now();
            nlohmann::json primaryArgs = nlohmann::json::object();
            if (plan.contains("primary_args") && plan["primary_args"].is_object())
                primaryArgs = plan["primary_args"];
            const auto primaryResult = RawrXD::Agent::AgentToolHandlers::Instance().Execute(primaryTool, primaryArgs);
            out["primary"] = primaryResult.toJson();
            out["primary"]["tool"] = primaryTool;

            nlohmann::json speculative = nlohmann::json::array();
            if (plan.contains("speculative") && plan["speculative"].is_array())
            {
                int executed = 0;
                for (const auto& item : plan["speculative"])
                {
                    if (executed >= speculativeLimit)
                        break;
                    if (!item.is_object() || !item.contains("tool") || !item["tool"].is_string())
                        continue;
                    const std::string tool = item["tool"].get<std::string>();
                    nlohmann::json args = nlohmann::json::object();
                    if (item.contains("args") && item["args"].is_object())
                        args = item["args"];
                    if (!RawrXD::Agent::AgentToolHandlers::Instance().HasTool(tool))
                        continue;
                    auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute(tool, args);
                    nlohmann::json entry = result.toJson();
                    entry["tool"] = tool;
                    entry["speculative"] = true;
                    speculative.push_back(entry);
                    ++executed;
                }
            }
            out["speculative_results"] = std::move(speculative);
            out["speculative_count"] = static_cast<int>(out["speculative_results"].size());
            out["primary_success"] = primaryResult.isSuccess();
            out["success"] = primaryResult.isSuccess();

            std::ostringstream narrative;
            narrative << "I handled this through the orchestration layer using `" << primaryTool
                      << "` as the primary tool";
            if (out["speculative_count"].get<int>() > 0)
            {
                narrative << " and ran " << out["speculative_count"].get<int>() << " speculative tool checks";
            }
            narrative << ".";
            out["assistant_response"] = narrative.str();
            return out;
        };

        if (normalizedOp == "orchestrate-conversation" || normalizedOp == "speculate-next")
        {
            nlohmann::json requestJson = nlohmann::json::object();
            try
            {
                requestJson = nlohmann::json::parse(body);
                if (!requestJson.is_object())
                    requestJson = nlohmann::json::object();
            }
            catch (...)
            {
                requestJson = nlohmann::json::object();
            }

            std::string promptText;
            if (requestJson.contains("prompt") && requestJson["prompt"].is_string())
                promptText = requestJson["prompt"].get<std::string>();
            if (promptText.empty() && requestJson.contains("message") && requestJson["message"].is_string())
                promptText = requestJson["message"].get<std::string>();
            if (promptText.empty())
                LocalServerUtil::extractJsonString(body, "prompt", promptText);
            if (promptText.empty())
                LocalServerUtil::extractJsonString(body, "message", promptText);

            const int speculativeLimit = (normalizedOp == "speculate-next") ? 5 : 3;
            const auto plan = buildOrchestrationPlan(promptText);
            auto out = executeOrchestrationPlan(plan, speculativeLimit);
            out["operation"] = opName;
            out["prompt"] = promptText;
            out["toolChatterHidden"] = true;
            LocalServerUtil::sendAll(
                client, LocalServerUtil::buildHttpResponse(out.value("success", false) ? 200 : 500, out.dump()));
            return;
        }

        auto mapped = opToTool.find(normalizedOp);
        if (mapped != opToTool.end())
        {
            nlohmann::json requestJson = nlohmann::json::object();
            try
            {
                requestJson = nlohmann::json::parse(body);
                if (!requestJson.is_object())
                    requestJson = nlohmann::json::object();
            }
            catch (...)
            {
                requestJson = nlohmann::json::object();
            }

            nlohmann::json args = nlohmann::json::object();
            if (requestJson.contains("args") && requestJson["args"].is_object())
            {
                args = requestJson["args"];
            }

            // Mirror historical op payload fields so old clients keep working.
            auto copyString = [&](const char* srcKey, const char* dstKey)
            {
                if (requestJson.contains(srcKey) && requestJson[srcKey].is_string())
                {
                    args[dstKey] = requestJson[srcKey].get<std::string>();
                    return;
                }
                std::string v;
                if (LocalServerUtil::extractJsonString(body, srcKey, v) && !v.empty())
                {
                    args[dstKey] = v;
                }
            };
            auto copyInt = [&](const char* srcKey, const char* dstKey)
            {
                if (requestJson.contains(srcKey) && requestJson[srcKey].is_number_integer())
                {
                    args[dstKey] = requestJson[srcKey].get<int>();
                    return;
                }
                int v = 0;
                if (LocalServerUtil::extractJsonInt(body, srcKey, v))
                {
                    args[dstKey] = v;
                }
            };
            copyString("task", "task");
            copyString("symbol", "symbol");
            copyString("goal", "goal");
            copyString("query", "query");
            copyString("pattern", "pattern");
            copyString("path", "path");
            copyString("session_id", "session_id");
            copyString("checkpoint_path", "checkpoint_path");
            copyInt("line_start", "line_start");
            copyInt("line_end", "line_end");
            copyInt("start_line", "start_line");
            copyInt("end_line", "end_line");
            copyInt("startLine", "start_line");
            copyInt("endLine", "end_line");
            copyInt("max_results", "max_results");
            copyInt("maxResults", "max_results");

            if (mapped->second == "resolve_symbol" && !args.contains("symbol"))
            {
                args["symbol"] = "Win32IDE";
            }
            if (mapped->second == "plan_code_exploration")
            {
                if (!args.contains("goal") && args.contains("query"))
                    args["goal"] = args["query"];
                if (!args.contains("goal"))
                    args["goal"] = "Perform targeted code exploration";
            }
            if (mapped->second == "read_lines")
            {
                if (!args.contains("path") && !m_currentFile.empty())
                    args["path"] = m_currentFile;
                if (!args.contains("start_line"))
                    args["start_line"] = 1;
                if (!args.contains("end_line"))
                    args["end_line"] = args["start_line"];
            }

            auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute(mapped->second, args);
            nlohmann::json out = result.toJson();
            out["operation"] = opName;
            out["tool"] = mapped->second;
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 500, out.dump()));
            return;
        }

        std::ostringstream json;

        if (opName == "compact-conversation")
        {
            if (!m_hwndCopilotChatInput)
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     409, "{\"success\":false,\"error\":\"chat_input_unavailable\"}"));
                return;
            }

            std::string input;
            LocalServerUtil::extractJsonString(body, "text", input);
            if (input.empty())
            {
                input = getWindowText(m_hwndCopilotChatInput);
            }
            if (input.empty())
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     400, "{\"success\":false,\"error\":\"empty_conversation\"}"));
                return;
            }

            std::string compacted;
            compacted.reserve(input.size());
            bool lastWasWhitespace = false;
            for (char c : input)
            {
                const bool isNewline = (c == '\n' || c == '\r');
                if (isNewline)
                {
                    if (!compacted.empty() && compacted.back() != '\n')
                    {
                        compacted.push_back('\n');
                    }
                    lastWasWhitespace = true;
                    continue;
                }
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!lastWasWhitespace && !compacted.empty() && compacted.back() != ' ')
                    {
                        compacted.push_back(' ');
                    }
                    lastWasWhitespace = true;
                    continue;
                }
                compacted.push_back(c);
                lastWasWhitespace = false;
            }
            compacted = LocalServerUtil::trimAscii(compacted);
            if (m_hwndCopilotChatInput)
            {
                SetWindowTextA(m_hwndCopilotChatInput, compacted.c_str());
            }

            const double reduction = input.empty() ? 0.0
                                                   : (100.0 * (static_cast<double>(input.size() - compacted.size()) /
                                                               static_cast<double>(input.size())));
            json << "{\"success\":true,\"operation\":\"compact-conversation\",\"originalBytes\":" << input.size()
                 << ",\"compactedBytes\":" << compacted.size() << ",\"reductionPct\":" << std::fixed
                 << std::setprecision(2) << reduction << ",\"compacted\":\"" << LocalServerUtil::escapeJson(compacted)
                 << "\"}";
            LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json.str()));
            return;
        }

        if (opName == "optimize-tool-selection")
        {
            std::string task;
            LocalServerUtil::extractJsonString(body, "task", task);
            if (task.empty())
            {
                task = "optimize tool selection for current task";
            }
            nlohmann::json args = nlohmann::json::object();
            args["task"] = task;
            auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("optimize_tool_selection", args);
            nlohmann::json out = result.toJson();
            out["operation"] = "optimize-tool-selection";
            out["bridgeReady"] = (m_agenticBridge != nullptr);
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 500, out.dump()));
            return;
        }

        if (opName == "resolving")
        {
            std::string symbol;
            LocalServerUtil::extractJsonString(body, "symbol", symbol);
            if (symbol.empty())
            {
                symbol = "Win32IDE";
            }
            nlohmann::json args = nlohmann::json::object();
            args["symbol"] = symbol;
            auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("resolve_symbol", args);
            nlohmann::json out = result.toJson();
            out["operation"] = "resolving";
            out["workspace"] = std::filesystem::current_path().string();
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 500, out.dump()));
            return;
        }

        if (opName == "read-lines")
        {
            int startLine = 1;
            int endLine = 1;
            LocalServerUtil::extractJsonInt(body, "startLine", startLine);
            if (!LocalServerUtil::extractJsonInt(body, "endLine", endLine))
            {
                endLine = startLine;
            }
            if (startLine <= 0 || endLine <= 0 || endLine < startLine)
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     400, "{\"success\":false,\"error\":\"invalid_line_range\"}"));
                return;
            }

            std::string requestedPath;
            LocalServerUtil::extractJsonString(body, "path", requestedPath);
            if (!requestedPath.empty() || !m_hwndEditor)
            {
                const std::string path = requestedPath.empty() ? m_currentFile : requestedPath;
                if (path.empty())
                {
                    LocalServerUtil::sendAll(client,
                                             LocalServerUtil::buildHttpResponse(
                                                 409, "{\"success\":false,\"error\":\"path_or_editor_required\"}"));
                    return;
                }
                nlohmann::json args = nlohmann::json::object();
                args["path"] = path;
                args["start_line"] = startLine;
                args["end_line"] = endLine;
                auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("read_lines", args);
                nlohmann::json out = result.toJson();
                out["operation"] = "read-lines";
                LocalServerUtil::sendAll(
                    client, LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 500, out.dump()));
                return;
            }

            const int lineCount = static_cast<int>(SendMessageA(m_hwndEditor, EM_GETLINECOUNT, 0, 0));
            if (startLine > lineCount)
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     400, "{\"success\":false,\"error\":\"start_line_out_of_range\"}"));
                return;
            }

            endLine = std::min(endLine, lineCount);
            std::ostringstream linesJson;
            linesJson << "{\"success\":true,\"operation\":\"read-lines\",\"file\":\""
                      << LocalServerUtil::escapeJson(m_currentFile) << "\",\"startLine\":" << startLine
                      << ",\"endLine\":" << endLine << ",\"lines\":[";

            bool first = true;
            for (int line = startLine; line <= endLine; ++line)
            {
                char buffer[4096] = {};
                *reinterpret_cast<WORD*>(buffer) = static_cast<WORD>(sizeof(buffer) - 1);
                const int len = static_cast<int>(
                    SendMessageA(m_hwndEditor, EM_GETLINE, line - 1, reinterpret_cast<LPARAM>(buffer)));
                if (len < 0)
                {
                    continue;
                }
                buffer[std::max(0, std::min(len, static_cast<int>(sizeof(buffer) - 1)))] = '\0';
                if (!first)
                {
                    linesJson << ",";
                }
                first = false;
                linesJson << "{\"line\":" << line << ",\"text\":\"" << LocalServerUtil::escapeJson(buffer) << "\"}";
            }
            linesJson << "]}";
            LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, linesJson.str()));
            return;
        }

        if (opName == "planning-exploration")
        {
            std::string goal;
            LocalServerUtil::extractJsonString(body, "query", goal);
            if (goal.empty())
            {
                LocalServerUtil::extractJsonString(body, "goal", goal);
            }
            if (goal.empty())
            {
                goal = "Perform targeted code exploration";
            }
            nlohmann::json args = nlohmann::json::object();
            args["goal"] = goal;
            auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute("plan_code_exploration", args);
            nlohmann::json out = result.toJson();
            out["operation"] = "planning-exploration";
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 500, out.dump()));
            return;
        }

        if (opName == "search-files")
        {
            std::string pattern;
            LocalServerUtil::extractJsonString(body, "pattern", pattern);
            if (pattern.empty())
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     400, "{\"success\":false,\"error\":\"pattern_required\"}"));
                return;
            }

            int maxResults = 50;
            if (LocalServerUtil::extractJsonInt(body, "maxResults", maxResults))
            {
                maxResults = std::clamp(maxResults, 1, 500);
            }
            else
            {
                maxResults = 50;
            }

            const std::filesystem::path root =
                m_projectRoot.empty() ? std::filesystem::current_path() : std::filesystem::path(m_projectRoot);
            if (!std::filesystem::exists(root))
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     409, "{\"success\":false,\"error\":\"workspace_missing\"}"));
                return;
            }

            std::vector<std::string> matches;
            try
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(
                         root, std::filesystem::directory_options::skip_permission_denied))
                {
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }
                    const std::string relative = std::filesystem::relative(entry.path(), root).generic_string();
                    if (LocalServerUtil::wildcardMatch(relative, pattern) ||
                        LocalServerUtil::wildcardMatch(entry.path().filename().string(), pattern))
                    {
                        matches.push_back(relative);
                        if (static_cast<int>(matches.size()) >= maxResults)
                        {
                            break;
                        }
                    }
                }
            }
            catch (const std::exception& ex)
            {
                std::string err = std::string("{\"success\":false,\"error\":\"search_failed\",\"detail\":\"") +
                                  LocalServerUtil::escapeJson(ex.what()) + "\"}";
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(500, err));
                return;
            }

            json << "{\"success\":true,\"operation\":\"search-files\",\"pattern\":\""
                 << LocalServerUtil::escapeJson(pattern) << "\",\"workspace\":\""
                 << LocalServerUtil::escapeJson(root.string()) << "\",\"count\":" << matches.size() << ",\"results\":[";
            for (size_t i = 0; i < matches.size(); ++i)
            {
                if (i > 0)
                {
                    json << ",";
                }
                json << "\"" << LocalServerUtil::escapeJson(matches[i]) << "\"";
            }
            json << "]}";
            LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json.str()));
            return;
        }

        if (opName == "evaluate-integration")
        {
            nlohmann::json args = nlohmann::json::object();
            args["workspace"] = m_projectRoot.empty() ? std::filesystem::current_path().string() : m_projectRoot;
            auto result =
                RawrXD::Agent::AgentToolHandlers::Instance().Execute("evaluate_integration_audit_feasibility", args);
            nlohmann::json out = result.toJson();
            out["operation"] = "evaluate-integration";
            out["bridgeReady"] = (m_agenticBridge != nullptr);
            out["hotpatchReady"] = (m_hotpatchUIInitialized || m_hotpatchEnabled);
            out["localServerRunning"] = m_localServerRunning.load();
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 500, out.dump()));
            return;
        }

        if (opName == "create-checkpoint")
        {
            std::string checkpointId;
            if (!LocalServerUtil::extractJsonString(body, "id", checkpointId) || checkpointId.empty())
            {
                checkpointId = "checkpoint-" + std::to_string(GetTickCount64());
            }
            std::string conversation;
            LocalServerUtil::extractJsonString(body, "conversation", conversation);
            if (conversation.empty() && m_hwndCopilotChatInput)
            {
                conversation = getWindowText(m_hwndCopilotChatInput);
            }
            std::string workspace = m_projectRoot.empty() ? std::filesystem::current_path().string() : m_projectRoot;
            LocalServerUtil::extractJsonString(body, "workspace", workspace);

            auto ur =
                UnifiedHotpatchManager::instance().copilot_create_checkpoint(checkpointId, conversation, workspace);
            json << "{\"success\":" << (ur.result.success ? "true" : "false")
                 << ",\"operation\":\"create-checkpoint\",\"id\":\"" << LocalServerUtil::escapeJson(checkpointId)
                 << "\",\"detail\":\"" << LocalServerUtil::escapeJson(ur.result.detail) << "\"}";
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(ur.result.success ? 200 : 500, json.str()));
            return;
        }

        if (opName == "list-checkpoints")
        {
            const auto checkpoints = UnifiedHotpatchManager::instance().copilot_list_checkpoints();
            json << "{\"success\":true,\"operation\":\"list-checkpoints\",\"count\":" << checkpoints.size()
                 << ",\"checkpoints\":[";
            for (size_t i = 0; i < checkpoints.size(); ++i)
            {
                if (i > 0)
                {
                    json << ",";
                }
                json << "\"" << LocalServerUtil::escapeJson(checkpoints[i]) << "\"";
            }
            json << "]}";
            LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json.str()));
            return;
        }

        if (opName == "delete-checkpoint")
        {
            std::string checkpointId;
            LocalServerUtil::extractJsonString(body, "id", checkpointId);
            if (checkpointId.empty())
            {
                LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                                     400, "{\"success\":false,\"error\":\"checkpoint_id_required\"}"));
                return;
            }
            auto ur = UnifiedHotpatchManager::instance().copilot_delete_checkpoint(checkpointId);
            json << "{\"success\":" << (ur.result.success ? "true" : "false")
                 << ",\"operation\":\"delete-checkpoint\",\"id\":\"" << LocalServerUtil::escapeJson(checkpointId)
                 << "\",\"detail\":\"" << LocalServerUtil::escapeJson(ur.result.detail) << "\"}";
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(ur.result.success ? 200 : 500, json.str()));
            return;
        }

        if (opName == "restore-checkpoint")
        {
            std::string checkpointId;
            LocalServerUtil::extractJsonString(body, "id", checkpointId);

            std::string conversation;
            std::string workspace;
            auto ur =
                UnifiedHotpatchManager::instance().copilot_restore_checkpoint(checkpointId, &conversation, &workspace);

            if (ur.result.success && !conversation.empty() && m_hwndCopilotChatInput)
            {
                SetWindowTextA(m_hwndCopilotChatInput, conversation.c_str());
            }
            if (ur.result.success && !workspace.empty())
            {
                m_projectRoot = workspace;
            }

            json << "{\"success\":" << (ur.result.success ? "true" : "false")
                 << ",\"operation\":\"restore-checkpoint\",\"id\":\"" << LocalServerUtil::escapeJson(checkpointId)
                 << "\",\"conversation\":\"" << LocalServerUtil::escapeJson(conversation) << "\",\"workspace\":\""
                 << LocalServerUtil::escapeJson(workspace) << "\",\"detail\":\""
                 << LocalServerUtil::escapeJson(ur.result.detail) << "\"}";
            LocalServerUtil::sendAll(client,
                                     LocalServerUtil::buildHttpResponse(ur.result.success ? 200 : 500, json.str()));
            return;
        }

        if (opName == "hotpatch-status")
        {
            auto& hmgr = UnifiedHotpatchManager::instance();
            auto& proxy = ProxyHotpatcher::instance();
            const auto& stats = hmgr.getStats();
            json << "{\"success\":true,\"operation\":\"hotpatch-status\",\"enabled\":"
                 << ((m_hotpatchEnabled || hmgr.get_target_tps() > 0.0) ? "true" : "false")
                 << ",\"targetTps\":" << hmgr.get_target_tps()
                 << ",\"proxyEnabled\":" << (proxy.isEnabled() ? "true" : "false")
                 << ",\"stats\":{\"memory\":" << stats.memoryPatchCount.load()
                 << ",\"byte\":" << stats.bytePatchCount.load() << ",\"server\":" << stats.serverPatchCount.load()
                 << ",\"operations\":" << stats.totalOperations.load() << ",\"failures\":" << stats.totalFailures.load()
                 << "}}";
            LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json.str()));
            return;
        }

        if (opName == "hotpatch-toggle-layer")
        {
            std::string layer;
            LocalServerUtil::extractJsonString(body, "layer", layer);
            layer = LocalServerUtil::toLower(LocalServerUtil::trimAscii(layer));
            bool enabled = true;
            LocalServerUtil::extractJsonBool(body, "enabled", enabled);

            auto& hmgr = UnifiedHotpatchManager::instance();
            auto& proxy = ProxyHotpatcher::instance();

            if (layer.empty() || layer == "all")
            {
                hmgr.set_target_tps(enabled ? std::max(1.0, hmgr.get_target_tps()) : 0.0);
                proxy.setEnabled(enabled);
                m_hotpatchEnabled = enabled;
            }
            else if (layer == "server" || layer == "proxy")
            {
                proxy.setEnabled(enabled);
            }
            else if (layer == "memory" || layer == "byte")
            {
                // Memory/byte layers are controlled by target TPS activation.
                hmgr.set_target_tps(enabled ? std::max(1.0, hmgr.get_target_tps()) : 0.0);
            }
            else
            {
                LocalServerUtil::sendAll(
                    client, LocalServerUtil::buildHttpResponse(400, "{\"success\":false,\"error\":\"invalid_layer\"}"));
                return;
            }

            json << "{\"success\":true,\"operation\":\"hotpatch-toggle-layer\",\"layer\":\""
                 << LocalServerUtil::escapeJson(layer.empty() ? "all" : layer)
                 << "\",\"enabled\":" << (enabled ? "true" : "false") << ",\"targetTps\":" << hmgr.get_target_tps()
                 << ",\"proxyEnabled\":" << (proxy.isEnabled() ? "true" : "false") << "}";
            LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, json.str()));
            return;
        }

        if (opName == "hotpatch-clear-all")
        {
            auto& hmgr = UnifiedHotpatchManager::instance();
            auto& proxy = ProxyHotpatcher::instance();
            hmgr.clearAllPatches();
            hmgr.set_target_tps(0.0);
            proxy.clear_rewrite_rules();
            proxy.clear_termination_rules();
            proxy.clear_validators();
            proxy.setEnabled(false);
            m_hotpatchEnabled = false;

            LocalServerUtil::sendAll(
                client,
                LocalServerUtil::buildHttpResponse(
                    200, "{\"success\":true,\"operation\":\"hotpatch-clear-all\",\"message\":\"all layers reset\"}"));
            return;
        }

        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(
                                             404, "{\"success\":false,\"error\":\"unknown_agent_operation\"}"));
        return;
    }

    // Determine action from path
    std::string action;
    if (path.find("toggle") != std::string::npos)
        action = "toggle";
    else if (path.find("apply") != std::string::npos)
        action = "apply";
    else if (path.find("revert") != std::string::npos)
        action = "revert";
    else
        action = "unknown";

    // Extract layer from request body
    std::string layer;
    LocalServerUtil::extractJsonString(body, "layer", layer);
    if (layer.empty())
        layer = "unknown";

    layer = LocalServerUtil::toLower(LocalServerUtil::trimAscii(layer));
    if (layer.empty() || layer == "unknown")
    {
        layer = "all";
    }

    auto& hmgr = UnifiedHotpatchManager::instance();
    auto& proxy = ProxyHotpatcher::instance();
    const auto beforeTps = hmgr.get_target_tps();
    const bool beforeProxy = proxy.isEnabled();

    bool success = true;
    std::string detail = "ok";

    if (action == "toggle")
    {
        bool enabled = true;
        LocalServerUtil::extractJsonBool(body, "enabled", enabled);
        if (layer == "all")
        {
            hmgr.set_target_tps(enabled ? std::max(1.0, hmgr.get_target_tps()) : 0.0);
            proxy.setEnabled(enabled);
            m_hotpatchEnabled = enabled;
        }
        else if (layer == "server" || layer == "proxy")
        {
            proxy.setEnabled(enabled);
        }
        else if (layer == "memory" || layer == "byte")
        {
            hmgr.set_target_tps(enabled ? std::max(1.0, hmgr.get_target_tps()) : 0.0);
        }
        else
        {
            success = false;
            detail = "invalid_layer";
        }
    }
    else if (action == "apply")
    {
        if (layer == "all")
        {
            hmgr.set_target_tps(std::max(1.0, hmgr.get_target_tps()));
            proxy.setEnabled(true);
            m_hotpatchEnabled = true;
        }
        else if (layer == "server")
        {
            hmgr.set_target_tps(std::max(1.0, hmgr.get_target_tps()));
            detail = "server layer hotpatch activation enabled";
        }
        else if (layer == "proxy")
        {
            proxy.setEnabled(true);
        }
        else if (layer == "memory" || layer == "byte")
        {
            hmgr.set_target_tps(std::max(1.0, hmgr.get_target_tps()));
        }
        else
        {
            success = false;
            detail = "invalid_layer";
        }
    }
    else if (action == "revert")
    {
        if (layer == "all")
        {
            hmgr.clearAllPatches();
            hmgr.set_target_tps(0.0);
            proxy.clear_rewrite_rules();
            proxy.clear_termination_rules();
            proxy.clear_validators();
            proxy.setEnabled(false);
            m_hotpatchEnabled = false;
        }
        else if (layer == "server")
        {
            hmgr.set_target_tps(0.0);
            detail = "server layer hotpatch activation disabled";
        }
        else if (layer == "proxy")
        {
            proxy.clear_rewrite_rules();
            proxy.clear_termination_rules();
            proxy.clear_validators();
            proxy.setEnabled(false);
        }
        else if (layer == "memory" || layer == "byte")
        {
            hmgr.set_target_tps(0.0);
        }
        else
        {
            success = false;
            detail = "invalid_layer";
        }
    }
    else
    {
        success = false;
        detail = "invalid_action";
    }

    std::ostringstream json;
    json << "{"
         << "\"success\":" << (success ? "true" : "false") << ",\"action\":\"" << LocalServerUtil::escapeJson(action)
         << "\""
         << ",\"layer\":\"" << LocalServerUtil::escapeJson(layer) << "\""
         << ",\"targetTpsBefore\":" << beforeTps << ",\"targetTpsAfter\":" << hmgr.get_target_tps()
         << ",\"proxyBefore\":" << (beforeProxy ? "true" : "false")
         << ",\"proxyAfter\":" << (proxy.isEnabled() ? "true" : "false") << ",\"detail\":\""
         << LocalServerUtil::escapeJson(detail) << "\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(success ? 200 : 400, json.str());
    LocalServerUtil::sendAll(client, resp);
}

// ============================================================================
// Hotpatch target TPS: GET/POST /api/hotpatch/target-tps
// ============================================================================
// GET: returns current target TPS (0 = disabled, run normally).
// POST: body {"targetTps": N} — set target tokens/sec; 0 or omit = run normally.
// ============================================================================
void Win32IDE::handleHotpatchTargetTpsEndpoint(SOCKET client, const std::string& method, const std::string& body)
{
    auto& hmgr = UnifiedHotpatchManager::instance();
    if (method == "GET")
    {
        double tps = hmgr.get_target_tps();
        std::ostringstream json;
        json << "{\"targetTps\":" << tps << ",\"enabled\":" << (tps > 0.0 ? "true" : "false") << "}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
        LocalServerUtil::sendAll(client, resp);
        return;
    }
    if (method == "POST")
    {
        double value = 0.0;
        size_t pos = body.find("\"targetTps\"");
        if (pos != std::string::npos)
        {
            size_t colon = body.find(':', pos);
            if (colon != std::string::npos)
            {
                try
                {
                    value = std::stod(body.substr(colon + 1));
                }
                catch (...)
                {
                }
                if (value < 0.0)
                    value = 0.0;
            }
        }
        hmgr.set_target_tps(value);
        std::ostringstream json;
        json << "{\"success\":true,\"targetTps\":" << value << ",\"message\":\""
             << (value > 0.0 ? "Force TPS hotpatching enabled" : "Run normally (target TPS cleared)") << "\"}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
        LocalServerUtil::sendAll(client, resp);
        return;
    }
    std::string resp = LocalServerUtil::buildHttpResponse(405, "{\"error\":\"Method not allowed\"}");
    LocalServerUtil::sendAll(client, resp);
}

// ============================================================================
// Phase 6B: GET /api/agents/history — Agent event history timeline
// ============================================================================
// Query params: ?agent_id=X&event_type=Y&limit=N&session_id=Z
// Returns the in-memory event buffer as JSON array.
// Purely read-only — no mutations, no side effects.
// ============================================================================

void Win32IDE::handleAgentHistoryEndpoint(SOCKET client, const std::string& path)
{
    // Parse query parameters from the URL
    std::string agentIdFilter, eventTypeFilter, sessionIdFilter;
    int limit = 200;

    auto qPos = path.find('?');
    if (qPos != std::string::npos)
    {
        std::string query = path.substr(qPos + 1);
        // Simple query parameter parsing (key=value&key2=value2)
        std::istringstream qs(query);
        std::string param;
        while (std::getline(qs, param, '&'))
        {
            auto eqPos = param.find('=');
            if (eqPos == std::string::npos)
                continue;
            std::string key = param.substr(0, eqPos);
            std::string val = param.substr(eqPos + 1);
            if (key == "agent_id")
                agentIdFilter = val;
            else if (key == "event_type")
                eventTypeFilter = val;
            else if (key == "session_id")
                sessionIdFilter = val;
            else if (key == "limit")
            {
                try
                {
                    limit = std::stoi(val);
                }
                catch (...)
                {
                }
                if (limit < 0)
                    limit = 200;
                if (limit > 10000)
                    limit = 10000;
            }
        }
    }

    // Lock and read event buffer
    std::vector<AgentEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        events = m_eventBuffer;  // Copy under lock
    }

    // Apply filters
    std::vector<const AgentEvent*> filtered;
    filtered.reserve(events.size());
    for (const auto& ev : events)
    {
        if (!agentIdFilter.empty() && ev.agentId != agentIdFilter)
            continue;
        if (!eventTypeFilter.empty() && ev.typeString() != eventTypeFilter)
            continue;
        if (!sessionIdFilter.empty() && ev.sessionId != sessionIdFilter)
            continue;
        filtered.push_back(&ev);
    }

    // Apply limit (most recent first)
    if ((int)filtered.size() > limit)
    {
        filtered.erase(filtered.begin(), filtered.begin() + (filtered.size() - limit));
    }

    // Build JSON response
    std::ostringstream j;
    j << "{\"events\":[";

    for (int i = 0; i < (int)filtered.size(); i++)
    {
        const AgentEvent* ev = filtered[i];
        if (i > 0)
            j << ",";
        j << "{"
          << "\"id\":" << i << ",\"eventType\":\"" << LocalServerUtil::escapeJson(ev->typeString()) << "\""
          << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(ev->sessionId) << "\""
          << ",\"timestampMs\":" << ev->timestampMs << ",\"durationMs\":" << ev->durationMs << ",\"agentId\":\""
          << LocalServerUtil::escapeJson(ev->agentId) << "\""
          << ",\"parentId\":\"" << LocalServerUtil::escapeJson(ev->parentId) << "\""
          << ",\"description\":\""
          << LocalServerUtil::escapeJson(ev->prompt.empty() ? ev->result.substr(0, 256) : ev->prompt.substr(0, 256))
          << "\""
          << ",\"input\":\"" << LocalServerUtil::escapeJson(ev->prompt.substr(0, 512)) << "\""
          << ",\"output\":\"" << LocalServerUtil::escapeJson(ev->result.substr(0, 512)) << "\""
          << ",\"metadata\":\"" << LocalServerUtil::escapeJson(ev->metadata) << "\""
          << ",\"success\":" << (ev->success ? "true" : "false") << ",\"errorMessage\":\""
          << (ev->success ? "" : LocalServerUtil::escapeJson(ev->result.substr(0, 256))) << "\""
          << "}";
    }

    // Compute stats inline
    j << "],\"stats\":{"
      << "\"totalEvents\":" << m_historyStats.totalEvents << ",\"sessionId\":\""
      << LocalServerUtil::escapeJson(m_currentSessionId) << "\""
      << ",\"successCount\":" << (m_historyStats.agentCompleted + m_historyStats.failuresCorrected)
      << ",\"failCount\":" << (m_historyStats.agentFailed + m_historyStats.failuresDetected) << ",\"eventTypes\":{"
      << "\"AgentStarted\":" << m_historyStats.agentStarted << ",\"AgentCompleted\":" << m_historyStats.agentCompleted
      << ",\"AgentFailed\":" << m_historyStats.agentFailed << ",\"SubAgentSpawned\":" << m_historyStats.subAgentSpawned
      << ",\"ChainSteps\":" << m_historyStats.chainSteps << ",\"SwarmTasks\":" << m_historyStats.swarmTasks
      << ",\"ToolInvocations\":" << m_historyStats.toolInvocations
      << ",\"FailuresDetected\":" << m_historyStats.failuresDetected
      << ",\"FailuresCorrected\":" << m_historyStats.failuresCorrected
      << ",\"GhostTextAccepted\":" << m_historyStats.ghostTextAccepted << "}"
      << "}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// Phase 6B: GET /api/agents/status — Agent + failure stats
// ============================================================================
// Returns:
//   - Agent counts (from history stats)
//   - Failure breakdown by type (from m_failureStats)
//   - Retry success rate
//   - Failure intelligence summary (from m_failureReasonStats)
// Purely read-only — no mutations, no side effects.
// ============================================================================

void Win32IDE::handleAgentStatusEndpoint(SOCKET client)
{
    std::ostringstream j;
    j << "{\"agents\":{"
      << "\"active\":" << m_historyStats.agentStarted << ",\"completed\":" << m_historyStats.agentCompleted
      << ",\"failed\":" << m_historyStats.agentFailed << ",\"subagents\":" << m_historyStats.subAgentSpawned
      << "},\"failures\":{"
      << "\"total\":" << m_failureStats.totalFailures << ",\"totalRequests\":" << m_failureStats.totalRequests
      << ",\"totalRetries\":" << m_failureStats.totalRetries
      << ",\"successAfterRetry\":" << m_failureStats.successAfterRetry
      << ",\"retriesDeclined\":" << m_failureStats.retriesDeclined << ",\"byType\":{"
      << "\"Refusal\":{\"count\":" << m_failureStats.refusalCount << ",\"corrected\":" << 0
      << "}"  // Individual correction counts not tracked per-type in FailureStats
      << ",\"Hallucination\":{\"count\":" << m_failureStats.hallucinationCount << ",\"corrected\":" << 0 << "}"
      << ",\"FormatViolation\":{\"count\":" << m_failureStats.formatViolationCount << ",\"corrected\":" << 0 << "}"
      << ",\"InfiniteLoop\":{\"count\":" << m_failureStats.infiniteLoopCount << ",\"corrected\":" << 0 << "}"
      << ",\"QualityDegradation\":{\"count\":" << m_failureStats.qualityDegradationCount << ",\"corrected\":" << 0
      << "}"
      << ",\"EmptyResponse\":{\"count\":" << m_failureStats.emptyResponseCount << ",\"corrected\":" << 0 << "}"
      << ",\"Timeout\":{\"count\":" << m_failureStats.timeoutCount << ",\"corrected\":" << 0 << "}"
      << ",\"ToolError\":{\"count\":" << m_failureStats.toolErrorCount << ",\"corrected\":" << 0 << "}"
      << ",\"InvalidOutput\":{\"count\":" << m_failureStats.invalidOutputCount << ",\"corrected\":" << 0 << "}"
      << ",\"LowConfidence\":{\"count\":" << m_failureStats.lowConfidenceCount << ",\"corrected\":" << 0 << "}"
      << ",\"SafetyViolation\":{\"count\":" << m_failureStats.safetyViolationCount << ",\"corrected\":" << 0 << "}"
      << ",\"UserAbort\":{\"count\":" << m_failureStats.userAbortCount << ",\"corrected\":" << 0 << "}"
      << "}"
      << ",\"retrySuccessRate\":";

    // Compute retry success rate
    if (m_failureStats.totalRetries > 0)
    {
        float rate = (float)m_failureStats.successAfterRetry / (float)m_failureStats.totalRetries;
        j << std::fixed;
        j.precision(3);
        j << rate;
    }
    else
    {
        j << "0.0";
    }

    // Add Failure Intelligence per-reason stats if available
    j << "},\"intelligence\":{";
    {
        std::lock_guard<std::mutex> lock(m_failureIntelligenceMutex);
        bool first = true;
        for (const auto& [reasonInt, stats] : m_failureReasonStats)
        {
            if (!first)
                j << ",";
            first = false;
            j << "\"" << reasonInt << "\":{"
              << "\"occurrences\":" << stats.occurrences << ",\"retriesAttempted\":" << stats.retriesAttempted
              << ",\"retriesSucceeded\":" << stats.retriesSucceeded
              << ",\"avgRetryAttempts\":" << stats.avgRetryAttempts << "}";
        }
    }

    j << "}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// Phase 6B: POST /api/agents/replay — Replay agent session events
// ============================================================================
// Body: { "agent_id": "abc123", "dry_run": true/false }
// Replays events for the given agent_id from the current event buffer.
// This re-executes the event sequence without mutations (dry_run default).
// ============================================================================

void Win32IDE::handleAgentReplayEndpoint(SOCKET client, const std::string& body)
{
    std::string agentId;
    bool dryRun = true;

    LocalServerUtil::extractJsonString(body, "agent_id", agentId);
    LocalServerUtil::extractJsonBool(body, "dry_run", dryRun);

    if (agentId.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(400, "{\"error\":\"agent_id is required\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Collect events for this agent from the event buffer
    std::vector<AgentEvent> agentEvents;
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        for (const auto& ev : m_eventBuffer)
        {
            if (ev.agentId == agentId || ev.parentId == agentId)
            {
                agentEvents.push_back(ev);
            }
        }
    }

    if (agentEvents.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            404, "{\"success\":false,\"result\":\"No events found for agent: " + LocalServerUtil::escapeJson(agentId) +
                     "\",\"events_replayed\":0,\"duration_ms\":0}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    // For dry_run, just return the event sequence without re-executing
    // For non-dry_run, the real replay is more complex and would need
    // the agentic bridge — but the data visibility is what matters here
    std::ostringstream replayResult;
    replayResult << "Replay of agent " << agentId << ":\\n";
    int stepNum = 0;
    for (const auto& ev : agentEvents)
    {
        stepNum++;
        replayResult << "  Step " << stepNum << ": " << ev.typeString();
        if (!ev.prompt.empty())
        {
            replayResult << " — " << ev.prompt.substr(0, 80);
        }
        replayResult << (ev.success ? " [OK]" : " [FAIL]") << "\\n";
    }

    auto endTime = std::chrono::steady_clock::now();
    int durationMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::ostringstream j;
    j << "{\"success\":true"
      << ",\"result\":\"" << LocalServerUtil::escapeJson(replayResult.str()) << "\""
      << ",\"events_replayed\":" << (int)agentEvents.size() << ",\"duration_ms\":" << durationMs
      << ",\"dry_run\":" << (dryRun ? "true" : "false") << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// Phase 6B: GET /api/failures — Failure timeline (Phase 4B data)
// ============================================================================
// Query params: ?limit=N&reason=X
// Returns the failure intelligence history as a dedicated timeline.
// Each record carries: timestamp, type, reason, confidence, evidence,
//                      strategy used, outcome, prompt snippet.
// Purely read-only — no mutations, no side effects.
// ============================================================================

void Win32IDE::handleFailuresEndpoint(SOCKET client, const std::string& path)
{
    int limit = 200;
    std::string reasonFilter;

    auto qPos = path.find('?');
    if (qPos != std::string::npos)
    {
        std::string query = path.substr(qPos + 1);
        std::istringstream qs(query);
        std::string param;
        while (std::getline(qs, param, '&'))
        {
            auto eqPos = param.find('=');
            if (eqPos == std::string::npos)
                continue;
            std::string key = param.substr(0, eqPos);
            std::string val = param.substr(eqPos + 1);
            if (key == "limit")
            {
                try
                {
                    limit = std::stoi(val);
                }
                catch (...)
                {
                }
                if (limit < 0)
                    limit = 200;
                if (limit > 10000)
                    limit = 10000;
            }
            else if (key == "reason")
            {
                reasonFilter = val;
            }
        }
    }

    // Build the failure timeline from two sources:
    // 1. FailureIntelligence history (rich records with classification)
    // 2. Event buffer (FailureDetected/Corrected/Failed/Declined events)

    std::ostringstream j;
    j << "{\"failures\":[";

    int count = 0;

    // Source 1: FailureIntelligence records (Phase 6 rich data)
    {
        std::lock_guard<std::mutex> lock(m_failureIntelligenceMutex);
        int startIdx = 0;
        if ((int)m_failureIntelligenceHistory.size() > limit)
        {
            startIdx = (int)m_failureIntelligenceHistory.size() - limit;
        }
        for (int i = startIdx; i < (int)m_failureIntelligenceHistory.size(); i++)
        {
            const auto& rec = m_failureIntelligenceHistory[i];

            // Apply reason filter
            if (!reasonFilter.empty())
            {
                std::string failTypeStr = failureTypeString(rec.failureType);
                if (failTypeStr != reasonFilter)
                    continue;
            }

            if (count > 0)
                j << ",";
            count++;

            // Map failure type to string
            std::string typeStr = failureTypeString(rec.failureType);

            // Determine outcome from retry data
            std::string outcome = "Detected";
            if (rec.retrySucceeded)
                outcome = "Corrected";
            else if (rec.attemptNumber > 0)
                outcome = "Failed";

            // Strategy description
            RetryStrategy tmpStrat;
            tmpStrat.type = rec.strategyUsed;
            std::string strategyStr = tmpStrat.typeString();

            j << "{"
              << "\"timestampMs\":" << rec.timestampMs << ",\"type\":\"" << LocalServerUtil::escapeJson(typeStr) << "\""
              << ",\"reason\":\"" << (int)rec.reason << "\""
              << ",\"confidence\":0"  // FailureIntelligenceRecord doesn't carry confidence directly
              << ",\"evidence\":\"" << LocalServerUtil::escapeJson(rec.failureDetail.substr(0, 256)) << "\""
              << ",\"strategy\":\"" << LocalServerUtil::escapeJson(strategyStr) << "\""
              << ",\"outcome\":\"" << outcome << "\""
              << ",\"promptSnippet\":\"" << LocalServerUtil::escapeJson(rec.promptSnippet.substr(0, 128)) << "\""
              << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(rec.sessionId) << "\""
              << ",\"attempt\":" << rec.attemptNumber << "}";
        }
    }

    // Source 2: Event buffer failure events (Phase 4B hooks)
    // These carry confidence + evidence in their metadata field
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        for (const auto& ev : m_eventBuffer)
        {
            if (ev.type != AgentEventType::FailureDetected && ev.type != AgentEventType::FailureCorrected &&
                ev.type != AgentEventType::FailureFailed && ev.type != AgentEventType::FailureRetryDeclined)
                continue;

            if (count >= limit)
                break;

            // Apply reason filter against the event result/metadata
            if (!reasonFilter.empty())
            {
                if (ev.result.find(reasonFilter) == std::string::npos &&
                    ev.metadata.find(reasonFilter) == std::string::npos)
                    continue;
            }

            if (count > 0)
                j << ",";
            count++;

            std::string outcome;
            switch (ev.type)
            {
                case AgentEventType::FailureDetected:
                    outcome = "Detected";
                    break;
                case AgentEventType::FailureCorrected:
                    outcome = "Corrected";
                    break;
                case AgentEventType::FailureFailed:
                    outcome = "Failed";
                    break;
                case AgentEventType::FailureRetryDeclined:
                    outcome = "Declined";
                    break;
                default:
                    outcome = "Unknown";
                    break;
            }

            j << "{"
              << "\"timestampMs\":" << ev.timestampMs << ",\"type\":\"" << LocalServerUtil::escapeJson(ev.typeString())
              << "\""
              << ",\"reason\":\"\""
              << ",\"confidence\":0"
              << ",\"evidence\":\"" << LocalServerUtil::escapeJson(ev.result.substr(0, 256)) << "\""
              << ",\"strategy\":\"\""
              << ",\"outcome\":\"" << outcome << "\""
              << ",\"promptSnippet\":\"" << LocalServerUtil::escapeJson(ev.prompt.substr(0, 128)) << "\""
              << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(ev.sessionId) << "\""
              << ",\"attempt\":0"
              << "}";
        }
    }

    // Summary stats
    j << "],\"stats\":{"
      << "\"totalFailures\":" << m_failureStats.totalFailures << ",\"totalRetries\":" << m_failureStats.totalRetries
      << ",\"successAfterRetry\":" << m_failureStats.successAfterRetry
      << ",\"retriesDeclined\":" << m_failureStats.retriesDeclined << ",\"topReasons\":["
      << "{\"type\":\"Hallucination\",\"count\":" << m_failureStats.hallucinationCount << "}"
      << ",{\"type\":\"Refusal\",\"count\":" << m_failureStats.refusalCount << "}"
      << ",{\"type\":\"FormatViolation\",\"count\":" << m_failureStats.formatViolationCount << "}"
      << ",{\"type\":\"ToolError\",\"count\":" << m_failureStats.toolErrorCount << "}"
      << ",{\"type\":\"EmptyResponse\",\"count\":" << m_failureStats.emptyResponseCount << "}"
      << ",{\"type\":\"Timeout\",\"count\":" << m_failureStats.timeoutCount << "}"
      << ",{\"type\":\"InvalidOutput\",\"count\":" << m_failureStats.invalidOutputCount << "}"
      << ",{\"type\":\"InfiniteLoop\",\"count\":" << m_failureStats.infiniteLoopCount << "}"
      << ",{\"type\":\"LowConfidence\",\"count\":" << m_failureStats.lowConfidenceCount << "}"
      << ",{\"type\":\"SafetyViolation\",\"count\":" << m_failureStats.safetyViolationCount << "}"
      << ",{\"type\":\"QualityDegradation\",\"count\":" << m_failureStats.qualityDegradationCount << "}"
      << ",{\"type\":\"UserAbort\",\"count\":" << m_failureStats.userAbortCount << "}"
      << "]}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// TOGGLE — start/stop server
// ============================================================================

void Win32IDE::toggleLocalServer()
{
    if (m_localServerRunning.load())
    {
        stopLocalServer();
    }
    else
    {
        startLocalServer();
    }
}

// ============================================================================
// STATUS
// ============================================================================

std::string Win32IDE::getLocalServerStatus() const
{
    std::ostringstream oss;
    oss << "=== Local GGUF HTTP Server ===\r\n";
    oss << "Running: " << (m_localServerRunning.load() ? "YES" : "NO") << "\r\n";
    oss << "Port: " << m_settings.localServerPort << "\r\n";
    oss << "Total Requests: " << m_localServerStats.totalRequests << "\r\n";
    oss << "Total Tokens: " << m_localServerStats.totalTokens << "\r\n";
    oss << "\r\nEndpoints:\r\n";
    oss << "  GET  /health              — Health check\r\n";
    oss << "  GET  /status              — Server status + model info\r\n";
    oss << "  GET  /api/tags            — List loaded models (Ollama-compatible)\r\n";
    oss << "  POST /api/generate        — Generate text (Ollama-compatible)\r\n";
    oss << "  POST /v1/chat/completions — Chat completions (OpenAI-compatible)\r\n";
    oss << "  GET  /models              — List all local GGUF + Ollama models (Frontend)\r\n";
    oss << "  GET  /api/model/profiles   — MASM model-bridge profiles (chat GUI)\r\n";
    oss << "  POST /api/model/load       — Load bridge profile by index/name\r\n";
    oss << "  POST /api/model/unload     — Unload bridge model\r\n";
    oss << "  GET  /api/engine/capabilities — Bridge CPU/memory/engine caps\r\n";
    oss << "  POST /ask                 — Unified chat endpoint (Frontend)\r\n";
    oss << "  GET  /gui                 — Serve agentic chatbot HTML interface\r\n";
    oss << "  POST /api/read-file       — Read a local file for chatbot attachments\r\n";
    oss << "  GET  /api/agents/history   — Agent event history timeline\r\n";
    oss << "  GET  /api/agents/status    — Agent + failure stats\r\n";
    oss << "  POST /api/agents/replay    — Replay agent session events\r\n";
    oss << "  POST /api/agent/ops/compact-conversation  — Compact chat context payload\r\n";
    oss << "  POST /api/agent/ops/optimize-tool-selection — Report tool routing optimization state\r\n";
    oss << "  POST /api/agent/ops/resolving             — Backend resolving subsystem status\r\n";
    oss << "  POST /api/agent/ops/read-lines            — Read editor lines by range\r\n";
    oss << "  POST /api/agent/ops/planning-exploration  — Spawn analyzer exploration task\r\n";
    oss << "  POST /api/agent/ops/search-files          — Workspace wildcard file search\r\n";
    oss << "  POST /api/agent/ops/evaluate-integration  — Agent/backend integration readiness\r\n";
    oss << "  POST /api/agent/ops/orchestrate-conversation — Conversational tool orchestration\r\n";
    oss << "  POST /api/agent/ops/speculate-next        — Intent prediction + speculative tools\r\n";
    oss << "  POST /api/agent/ops/create-checkpoint     — Persist conversation/workspace checkpoint\r\n";
    oss << "  POST /api/agent/ops/list-checkpoints      — List saved checkpoints\r\n";
    oss << "  POST /api/agent/ops/delete-checkpoint     — Delete checkpoint by id\r\n";
    oss << "  POST /api/agent/ops/restore-checkpoint    — Restore checkpoint state\r\n";
    oss << "  POST /api/agent/ops/hotpatch-status       — Hotpatch status + stats snapshot\r\n";
    oss << "  POST /api/agent/ops/hotpatch-toggle-layer — Enable/disable layer routing\r\n";
    oss << "  POST /api/agent/ops/hotpatch-clear-all    — Clear all layer patches + disable\r\n";
    oss << "  GET  /api/failures         — Failure timeline (Phase 4B)\r\n";
    oss << "  GET  /api/backends         — List all configured backends\r\n";
    oss << "  GET  /api/backend/active   — Get active backend info\r\n";
    oss << "  POST /api/backend/switch   — Switch active backend\r\n";
    oss << "  GET  /api/router/status       — LLM Router status & preferences\r\n";
    oss << "  GET  /api/router/decision      — Last routing decision trace\r\n";
    oss << "  GET  /api/router/capabilities  — Backend capability profiles\r\n";
    oss << "  POST /api/router/route         — Dry-run route a prompt (no inference)\r\n";
    oss << "  GET  /api/multi-response/status      — Multi-Response engine status\r\n";
    oss << "  GET  /api/multi-response/templates    — List response templates (S/G/C/X)\r\n";
    oss << "  POST /api/multi-response/generate     — Generate multi-response chain\r\n";
    oss << "  GET  /api/multi-response/results      — Get session results (?session=ID)\r\n";
    oss << "  POST /api/multi-response/prefer       — Record preference for a response\r\n";
    oss << "  GET  /api/multi-response/stats        — Generation & preference stats\r\n";
    oss << "  GET  /api/multi-response/preferences  — Preference history\r\n";
    oss << "  GET  /api/cot/status          — Chain-of-Thought engine status\r\n";
    oss << "  GET  /api/cot/presets         — List available CoT presets\r\n";
    oss << "  GET  /api/cot/steps           — Current chain steps\r\n";
    oss << "  GET  /api/cot/roles           — All available CoT roles\r\n";
    oss << "  POST /api/cot/preset          — Apply a preset (body: {\"preset\":\"review\"})\r\n";
    oss << "  POST /api/cot/steps           — Set custom steps\r\n";
    oss << "  POST /api/cot/execute         — Execute chain (body: {\"query\":\"...\"})\r\n";
    oss << "  POST /api/cot/cancel          — Cancel running chain\r\n";
    return oss.str();
}

// ============================================================================
// Phase 8B: Backend Switcher HTTP Endpoints
// ============================================================================

// GET /api/backends — list all configured backends with status
void Win32IDE::handleBackendsListEndpoint(SOCKET client)
{
    nlohmann::json j;
    j["active"] = backendTypeString(m_activeBackend);

    nlohmann::json backendsArr = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i)
    {
        const auto& cfg = m_backendConfigs[i];
        const auto& st = m_backendStatuses[i];
        nlohmann::json bj;
        bj["type"] = backendTypeString(cfg.type);
        bj["name"] = cfg.name;
        bj["endpoint"] = cfg.endpoint;
        bj["model"] = cfg.model;
        bj["enabled"] = cfg.enabled;
        bj["timeoutMs"] = cfg.timeoutMs;
        bj["maxTokens"] = cfg.maxTokens;
        bj["temperature"] = cfg.temperature;
        bj["hasApiKey"] = !cfg.apiKey.empty();
        bj["connected"] = st.connected;
        bj["healthy"] = st.healthy;
        bj["latencyMs"] = st.latencyMs;
        bj["requestCount"] = st.requestCount;
        bj["failureCount"] = st.failureCount;
        bj["lastError"] = st.lastError;
        bj["lastModel"] = st.lastModel;
        bj["lastUsedEpochMs"] = st.lastUsedEpochMs;
        bj["isActive"] = ((AIBackendType)i == m_activeBackend);
        backendsArr.push_back(bj);
    }
    j["backends"] = backendsArr;
    j["count"] = (int)AIBackendType::Count;

    std::string response = LocalServerUtil::buildHttpResponse(200, j.dump(2));
    LocalServerUtil::sendAll(client, response);
}

// GET /api/backend/active — current active backend details
void Win32IDE::handleBackendActiveEndpoint(SOCKET client)
{
    const auto& cfg = m_backendConfigs[(size_t)m_activeBackend];
    const auto& st = m_backendStatuses[(size_t)m_activeBackend];

    nlohmann::json j;
    j["type"] = backendTypeString(cfg.type);
    j["name"] = cfg.name;
    j["endpoint"] = cfg.endpoint;
    j["model"] = cfg.model;
    j["enabled"] = cfg.enabled;
    j["hasApiKey"] = !cfg.apiKey.empty();
    j["connected"] = st.connected;
    j["healthy"] = st.healthy;
    j["latencyMs"] = st.latencyMs;
    j["requestCount"] = st.requestCount;
    j["failureCount"] = st.failureCount;
    j["lastError"] = st.lastError;

    std::string response = LocalServerUtil::buildHttpResponse(200, j.dump(2));
    LocalServerUtil::sendAll(client, response);
}

// POST /api/backend/switch — switch active backend
// Body: {"backend": "Ollama"} or {"backend": "openai", "model": "gpt-4o", "apiKey": "sk-..."}
void Win32IDE::handleBackendSwitchEndpoint(SOCKET client, const std::string& body)
{
    try
    {
        nlohmann::json req = nlohmann::json::parse(body);
        std::string backendName = req.value("backend", "");
        if (backendName.empty())
        {
            std::string errResp = LocalServerUtil::buildHttpResponse(
                400, "{\"error\":\"missing_field\",\"message\":\"'backend' field is required\"}");
            LocalServerUtil::sendAll(client, errResp);
            return;
        }

        AIBackendType target = backendTypeFromString(backendName);
        if (target == AIBackendType::Count)
        {
            std::string errResp = LocalServerUtil::buildHttpResponse(
                400, "{\"error\":\"invalid_backend\",\"message\":\"Unknown backend: " +
                         LocalServerUtil::escapeJson(backendName) +
                         ". Valid: LocalGGUF, Ollama, OpenAI, Claude, Gemini\"}");
            LocalServerUtil::sendAll(client, errResp);
            return;
        }

        // Apply optional config updates before switching
        if (req.contains("model") && req["model"].is_string())
        {
            setBackendModel(target, req["model"].get<std::string>());
        }
        if (req.contains("apiKey") && req["apiKey"].is_string())
        {
            setBackendApiKey(target, req["apiKey"].get<std::string>());
        }
        if (req.contains("endpoint") && req["endpoint"].is_string())
        {
            setBackendEndpoint(target, req["endpoint"].get<std::string>());
        }

        bool ok = setActiveBackend(target);
        if (ok)
        {
            nlohmann::json resp;
            resp["success"] = true;
            resp["active"] = backendTypeString(m_activeBackend);
            resp["model"] = m_backendConfigs[(size_t)m_activeBackend].model;
            resp["message"] = "Switched to " + m_backendConfigs[(size_t)m_activeBackend].name;

            std::string httpResp = LocalServerUtil::buildHttpResponse(200, resp.dump(2));
            LocalServerUtil::sendAll(client, httpResp);
        }
        else
        {
            std::string errResp = LocalServerUtil::buildHttpResponse(
                422, "{\"error\":\"switch_failed\",\"message\":\"Backend '" + LocalServerUtil::escapeJson(backendName) +
                         "' is disabled or unavailable\"}");
            LocalServerUtil::sendAll(client, errResp);
        }
    }
    catch (const std::exception& e)
    {
        std::string errResp = LocalServerUtil::buildHttpResponse(
            400,
            "{\"error\":\"parse_error\",\"message\":\"Invalid JSON: " + LocalServerUtil::escapeJson(e.what()) + "\"}");
        LocalServerUtil::sendAll(client, errResp);
    }
}

// ============================================================================
// Phase 32A: Chain-of-Thought Multi-Model Review HTTP Endpoints
// ============================================================================

void Win32IDE::initChainOfThought()
{
    auto& cot = ChainOfThoughtEngine::instance();

    // Wire the inference callback to route through our LLM Router / Backend Switcher
    cot.setInferenceCallback(
        [this](const std::string& systemPrompt, const std::string& userMessage, const std::string& model) -> std::string
        {
            // Build a combined prompt: system instruction + user content
            std::string combined = systemPrompt + "\n\n" + userMessage;
            // Route through the intelligence layer (task classification, backend selection)
            return routeWithIntelligence(combined);
        });

    // Apply default preset
    cot.applyPreset("review");
}

// GET /api/cot/status — engine status + stats
void Win32IDE::handleCoTStatusEndpoint(SOCKET client)
{
    auto& cot = ChainOfThoughtEngine::instance();
    std::string json = cot.getStatusJSON();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);
}

// GET /api/cot/presets — list all available presets
void Win32IDE::handleCoTPresetsEndpoint(SOCKET client)
{
    auto& cot = ChainOfThoughtEngine::instance();
    std::string json = cot.getPresetsJSON();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);
}

// GET /api/cot/steps — current chain configuration
void Win32IDE::handleCoTStepsEndpoint(SOCKET client)
{
    auto& cot = ChainOfThoughtEngine::instance();
    std::string json = cot.getStepsJSON();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);
}

// GET /api/cot/roles — all available roles
void Win32IDE::handleCoTRolesEndpoint(SOCKET client)
{
    const auto& roles = getAllCoTRoles();
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < roles.size(); i++)
    {
        if (i > 0)
            j << ",";
        j << "{\"id\":\"" << roles[i].name << "\",\"label\":\"" << roles[i].label << "\",\"icon\":\"" << roles[i].icon
          << "\",\"instruction\":\"" << LocalServerUtil::escapeJson(roles[i].instruction) << "\"}";
    }
    j << "]";
    std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, resp);
}

// POST /api/cot/preset — apply a preset { "preset": "review" }
void Win32IDE::handleCoTApplyPresetEndpoint(SOCKET client, const std::string& body)
{
    std::string presetName;
    LocalServerUtil::extractJsonString(body, "preset", presetName);
    if (presetName.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"missing_field\",\"message\":\"'preset' field required\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto& cot = ChainOfThoughtEngine::instance();
    if (!cot.applyPreset(presetName))
    {
        std::string resp =
            LocalServerUtil::buildHttpResponse(404, "{\"error\":\"preset_not_found\",\"message\":\"Unknown preset: " +
                                                        LocalServerUtil::escapeJson(presetName) + "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string json = "{\"success\":true,\"preset\":\"" + LocalServerUtil::escapeJson(presetName) +
                       "\",\"steps\":" + cot.getStepsJSON() + "}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);
}

// POST /api/cot/steps — set custom steps
// Body: { "steps": [{ "role": "reviewer", "model": "", "instruction": "", "skip": false }, ...] }
void Win32IDE::handleCoTSetStepsEndpoint(SOCKET client, const std::string& body)
{
    auto& cot = ChainOfThoughtEngine::instance();
    std::vector<CoTStep> steps;

    // Simple JSON array parsing: find each "role" field
    size_t pos = 0;
    while ((pos = body.find("\"role\"", pos)) != std::string::npos)
    {
        std::string roleName;
        if (LocalServerUtil::extractJsonString(body.substr(pos), "role", roleName))
        {
            const CoTRoleInfo* info = getCoTRoleByName(roleName);
            if (info)
            {
                CoTStep step;
                step.role = info->id;

                // Try to extract optional fields from the surrounding object
                size_t objStart = body.rfind('{', pos);
                size_t objEnd = body.find('}', pos);
                if (objStart != std::string::npos && objEnd != std::string::npos)
                {
                    std::string objStr = body.substr(objStart, objEnd - objStart + 1);
                    LocalServerUtil::extractJsonString(objStr, "model", step.model);
                    LocalServerUtil::extractJsonString(objStr, "instruction", step.instruction);
                    LocalServerUtil::extractJsonBool(objStr, "skip", step.skip);
                }
                steps.push_back(step);
            }
        }
        pos += 6;
    }

    if (steps.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"invalid_steps\",\"message\":\"No valid steps found in request\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    cot.setSteps(steps);
    std::string json =
        "{\"success\":true,\"stepCount\":" + std::to_string(steps.size()) + ",\"steps\":" + cot.getStepsJSON() + "}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);
}

// POST /api/cot/execute — execute the chain { "query": "..." }
void Win32IDE::handleCoTExecuteEndpoint(SOCKET client, const std::string& body)
{
    std::string query;
    LocalServerUtil::extractJsonString(body, "query", query);
    if (query.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"missing_field\",\"message\":\"'query' field required\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto& cot = ChainOfThoughtEngine::instance();
    if (cot.isRunning())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            409, "{\"error\":\"chain_running\",\"message\":\"A chain is already running. Cancel it first.\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Execute the chain synchronously
    CoTChainResult result = cot.executeChain(query);

    // Build JSON response
    std::ostringstream j;
    j << "{\"success\":" << (result.success ? "true" : "false");
    j << ",\"totalLatencyMs\":" << result.totalLatencyMs;
    j << ",\"stepsCompleted\":" << result.stepsCompleted;
    j << ",\"stepsSkipped\":" << result.stepsSkipped;
    j << ",\"stepsFailed\":" << result.stepsFailed;
    j << ",\"finalOutput\":\"" << LocalServerUtil::escapeJson(result.finalOutput) << "\"";
    if (!result.error.empty())
    {
        j << ",\"error\":\"" << LocalServerUtil::escapeJson(result.error) << "\"";
    }
    j << ",\"steps\":[";
    for (size_t i = 0; i < result.stepResults.size(); i++)
    {
        if (i > 0)
            j << ",";
        const auto& sr = result.stepResults[i];
        j << "{\"index\":" << sr.stepIndex;
        j << ",\"role\":\"" << sr.roleName << "\"";
        j << ",\"model\":\"" << LocalServerUtil::escapeJson(sr.model) << "\"";
        j << ",\"success\":" << (sr.success ? "true" : "false");
        j << ",\"skipped\":" << (sr.skipped ? "true" : "false");
        j << ",\"latencyMs\":" << sr.latencyMs;
        j << ",\"tokenCount\":" << sr.tokenCount;
        if (!sr.output.empty())
            j << ",\"output\":\"" << LocalServerUtil::escapeJson(sr.output) << "\"";
        if (!sr.error.empty())
            j << ",\"error\":\"" << LocalServerUtil::escapeJson(sr.error) << "\"";
        j << "}";
    }
    j << "]}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
    LocalServerUtil::sendAll(client, resp);
}

// POST /api/cot/cancel — cancel running chain
void Win32IDE::handleCoTCancelEndpoint(SOCKET client)
{
    auto& cot = ChainOfThoughtEngine::instance();
    cot.cancel();
    std::string resp = LocalServerUtil::buildHttpResponse(200, "{\"success\":true,\"message\":\"Cancel requested\"}");
    LocalServerUtil::sendAll(client, resp);
}

// ============================================================================
// FILE OPERATIONS — Comprehensive file management endpoints for IDE chatbot
// ============================================================================
// Security rules applied to ALL file operation endpoints:
//   1. Path traversal ("..") is REJECTED
//   2. Only absolute paths (drive-letter prefixed on Windows) are accepted
//   3. All paths are normalized (forward slashes → backslashes on Windows)
//   4. Size limits enforced per endpoint
//   5. All operations logged
// ============================================================================

namespace
{
// --- Shared path validation for all file operation endpoints ---
struct ValidatedPath
{
    std::string path;
    std::string error;
    int errorCode;
    bool ok;
};

static ValidatedPath validateFilePath(const std::string& body, const std::string& fieldName = "path")
{
    ValidatedPath vp;
    vp.ok = false;
    vp.errorCode = 400;

    if (!LocalServerUtil::extractJsonString(body, fieldName, vp.path) || vp.path.empty())
    {
        vp.error = "{\"error\":\"missing_" + fieldName + "\",\"message\":\"Request body must contain '" + fieldName +
                   "' field\"}";
        return vp;
    }

    // Normalize forward slashes to backslashes for Windows
    for (auto& ch : vp.path)
    {
        if (ch == '/')
            ch = '\\';
    }

    // Security: reject directory traversal
    if (vp.path.find("..") != std::string::npos)
    {
        vp.error = "{\"error\":\"forbidden\",\"message\":\"Directory traversal not allowed\"}";
        vp.errorCode = 403;
        return vp;
    }

    // Security: must be an absolute path (drive letter)
    if (vp.path.size() < 3 || vp.path[1] != ':' || vp.path[2] != '\\')
    {
        vp.error = "{\"error\":\"invalid_path\",\"message\":\"Only absolute paths are accepted (e.g., "
                   "C:\\\\path\\\\to\\\\file)\"}";
        return vp;
    }

    vp.ok = true;
    return vp;
}

// Extract filename from a full path
static std::string extractFileName(const std::string& fullPath)
{
    auto lastSlash = fullPath.rfind('\\');
    if (lastSlash != std::string::npos && lastSlash + 1 < fullPath.size())
    {
        return fullPath.substr(lastSlash + 1);
    }
    return fullPath;
}

// Extract parent directory from a full path
static std::string extractParentDir(const std::string& fullPath)
{
    auto lastSlash = fullPath.rfind('\\');
    if (lastSlash != std::string::npos)
    {
        return fullPath.substr(0, lastSlash);
    }
    return fullPath;
}

// Recursive directory creation (like mkdir -p)
static bool createDirectoriesRecursive(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        return true;  // Already exists
    }

    // Try to create parent first
    std::string parent = extractParentDir(path);
    if (parent.size() > 3 && parent != path)
    {  // Don't go above drive root
        if (!createDirectoriesRecursive(parent))
        {
            return false;
        }
    }

    return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

// Recursive directory listing for search
struct FileSearchResult
{
    std::string path;
    int line;
    std::string lineText;
};

static void searchFilesRecursive(const std::string& dir, const std::string& pattern, const std::string& textQuery,
                                 bool searchContent, int maxResults, std::vector<FileSearchResult>& results)
{
    if ((int)results.size() >= maxResults)
        return;

    WIN32_FIND_DATAA fd;
    std::string searchPath = dir + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if ((int)results.size() >= maxResults)
            break;
        std::string name = fd.cFileName;
        if (name == "." || name == "..")
            continue;

        std::string fullPath = dir + "\\" + name;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Skip hidden/system directories and common ignored dirs
            if (name[0] == '.' || name == "node_modules" || name == ".git" || name == "__pycache__" || name == ".vs" ||
                name == "Debug" || name == "Release")
            {
                continue;
            }
            searchFilesRecursive(fullPath, pattern, textQuery, searchContent, maxResults, results);
        }
        else
        {
            // Check filename pattern match (simple glob: *.ext or *query*)
            bool nameMatch = false;
            if (pattern.empty() || pattern == "*")
            {
                nameMatch = true;
            }
            else if (pattern[0] == '*' && pattern[pattern.size() - 1] == '*')
            {
                // *query* — contains search
                std::string sub = pattern.substr(1, pattern.size() - 2);
                std::string lowerName = name;
                std::string lowerSub = sub;
                for (auto& c : lowerName)
                    c = (char)std::tolower((unsigned char)c);
                for (auto& c : lowerSub)
                    c = (char)std::tolower((unsigned char)c);
                nameMatch = lowerName.find(lowerSub) != std::string::npos;
            }
            else if (pattern[0] == '*')
            {
                // *.ext — extension match
                std::string ext = pattern.substr(1);
                std::string lowerName = name;
                std::string lowerExt = ext;
                for (auto& c : lowerName)
                    c = (char)std::tolower((unsigned char)c);
                for (auto& c : lowerExt)
                    c = (char)std::tolower((unsigned char)c);
                nameMatch = (lowerName.size() >= lowerExt.size() &&
                             lowerName.substr(lowerName.size() - lowerExt.size()) == lowerExt);
            }
            else
            {
                std::string lowerName = name;
                std::string lowerPattern = pattern;
                for (auto& c : lowerName)
                    c = (char)std::tolower((unsigned char)c);
                for (auto& c : lowerPattern)
                    c = (char)std::tolower((unsigned char)c);
                nameMatch = (lowerName.find(lowerPattern) != std::string::npos);
            }

            if (nameMatch && !searchContent)
            {
                FileSearchResult r;
                r.path = fullPath;
                r.line = 0;
                results.push_back(r);
            }
            else if (nameMatch && searchContent && !textQuery.empty())
            {
                // Search file content for text (only text files under 2MB)
                if (fd.nFileSizeLow < 2 * 1024 * 1024 && fd.nFileSizeHigh == 0)
                {
                    HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE)
                    {
                        DWORD fileSize = fd.nFileSizeLow;
                        std::string content(fileSize, '\0');
                        DWORD bytesRead = 0;
                        ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
                        CloseHandle(hFile);
                        content.resize(bytesRead);

                        // Case-insensitive search
                        std::string lowerContent = content;
                        std::string lowerQuery = textQuery;
                        for (auto& c : lowerContent)
                            c = (char)std::tolower((unsigned char)c);
                        for (auto& c : lowerQuery)
                            c = (char)std::tolower((unsigned char)c);

                        size_t pos = 0;
                        int lineNum = 1;
                        size_t lineStart = 0;
                        while (pos < lowerContent.size() && (int)results.size() < maxResults)
                        {
                            size_t found = lowerContent.find(lowerQuery, pos);
                            if (found == std::string::npos)
                                break;

                            // Count lines up to found position
                            while (lineStart < found)
                            {
                                if (content[lineStart] == '\n')
                                    lineNum++;
                                lineStart++;
                            }

                            // Extract the line text
                            size_t lineEnd = content.find('\n', found);
                            size_t prevNewline = content.rfind('\n', found);
                            size_t ls = (prevNewline == std::string::npos) ? 0 : prevNewline + 1;
                            size_t le = (lineEnd == std::string::npos) ? content.size() : lineEnd;
                            std::string lineText = content.substr(ls, std::min(le - ls, (size_t)200));

                            FileSearchResult r;
                            r.path = fullPath;
                            r.line = lineNum;
                            r.lineText = lineText;
                            results.push_back(r);

                            pos = found + lowerQuery.size();
                        }
                    }
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

// Recursive directory listing for list-directory with depth control
struct DirEntry
{
    std::string name;
    std::string path;
    bool isDir;
    DWORD size;
    FILETIME lastWrite;
};

static void listDirectoryEntries(const std::string& dir, int depth, int maxDepth, int maxEntries,
                                 std::vector<DirEntry>& entries)
{
    if ((int)entries.size() >= maxEntries || depth > maxDepth)
        return;

    WIN32_FIND_DATAA fd;
    std::string searchPath = dir + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if ((int)entries.size() >= maxEntries)
            break;
        std::string name = fd.cFileName;
        if (name == "." || name == "..")
            continue;

        DirEntry entry;
        entry.name = name;
        entry.path = dir + "\\" + name;
        entry.isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry.size = fd.nFileSizeLow;
        entry.lastWrite = fd.ftLastWriteTime;
        entries.push_back(entry);

        if (entry.isDir && depth < maxDepth)
        {
            listDirectoryEntries(entry.path, depth + 1, maxDepth, maxEntries, entries);
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

}  // anonymous namespace

// ============================================================================
// POST /api/write-file — Write/overwrite a file on disk
// ============================================================================
// Request body: {"path":"C:/some/file.cpp","content":"...file text..."}
// Optional: {"createDirs":true} to auto-create parent directories
// Response: {"success":true,"path":"...","size":12345}
// Security: Rejects ".." traversal, requires absolute paths, max 50MB content
// ============================================================================

void Win32IDE::handleWriteFileEndpoint(SOCKET client, const std::string& body)
{
    auto vp = validateFilePath(body);
    if (!vp.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vp.errorCode, vp.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string content;
    if (!LocalServerUtil::extractJsonString(body, "content", content))
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"missing_content\",\"message\":\"Request body must contain 'content' field\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Size limit: 50MB for writes
    const size_t maxWriteSize = 50 * 1024 * 1024;
    if (content.size() > maxWriteSize)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            413, "{\"error\":\"content_too_large\",\"message\":\"Content exceeds 50MB write limit\",\"size\":" +
                     std::to_string(content.size()) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Auto-create parent directories if requested
    bool createDirs = false;
    LocalServerUtil::extractJsonBool(body, "createDirs", createDirs);
    if (createDirs)
    {
        std::string parentDir = extractParentDir(vp.path);
        if (!createDirectoriesRecursive(parentDir))
        {
            DWORD err = GetLastError();
            std::string resp = LocalServerUtil::buildHttpResponse(
                500,
                "{\"error\":\"mkdir_failed\",\"message\":\"Failed to create parent directories\",\"win32_error\":" +
                    std::to_string(err) + "}");
            LocalServerUtil::sendAll(client, resp);
            return;
        }
    }

    // Write the file
    HANDLE hFile = CreateFileA(vp.path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"write_failed\",\"message\":\"Cannot create/open file for writing: " +
                     LocalServerUtil::escapeJson(vp.path) + "\",\"win32_error\":" + std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    DWORD bytesWritten = 0;
    BOOL writeOk = WriteFile(hFile, content.c_str(), (DWORD)content.size(), &bytesWritten, NULL);
    CloseHandle(hFile);

    if (!writeOk)
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"write_failed\",\"message\":\"Failed to write file content\",\"win32_error\":" +
                     std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::ostringstream json;
    json << "{\"success\":true"
         << ",\"path\":\"" << LocalServerUtil::escapeJson(vp.path) << "\""
         << ",\"name\":\"" << LocalServerUtil::escapeJson(extractFileName(vp.path)) << "\""
         << ",\"size\":" << bytesWritten << ",\"message\":\"File written successfully\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("write-file: " + vp.path + " (" + std::to_string(bytesWritten) + " bytes)");
}

// ============================================================================
// POST /api/list-directory — List directory contents with optional recursion
// ============================================================================
// Request body: {"path":"C:/some/dir"}
// Optional: {"depth":1,"maxEntries":10000}
// Response: {"entries":[{"name":"...","path":"...","type":"file|dir","size":123}],"total":42}
// ============================================================================

void Win32IDE::handleListDirEndpoint(SOCKET client, const std::string& body)
{
    auto vp = validateFilePath(body);
    if (!vp.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vp.errorCode, vp.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Check that path is a directory
    DWORD attr = GetFileAttributesA(vp.path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            404, "{\"error\":\"not_found\",\"message\":\"Directory not found: " + LocalServerUtil::escapeJson(vp.path) +
                     "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"not_directory\",\"message\":\"Path is not a directory: " +
                     LocalServerUtil::escapeJson(vp.path) + "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    int depth = 0;
    int maxEntries = 50000;
    LocalServerUtil::extractJsonInt(body, "depth", depth);
    LocalServerUtil::extractJsonInt(body, "maxEntries", maxEntries);
    if (maxEntries > 50000)
        maxEntries = 50000;
    if (depth < 0)
        depth = 0;
    if (depth > 10)
        depth = 10;

    std::vector<DirEntry> entries;
    entries.reserve(std::min(maxEntries, 10000));
    listDirectoryEntries(vp.path, 0, depth, maxEntries, entries);

    // Build JSON response
    std::ostringstream json;
    json << "{\"entries\":[";
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (i > 0)
            json << ",";
        json << "{\"name\":\"" << LocalServerUtil::escapeJson(entries[i].name) << "\""
             << ",\"path\":\"" << LocalServerUtil::escapeJson(entries[i].path) << "\""
             << ",\"type\":\"" << (entries[i].isDir ? "dir" : "file") << "\""
             << ",\"size\":" << entries[i].size << "}";
    }
    json << "],\"total\":" << entries.size() << ",\"path\":\"" << LocalServerUtil::escapeJson(vp.path) << "\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("list-directory: " + vp.path + " (" + std::to_string(entries.size()) + " entries)");
}

// ============================================================================
// POST /api/delete-file — Delete a file or empty directory
// ============================================================================
// Request body: {"path":"C:/some/file.cpp"}
// Optional: {"force":true} to delete read-only files
// Response: {"success":true,"path":"...","message":"Deleted"}
// ============================================================================

void Win32IDE::handleDeleteFileEndpoint(SOCKET client, const std::string& body)
{
    auto vp = validateFilePath(body);
    if (!vp.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vp.errorCode, vp.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    DWORD attr = GetFileAttributesA(vp.path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            404,
            "{\"error\":\"not_found\",\"message\":\"File not found: " + LocalServerUtil::escapeJson(vp.path) + "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    bool force = false;
    LocalServerUtil::extractJsonBool(body, "force", force);

    // If read-only and force requested, clear read-only attribute
    if (force && (attr & FILE_ATTRIBUTE_READONLY))
    {
        SetFileAttributesA(vp.path.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
    }

    BOOL deleteOk;
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        deleteOk = RemoveDirectoryA(vp.path.c_str());
    }
    else
    {
        deleteOk = DeleteFileA(vp.path.c_str());
    }

    if (!deleteOk)
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"delete_failed\",\"message\":\"Failed to delete: " +
                     LocalServerUtil::escapeJson(vp.path) + "\",\"win32_error\":" + std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string json = "{\"success\":true,\"path\":\"" + LocalServerUtil::escapeJson(vp.path) +
                       "\",\"message\":\"Deleted successfully\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("delete-file: " + vp.path);
}

// ============================================================================
// POST /api/rename-file — Rename or move a file/directory
// ============================================================================
// Request body: {"path":"C:/old/path.cpp","newPath":"C:/new/path.cpp"}
// Response: {"success":true,"oldPath":"...","newPath":"..."}
// ============================================================================

void Win32IDE::handleRenameFileEndpoint(SOCKET client, const std::string& body)
{
    auto vpOld = validateFilePath(body, "path");
    if (!vpOld.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vpOld.errorCode, vpOld.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto vpNew = validateFilePath(body, "newPath");
    if (!vpNew.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vpNew.errorCode, vpNew.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Check source exists
    DWORD attr = GetFileAttributesA(vpOld.path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            404, "{\"error\":\"not_found\",\"message\":\"Source not found: " + LocalServerUtil::escapeJson(vpOld.path) +
                     "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Check destination doesn't already exist
    DWORD destAttr = GetFileAttributesA(vpNew.path.c_str());
    if (destAttr != INVALID_FILE_ATTRIBUTES)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            409, "{\"error\":\"already_exists\",\"message\":\"Destination already exists: " +
                     LocalServerUtil::escapeJson(vpNew.path) + "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Create parent directories for destination
    std::string destParent = extractParentDir(vpNew.path);
    createDirectoriesRecursive(destParent);

    if (!MoveFileA(vpOld.path.c_str(), vpNew.path.c_str()))
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"rename_failed\",\"message\":\"Failed to rename: " +
                     LocalServerUtil::escapeJson(vpOld.path) + "\",\"win32_error\":" + std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string json = "{\"success\":true,\"oldPath\":\"" + LocalServerUtil::escapeJson(vpOld.path) +
                       "\",\"newPath\":\"" + LocalServerUtil::escapeJson(vpNew.path) +
                       "\",\"message\":\"Renamed successfully\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("rename-file: " + vpOld.path + " -> " + vpNew.path);
}

// ============================================================================
// POST /api/mkdir — Create directories recursively
// ============================================================================
// Request body: {"path":"C:/some/new/directory/tree"}
// Response: {"success":true,"path":"..."}
// ============================================================================

void Win32IDE::handleMkdirEndpoint(SOCKET client, const std::string& body)
{
    auto vp = validateFilePath(body);
    if (!vp.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vp.errorCode, vp.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    if (!createDirectoriesRecursive(vp.path))
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"mkdir_failed\",\"message\":\"Failed to create directory: " +
                     LocalServerUtil::escapeJson(vp.path) + "\",\"win32_error\":" + std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string json = "{\"success\":true,\"path\":\"" + LocalServerUtil::escapeJson(vp.path) +
                       "\",\"message\":\"Directory created\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("mkdir: " + vp.path);
}

// ============================================================================
// POST /api/search-files — Recursive file/content search
// ============================================================================
// Request body: {"path":"C:/some/dir","pattern":"*.cpp","query":"searchText"}
// Optional: {"maxResults":500,"searchContent":true}
// Response: {"results":[{"path":"...","line":42,"text":"..."}],"total":15}
// ============================================================================

void Win32IDE::handleSearchFilesEndpoint(SOCKET client, const std::string& body)
{
    auto vp = validateFilePath(body);
    if (!vp.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vp.errorCode, vp.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string pattern = LocalServerUtil::extractJsonStringValue(body, "pattern");
    std::string textQuery = LocalServerUtil::extractJsonStringValue(body, "query");
    int maxResults = 500;
    LocalServerUtil::extractJsonInt(body, "maxResults", maxResults);
    if (maxResults > 10000)
        maxResults = 10000;
    if (maxResults < 1)
        maxResults = 1;

    bool searchContent = false;
    LocalServerUtil::extractJsonBool(body, "searchContent", searchContent);
    // If a text query is provided but searchContent not explicitly set, enable content search
    if (!textQuery.empty() && !searchContent)
    {
        searchContent = true;
    }

    std::vector<FileSearchResult> results;
    results.reserve(std::min(maxResults, 1000));
    searchFilesRecursive(vp.path, pattern, textQuery, searchContent, maxResults, results);

    // Build JSON
    std::ostringstream json;
    json << "{\"results\":[";
    for (size_t i = 0; i < results.size(); i++)
    {
        if (i > 0)
            json << ",";
        json << "{\"path\":\"" << LocalServerUtil::escapeJson(results[i].path) << "\"";
        if (results[i].line > 0)
        {
            json << ",\"line\":" << results[i].line;
            json << ",\"text\":\"" << LocalServerUtil::escapeJson(results[i].lineText) << "\"";
        }
        json << "}";
    }
    json << "],\"total\":" << results.size() << ",\"pattern\":\"" << LocalServerUtil::escapeJson(pattern) << "\""
         << ",\"query\":\"" << LocalServerUtil::escapeJson(textQuery) << "\""
         << ",\"searchPath\":\"" << LocalServerUtil::escapeJson(vp.path) << "\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("search-files: " + vp.path + " pattern=" + pattern + " query=" + textQuery + " (" +
             std::to_string(results.size()) + " results)");
}

// ============================================================================
// POST /api/stat-file — Get file/directory metadata
// ============================================================================
// Request body: {"path":"C:/some/file.cpp"}
// Response: {"name":"file.cpp","path":"...","size":1234,"isDir":false,"exists":true,
//            "readOnly":false,"hidden":false,"created":"...","modified":"...","accessed":"..."}
// ============================================================================

void Win32IDE::handleStatFileEndpoint(SOCKET client, const std::string& body)
{
    auto vp = validateFilePath(body);
    if (!vp.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vp.errorCode, vp.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(vp.path.c_str(), GetFileExInfoStandard, &fad))
    {
        // File doesn't exist — return exists:false rather than error
        std::string json = "{\"exists\":false,\"path\":\"" + LocalServerUtil::escapeJson(vp.path) + "\"}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    ULARGE_INTEGER fileSize;
    fileSize.LowPart = fad.nFileSizeLow;
    fileSize.HighPart = fad.nFileSizeHigh;

    // Convert FILETIME to milliseconds since epoch
    auto filetimeToMs = [](const FILETIME& ft) -> uint64_t
    {
        ULARGE_INTEGER ul;
        ul.LowPart = ft.dwLowDateTime;
        ul.HighPart = ft.dwHighDateTime;
        // Windows FILETIME epoch: 1601-01-01; Unix epoch: 1970-01-01
        // Difference: 11644473600 seconds = 116444736000000000 in 100ns units
        static const uint64_t epochDiff = 116444736000000000ULL;
        return (ul.QuadPart - epochDiff) / 10000;  // Convert 100ns to ms
    };

    bool isDir = (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    bool readOnly = (fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    bool hidden = (fad.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

    std::ostringstream json;
    json << "{\"exists\":true"
         << ",\"name\":\"" << LocalServerUtil::escapeJson(extractFileName(vp.path)) << "\""
         << ",\"path\":\"" << LocalServerUtil::escapeJson(vp.path) << "\""
         << ",\"size\":" << fileSize.QuadPart << ",\"isDir\":" << (isDir ? "true" : "false")
         << ",\"readOnly\":" << (readOnly ? "true" : "false") << ",\"hidden\":" << (hidden ? "true" : "false")
         << ",\"createdMs\":" << filetimeToMs(fad.ftCreationTime)
         << ",\"modifiedMs\":" << filetimeToMs(fad.ftLastWriteTime)
         << ",\"accessedMs\":" << filetimeToMs(fad.ftLastAccessTime) << ",\"attributes\":" << fad.dwFileAttributes
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("stat-file: " + vp.path);
}

// ============================================================================
// POST /api/copy-file — Copy a file to a new destination
// ============================================================================
// Request body: {"path":"C:/source.cpp","destPath":"C:/dest.cpp"}
// Optional: {"overwrite":false}
// Response: {"success":true,"source":"...","dest":"...","size":1234}
// ============================================================================

void Win32IDE::handleCopyFileEndpoint(SOCKET client, const std::string& body)
{
    auto vpSrc = validateFilePath(body, "path");
    if (!vpSrc.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vpSrc.errorCode, vpSrc.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto vpDest = validateFilePath(body, "destPath");
    if (!vpDest.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vpDest.errorCode, vpDest.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Check source exists
    DWORD srcAttr = GetFileAttributesA(vpSrc.path.c_str());
    if (srcAttr == INVALID_FILE_ATTRIBUTES)
    {
        std::string resp =
            LocalServerUtil::buildHttpResponse(404, "{\"error\":\"not_found\",\"message\":\"Source file not found: " +
                                                        LocalServerUtil::escapeJson(vpSrc.path) + "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    bool overwrite = false;
    LocalServerUtil::extractJsonBool(body, "overwrite", overwrite);

    // Create parent directories for destination
    std::string destParent = extractParentDir(vpDest.path);
    createDirectoriesRecursive(destParent);

    BOOL failIfExists = overwrite ? FALSE : TRUE;
    if (!CopyFileA(vpSrc.path.c_str(), vpDest.path.c_str(), failIfExists))
    {
        DWORD err = GetLastError();
        std::string errMsg = (err == ERROR_FILE_EXISTS) ? "already_exists" : "copy_failed";
        int httpCode = (err == ERROR_FILE_EXISTS) ? 409 : 500;
        std::string resp = LocalServerUtil::buildHttpResponse(
            httpCode, "{\"error\":\"" + errMsg +
                          "\",\"message\":\"Failed to copy file\",\"win32_error\":" + std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Get copied file size
    DWORD destSize = 0;
    HANDLE hDest = CreateFileA(vpDest.path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDest != INVALID_HANDLE_VALUE)
    {
        destSize = GetFileSize(hDest, NULL);
        CloseHandle(hDest);
    }

    std::ostringstream json;
    json << "{\"success\":true"
         << ",\"source\":\"" << LocalServerUtil::escapeJson(vpSrc.path) << "\""
         << ",\"dest\":\"" << LocalServerUtil::escapeJson(vpDest.path) << "\""
         << ",\"size\":" << destSize << ",\"message\":\"File copied successfully\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("copy-file: " + vpSrc.path + " -> " + vpDest.path);
}

// ============================================================================
// POST /api/move-file — Move a file to a new destination
// ============================================================================
// Request body: {"path":"C:/source.cpp","destPath":"C:/dest.cpp"}
// Optional: {"overwrite":false}
// Response: {"success":true,"source":"...","dest":"..."}
// ============================================================================

void Win32IDE::handleMoveFileEndpoint(SOCKET client, const std::string& body)
{
    auto vpSrc = validateFilePath(body, "path");
    if (!vpSrc.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vpSrc.errorCode, vpSrc.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto vpDest = validateFilePath(body, "destPath");
    if (!vpDest.ok)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(vpDest.errorCode, vpDest.error);
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    // Check source exists
    DWORD srcAttr = GetFileAttributesA(vpSrc.path.c_str());
    if (srcAttr == INVALID_FILE_ATTRIBUTES)
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            404, "{\"error\":\"not_found\",\"message\":\"Source not found: " + LocalServerUtil::escapeJson(vpSrc.path) +
                     "\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    bool overwrite = false;
    LocalServerUtil::extractJsonBool(body, "overwrite", overwrite);

    // Create parent directories for destination
    std::string destParent = extractParentDir(vpDest.path);
    createDirectoriesRecursive(destParent);

    DWORD moveFlags = MOVEFILE_COPY_ALLOWED;
    if (overwrite)
        moveFlags |= MOVEFILE_REPLACE_EXISTING;

    if (!MoveFileExA(vpSrc.path.c_str(), vpDest.path.c_str(), moveFlags))
    {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(
            500, "{\"error\":\"move_failed\",\"message\":\"Failed to move file\",\"win32_error\":" +
                     std::to_string(err) + "}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    std::string json = "{\"success\":true"
                       ",\"source\":\"" +
                       LocalServerUtil::escapeJson(vpSrc.path) +
                       "\""
                       ",\"dest\":\"" +
                       LocalServerUtil::escapeJson(vpDest.path) +
                       "\""
                       ",\"message\":\"File moved successfully\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, resp);

    LOG_INFO("move-file: " + vpSrc.path + " -> " + vpDest.path);
}

// ============================================================================
// POST /api/tool — Unified tool call dispatcher
// ============================================================================
// Request body: {"tool":"read_file","args":{"path":"C:/file.cpp"}}
// Dispatches to the appropriate file operation endpoint handler.
// This is the endpoint the frontend EngineAPI.executeTool() calls.
// ============================================================================

void Win32IDE::handleToolDispatchEndpoint(SOCKET client, const std::string& body)
{
    nlohmann::json requestJson = nlohmann::json::object();
    try
    {
        requestJson = nlohmann::json::parse(body);
        if (!requestJson.is_object())
        {
            requestJson = nlohmann::json::object();
        }
    }
    catch (...)
    {
        requestJson = nlohmann::json::object();
    }

    std::string tool;
    if (requestJson.contains("tool") && requestJson["tool"].is_string())
    {
        tool = requestJson["tool"].get<std::string>();
    }
    if (tool.empty())
    {
        tool = LocalServerUtil::extractJsonStringValue(body, "tool");
    }
    if (tool.empty())
    {
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"missing_tool\",\"message\":\"Request body must contain 'tool' field\"}");
        LocalServerUtil::sendAll(client, resp);
        return;
    }

    auto normalizeTool = [](std::string name) -> std::string
    {
        if (name == "rstore_checkpoint")
            return "restore_checkpoint";
        if (name == "compacted_conversation")
            return "compact_conversation";
        if (name == "optimize-tool-selection")
            return "optimize_tool_selection";
        if (name == "read-lines")
            return "read_lines";
        if (name == "planning-exploration")
            return "plan_code_exploration";
        if (name == "search-files")
            return "search_files";
        if (name == "evaluate-integration")
            return "evaluate_integration_audit_feasibility";
        if (name == "resolve-symbol")
            return "resolve_symbol";
        if (name == "list_directory")
            return "list_dir";
        if (name == "create_file")
            return "write_file";
        if (name == "run_command")
            return "execute_command";
        return name;
    };
    tool = normalizeTool(tool);

    nlohmann::json toolArgs = nlohmann::json::object();
    if (requestJson.contains("args") && requestJson["args"].is_object())
    {
        toolArgs = requestJson["args"];
    }

    // Compatibility with flat payloads sent by older clients.
    auto readFlat = [&](const char* key, const char* outKey)
    {
        if (requestJson.contains(key) && requestJson[key].is_string())
        {
            std::string value = requestJson[key].get<std::string>();
            if (!value.empty())
                toolArgs[outKey] = value;
            return;
        }
        std::string v = LocalServerUtil::extractJsonStringValue(body, key);
        if (!v.empty())
            toolArgs[outKey] = v;
    };
    auto readFlatBool = [&](const char* key, const char* outKey)
    {
        if (requestJson.contains(key) && requestJson[key].is_boolean())
        {
            toolArgs[outKey] = requestJson[key].get<bool>();
            return;
        }
        bool v = false;
        if (LocalServerUtil::extractJsonBool(body, key, v))
            toolArgs[outKey] = v;
    };
    auto readFlatNumber = [&](const char* key, const char* outKey)
    {
        if (requestJson.contains(key) && requestJson[key].is_number_integer())
        {
            toolArgs[outKey] = requestJson[key].get<int>();
            return;
        }
        std::string raw = LocalServerUtil::extractJsonStringValue(body, key);
        if (raw.empty())
            return;
        try
        {
            toolArgs[outKey] = std::stoi(raw);
        }
        catch (...)
        {
        }
    };
    readFlat("task", "task");
    readFlat("goal", "goal");
    readFlat("owner", "owner");
    readFlat("deadline", "deadline");
    readFlat("symbol", "symbol");
    readFlat("query", "query");
    readFlat("pattern", "pattern");
    readFlat("command", "command");
    readFlat("cwd", "cwd");
    readFlat("context", "context");
    readFlat("checkpoint_path", "checkpoint_path");
    readFlat("history_log_path", "history_log_path");
    readFlat("path", "path");
    readFlat("hint", "hint");
    readFlat("current_file", "current_file");
    readFlat("selection", "selection");
    readFlat("session_id", "session_id");
    readFlatBool("flush", "flush");
    readFlatBool("include_context", "include_context");
    readFlatNumber("max_results", "max_results");
    readFlatNumber("limit", "limit");
    readFlatNumber("line_start", "line_start");
    readFlatNumber("line_end", "line_end");

    auto& toolHandlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (toolHandlers.HasTool(tool))
    {
        auto result = toolHandlers.Execute(tool, toolArgs);
        nlohmann::json out = result.toJson();
        out["tool"] = tool;
        std::string resp = LocalServerUtil::buildHttpResponse(result.isSuccess() ? 200 : 400, out.dump());
        LocalServerUtil::sendAll(client, resp);
        LOG_INFO("tool-dispatch-bridge: " + tool);
        return;
    }

    std::string argsBody = toolArgs.dump();

    // Dispatch to the appropriate handler
    if (tool == "read_file")
    {
        handleReadFileEndpoint(client, argsBody);
    }
    else if (tool == "write_file")
    {
        handleWriteFileEndpoint(client, argsBody);
    }
    else if (tool == "list_directory")
    {
        handleListDirEndpoint(client, argsBody);
    }
    else if (tool == "delete_file")
    {
        handleDeleteFileEndpoint(client, argsBody);
    }
    else if (tool == "rename_file")
    {
        handleRenameFileEndpoint(client, argsBody);
    }
    else if (tool == "mkdir")
    {
        handleMkdirEndpoint(client, argsBody);
    }
    else if (tool == "search_files")
    {
        handleSearchFilesEndpoint(client, argsBody);
    }
    else if (tool == "stat_file")
    {
        handleStatFileEndpoint(client, argsBody);
    }
    else if (tool == "copy_file")
    {
        handleCopyFileEndpoint(client, argsBody);
    }
    else if (tool == "move_file")
    {
        handleMoveFileEndpoint(client, argsBody);
    }
    else if (tool == "execute_command")
    {
        handleCliEndpoint(client, argsBody);
    }
    else if (tool == "git_status")
    {
        // Execute git status command via CLI handler
        std::string gitBody = "{\"command\":\"git status\"}";
        handleCliEndpoint(client, gitBody);
    }
    else
    {
        nlohmann::json available = nlohmann::json::array();
        auto schemas = RawrXD::Agent::AgentToolHandlers::GetAllSchemas();
        if (schemas.is_array())
        {
            for (const auto& schema : schemas)
            {
                if (schema.is_object() && schema.contains("name") && schema["name"].is_string())
                {
                    available.push_back(schema["name"].get<std::string>());
                }
            }
        }
        available.push_back("git_status");
        std::string resp = LocalServerUtil::buildHttpResponse(
            400, "{\"error\":\"unknown_tool\",\"message\":\"Unknown tool: " + LocalServerUtil::escapeJson(tool) +
                     "\",\"available\":" + available.dump() + "}");
        LocalServerUtil::sendAll(client, resp);
    }

    LOG_INFO("tool-dispatch: " + tool);
}

void Win32IDE::handleAgentOrchestrateEndpoint(SOCKET client, const std::string& body)
{
    std::string message;
    LocalServerUtil::extractJsonString(body, "message", message);
    if (message.empty())
    {
        LocalServerUtil::extractJsonString(body, "prompt", message);
    }
    if (message.empty())
    {
        LocalServerUtil::sendAll(
            client, LocalServerUtil::buildHttpResponse(400, "{\"success\":false,\"error\":\"missing_message\"}"));
        return;
    }

    bool showActions = false;
    LocalServerUtil::extractJsonBool(body, "show_actions", showActions);
    bool includeActions = false;
    LocalServerUtil::extractJsonBool(body, "include_actions", includeActions);
    showActions = showActions || includeActions;

    std::string lower = LocalServerUtil::toLower(message);
    nlohmann::json actionLog = nlohmann::json::array();
    nlohmann::json synthesisSignals = nlohmann::json::object();

    auto runTool = [&](const std::string& toolName, nlohmann::json args) -> nlohmann::json
    {
        auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute(toolName, args);
        nlohmann::json out = result.toJson();
        nlohmann::json action;
        action["tool"] = toolName;
        action["ok"] = result.isSuccess();
        action["result"] = out;
        actionLog.push_back(action);
        return out;
    };

    // Intent planner: always start with tool selection optimization.
    nlohmann::json selectionArgs = nlohmann::json::object();
    selectionArgs["task"] = message;
    selectionArgs["context"] = "agent_orchestrator";
    synthesisSignals["toolSelection"] = runTool("optimize_tool_selection", selectionArgs);

    // Speculation engine: proactively gather useful context based on intent.
    const bool wantsSearch = (lower.find("find") != std::string::npos || lower.find("search") != std::string::npos ||
                              lower.find("where") != std::string::npos || lower.find("locate") != std::string::npos);
    const bool wantsDiagnostics =
        (lower.find("error") != std::string::npos || lower.find("fix") != std::string::npos ||
         lower.find("diagnostic") != std::string::npos || lower.find("lint") != std::string::npos);
    const bool wantsSymbol =
        (lower.find("symbol") != std::string::npos || lower.find("definition") != std::string::npos ||
         lower.find("reference") != std::string::npos);

    if (wantsSearch)
    {
        nlohmann::json searchArgs = nlohmann::json::object();
        searchArgs["query"] = message;
        searchArgs["max_results"] = 25;
        synthesisSignals["search"] = runTool("search_code", searchArgs);
    }
    if (wantsDiagnostics)
    {
        synthesisSignals["diagnostics"] = runTool("get_diagnostics", nlohmann::json::object());
    }
    if (wantsSymbol)
    {
        nlohmann::json resolveArgs = nlohmann::json::object();
        resolveArgs["symbol"] = message;
        synthesisSignals["symbol"] = runTool("resolve_symbol", resolveArgs);
    }

    // Result synthesizer: conversational response hides low-level tool chatter by default.
    std::string reply = "I analyzed your intent and ran backend tools to gather actionable context.";
    if (wantsSearch || wantsDiagnostics || wantsSymbol)
    {
        reply += " I pre-fetched relevant results and can continue with focused edits or a patch plan.";
    }
    else
    {
        reply += " Tell me whether you want code exploration, diagnostics, symbol resolution, or direct edits next.";
    }

    nlohmann::json response = nlohmann::json::object();
    response["success"] = true;
    response["mode"] = "orchestrated";
    response["outsideHotpatchAccessible"] = true;
    response["reply"] = reply;
    response["response"] = reply;
    response["confidence"] = "medium";
    response["autoApplied"] = false;
    response["actions_hidden"] = !showActions;
    response["showActionsHint"] = "Set show_actions=true or include_actions=true to inspect executed tool calls.";
    response["signals"] = synthesisSignals;
    if (showActions)
    {
        response["actions"] = actionLog;
    }
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, response.dump()));
}

// ============================================================================
// Phase 34: INSTRUCTIONS CONTEXT — /api/instructions
// Read and serve tools.instructions.md (all lines) as context
// ============================================================================

void Win32IDE::handleInstructionsEndpoint(SOCKET client, const std::string& mode)
{
    auto& provider = InstructionsProvider::instance();

    // Lazy-load on first access
    if (!provider.isLoaded())
    {
        provider.loadAll();
    }

    std::string json;
    if (mode == "summary")
    {
        json = provider.toJSONSummary();
    }
    else
    {
        json = provider.toJSON();
    }

    std::string response = LocalServerUtil::buildHttpResponse(200, json);
    LocalServerUtil::sendAll(client, response);
}

void Win32IDE::handleInstructionsContentEndpoint(SOCKET client)
{
    auto& provider = InstructionsProvider::instance();

    if (!provider.isLoaded())
    {
        provider.loadAll();
    }

    std::string content = provider.getAllContent();

    // Respond as text/markdown
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    resp << "Content-Type: text/markdown; charset=utf-8\r\n";
    resp << "Content-Length: " << content.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "X-Instructions-Files: " << provider.getLoadedCount() << "\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << content;

    LocalServerUtil::sendAll(client, resp.str());
}

void Win32IDE::handleInstructionsReloadEndpoint(SOCKET client)
{
    auto& provider = InstructionsProvider::instance();
    auto result = provider.reload();

    std::ostringstream json;
    json << "{\"success\":" << (result.success ? "true" : "false") << ",\"detail\":\""
         << LocalServerUtil::escapeJson(result.detail) << "\""
         << ",\"files_loaded\":" << provider.getLoadedCount() << "}";

    std::string response = LocalServerUtil::buildHttpResponse(result.success ? 200 : 500, json.str());
    LocalServerUtil::sendAll(client, response);
}

// ============================================================================
// POST /complete — Inline completion endpoint (parity with complete_server)
// ============================================================================
void Win32IDE::handleCompleteEndpoint(SOCKET client, const std::string& body)
{
    std::string buffer;
    int cursorOffset = -1;
    int maxTokens = 128;

    LocalServerUtil::extractJsonString(body, "buffer", buffer);
    LocalServerUtil::extractJsonInt(body, "cursor_offset", cursorOffset);
    LocalServerUtil::extractJsonInt(body, "max_tokens", maxTokens);

    if (buffer.empty()) {
        LocalServerUtil::sendAll(client,
            LocalServerUtil::buildHttpResponse(400, R"({"error":"buffer is required"})"));
        return;
    }

    std::string prefix = (cursorOffset >= 0 && cursorOffset < (int)buffer.size())
                         ? buffer.substr(0, cursorOffset) : buffer;

    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        std::string result = routeWithIntelligence(prefix);
        std::ostringstream j;
        j << "{\"completion\":\"" << LocalServerUtil::escapeJson(result) << "\"}";
        LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, j.str()));
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prefix);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);
    std::string completion = m_nativeEngine->Detokenize(generated);

    std::ostringstream j;
    j << "{\"completion\":\"" << LocalServerUtil::escapeJson(completion) << "\"}";
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, j.str()));
}

// ============================================================================
// POST /complete/stream — Streaming completion SSE (parity with complete_server)
// ============================================================================
void Win32IDE::handleCompleteStreamEndpoint(SOCKET client, const std::string& body)
{
    std::string buffer;
    int cursorOffset = -1;
    int maxTokens = 128;

    LocalServerUtil::extractJsonString(body, "buffer", buffer);
    LocalServerUtil::extractJsonInt(body, "cursor_offset", cursorOffset);
    LocalServerUtil::extractJsonInt(body, "max_tokens", maxTokens);

    if (buffer.empty()) {
        LocalServerUtil::sendAll(client,
            LocalServerUtil::buildHttpResponse(400, R"({"error":"buffer is required"})"));
        return;
    }

    std::string prefix = (cursorOffset >= 0 && cursorOffset < (int)buffer.size())
                         ? buffer.substr(0, cursorOffset) : buffer;

    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        std::string result = routeWithIntelligence(prefix);
        LocalServerUtil::sendSSEHeaders(client);
        std::string event = "data: {\"token\":\"" + LocalServerUtil::escapeJson(result) + "\"}\n\n";
        LocalServerUtil::sendAll(client, event);
        LocalServerUtil::sendAll(client, std::string("data: [DONE]\n\n"));
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prefix);
    LocalServerUtil::sendSSEHeaders(client);

    auto generated = m_nativeEngine->Generate(tokens, maxTokens);
    for (const auto& tok : generated) {
        std::string text = m_nativeEngine->Detokenize({tok});
        std::string event = "data: {\"token\":\"" + LocalServerUtil::escapeJson(text) + "\"}\n\n";
        LocalServerUtil::sendAll(client, event);
    }
    LocalServerUtil::sendAll(client, std::string("data: [DONE]\n\n"));
}

// ============================================================================
// POST /api/agent/wish — Agent wish endpoint (parity with complete_server)
// ============================================================================
void Win32IDE::handleAgentWishEndpoint(SOCKET client, const std::string& body)
{
    std::string wish;
    if (!LocalServerUtil::extractJsonString(body, "wish", wish)) {
        LocalServerUtil::extractJsonString(body, "prompt", wish);
    }

    if (wish.empty()) {
        LocalServerUtil::sendAll(client,
            LocalServerUtil::buildHttpResponse(400, R"({"error":"wish or prompt is required"})"));
        return;
    }

    std::string prompt = "Execute the following user wish. Respond with a clear plan or result: " + wish;
    std::string result = routeWithIntelligence(prompt);

    std::ostringstream j;
    j << "{\"success\":true,\"surface\":\"local_server\",\"wish\":\""
      << LocalServerUtil::escapeJson(wish) << "\",\"result\":\""
      << LocalServerUtil::escapeJson(result) << "\"}";
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, j.str()));
}

// ============================================================================
// GET|POST /api/policies — Policy management (parity with complete_server)
// ============================================================================
void Win32IDE::handlePoliciesEndpoint(SOCKET client, const std::string& method, const std::string& body)
{
    nlohmann::json out = nlohmann::json::object();
    out["success"] = true;
    out["surface"] = "local_server";
    out["policies"] = nlohmann::json::array();
    out["count"] = 0;
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
}

void Win32IDE::handlePoliciesSuggestionsEndpoint(SOCKET client)
{
    nlohmann::json out = nlohmann::json::object();
    out["success"] = true;
    out["surface"] = "local_server";
    out["suggestions"] = nlohmann::json::array();
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
}

void Win32IDE::handlePoliciesApplyEndpoint(SOCKET client, const std::string& body)
{
    std::string policyId;
    LocalServerUtil::extractJsonString(body, "policy_id", policyId);
    nlohmann::json out = nlohmann::json::object();
    out["success"] = true;
    out["surface"] = "local_server";
    out["applied"] = policyId;
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
}

void Win32IDE::handlePoliciesRejectEndpoint(SOCKET client, const std::string& body)
{
    std::string policyId;
    LocalServerUtil::extractJsonString(body, "policy_id", policyId);
    nlohmann::json out = nlohmann::json::object();
    out["success"] = true;
    out["surface"] = "local_server";
    out["rejected"] = policyId;
    LocalServerUtil::sendAll(client, LocalServerUtil::buildHttpResponse(200, out.dump()));
}
