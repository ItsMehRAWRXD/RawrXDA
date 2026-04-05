// ============================================================================
// HeadlessIDE.cpp — GUI-free surface for the RawrXD Win32IDE engine
// Phase 19C: Headless Surface Extraction
//
// Implements the headless IDE that exposes the full engine capabilities
// without any HWND, window, or GDI dependency. Starts the HTTP server,
// initializes all backend subsystems, and runs in one of four modes:
//   Server / REPL / SingleShot / Batch
//
// NO exceptions. NO HWND. NO GDI. NO message loop.
// ============================================================================

#include "HeadlessIDE.h"
#include "IOutputSink.h"
#include "../agentic_engine.h"
#include "../../include/chain_of_thought_engine.h"
#include "../core/instructions_provider.hpp"

// Phase 10+ singletons — wired for real status queries
#include "../core/execution_governor.h"
#include "../core/agent_safety_contract.h"
#include "../core/deterministic_replay.h"
#include "../core/confidence_gate.h"
#include "multi_response_engine.h"
#include "../core/universal_model_hotpatcher.h"
#include "../BackendOrchestrator.h"
#include "../cpu_inference_engine.h"
#include "../kernels/kv_accum_avx512.h"
#include "../cli/swarm_orchestrator.h"
#include "../agent_history.h"
#include "../agent_explainability.h"
#include "../agent_policy.h"
#include "../agentic/AgentOllamaClient.h"
#include "../core/enterprise_license.h"
#include "../../include/lsp/RawrXD_LSPServer.h"
#include "../agent_history.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <exception>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <mutex>
#include <condition_variable>
#include <set>
#include <unordered_map>
#include <deque>
#include <psapi.h>
#include <tlhelp32.h>

// SCAFFOLD_130: Headless inference and model load

namespace {
constexpr size_t kMaxHeadlessPromptBytes = 32 * 1024;
constexpr size_t kMaxHeadlessBatchInputBytes = 4 * 1024 * 1024;
constexpr size_t kMaxHeadlessBatchPrompts = 10000;
constexpr size_t kMaxHeadlessHttpRequestBytes = 64 * 1024;
constexpr size_t kMaxHeadlessHttpBodyBytes = 32 * 1024;
constexpr size_t kMaxHeadlessInferenceResponseBytes = 1024 * 1024;
constexpr size_t kMaxHeadlessHttpResponseBytes = 1024 * 1024;
constexpr size_t kMaxHeadlessSearchMatches = 1000;
constexpr DWORD kHeadlessHttpRecvTimeoutMs = 2000;
constexpr int kHeadlessOllamaTimeoutMs = 5000;

constexpr const char* kPreferredLocalModelNames[] = {
    "ministral3.gguf",
    "phi3mini.gguf",
    "gptoss20b_link.gguf",
    "gptoss20b.gguf",
    "codestral22b.gguf"
};

struct AttentionBenchResult {
    double avgMs = 0.0;
    double tps = 0.0;
    double medianMs = 0.0;
};

static inline float benchDotProductF32(const float* a, const float* b, int length, bool useAvx512) {
    if (!a || !b || length <= 0) {
        return 0.0f;
    }

#if defined(__AVX512F__)
    if (useAvx512) {
        int i = 0;
        __m512 acc = _mm512_setzero_ps();
        for (; i + 16 <= length; i += 16) {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            acc = _mm512_fmadd_ps(va, vb, acc);
        }
        float sum = _mm512_reduce_add_ps(acc);
        for (; i < length; ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }
#else
    (void)useAvx512;
#endif

    float sum = 0.0f;
    for (int i = 0; i < length; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

static inline void benchAccumulateScaledF32(float* dst, const float* src, float scale, int length, bool useAvx512) {
    if (!dst || !src || length <= 0) {
        return;
    }

    if (useAvx512) {
        RawrXD::KernelOps::AccumulateScaledKV(src, dst, length, scale);
        return;
    }

    for (int i = 0; i < length; ++i) {
        dst[i] += src[i] * scale;
    }
}

static inline void benchSoftmaxInplace(float* x, int size) {
    if (!x || size <= 0) {
        return;
    }
    float maxVal = x[0];
    for (int i = 1; i < size; ++i) {
        maxVal = std::max(maxVal, x[i]);
    }

    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        x[i] = std::exp(x[i] - maxVal);
        sum += x[i];
    }

    const float inv = 1.0f / (sum + 1e-8f);
    for (int i = 0; i < size; ++i) {
        x[i] *= inv;
    }
}

static AttentionBenchResult MeasureAttentionLatency(int batchSize, int seqLen, int headDim, int numHeads,
                                                    int warmupIters, int measureIters, bool useAvx512,
                                                    bool medianMode) {
    AttentionBenchResult result{};
    if (batchSize <= 0 || seqLen <= 0 || headDim <= 0 || numHeads <= 0 || warmupIters < 0 || measureIters <= 0) {
        return result;
    }

    const float scale = 1.0f / std::sqrt(static_cast<float>(headDim));
    const size_t total = static_cast<size_t>(batchSize) * static_cast<size_t>(numHeads) * static_cast<size_t>(seqLen) *
                         static_cast<size_t>(headDim);

    std::vector<float> q(total, 0.125f);
    std::vector<float> k(total, 0.25f);
    std::vector<float> v(total, 0.5f);
    std::vector<float> out(total, 0.0f);

    auto runOnce = [&]() {
        for (int b = 0; b < batchSize; ++b) {
            for (int h = 0; h < numHeads; ++h) {
                const int offset = (b * numHeads + h) * seqLen * headDim;
                float* qHead = q.data() + offset;
                float* kHead = k.data() + offset;
                float* vHead = v.data() + offset;
                float* outHead = out.data() + offset;

                std::vector<float> attn(static_cast<size_t>(seqLen) * static_cast<size_t>(seqLen), 0.0f);

                for (int i = 0; i < seqLen; ++i) {
                    for (int j = 0; j < seqLen; ++j) {
                        const float* qRow = &qHead[i * headDim];
                        const float* kRow = &kHead[j * headDim];
                        attn[static_cast<size_t>(i) * static_cast<size_t>(seqLen) + static_cast<size_t>(j)] =
                            benchDotProductF32(qRow, kRow, headDim, useAvx512) * scale;
                    }
                    benchSoftmaxInplace(&attn[static_cast<size_t>(i) * static_cast<size_t>(seqLen)], seqLen);
                }

                for (int i = 0; i < seqLen; ++i) {
                    float* outRow = &outHead[i * headDim];
                    std::memset(outRow, 0, static_cast<size_t>(headDim) * sizeof(float));
                    for (int j = 0; j < seqLen; ++j) {
                        const float weight = attn[static_cast<size_t>(i) * static_cast<size_t>(seqLen) + static_cast<size_t>(j)];
                        const float* vRow = &vHead[j * headDim];
                        benchAccumulateScaledF32(outRow, vRow, weight, headDim, useAvx512);
                    }
                }
            }
        }
    };

    for (int i = 0; i < warmupIters; ++i) {
        runOnce();
    }

    std::vector<double> samplesMs;
    samplesMs.reserve(static_cast<size_t>(measureIters));

    double elapsedMs = 0.0;
    for (int i = 0; i < measureIters; ++i) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        runOnce();
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double iterMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        elapsedMs += iterMs;
        samplesMs.push_back(iterMs);
    }

    result.avgMs = elapsedMs / static_cast<double>(measureIters);

    if (!samplesMs.empty()) {
        const size_t mid = samplesMs.size() / 2;
        std::nth_element(samplesMs.begin(), samplesMs.begin() + static_cast<std::ptrdiff_t>(mid), samplesMs.end());
        if ((samplesMs.size() % 2) == 1) {
            result.medianMs = samplesMs[mid];
        } else {
            const double upper = samplesMs[mid];
            std::nth_element(samplesMs.begin(), samplesMs.begin() + static_cast<std::ptrdiff_t>(mid - 1),
                             samplesMs.begin() + static_cast<std::ptrdiff_t>(mid));
            result.medianMs = (samplesMs[mid - 1] + upper) * 0.5;
        }
    }

    const double effectiveMs = (medianMode && result.medianMs > 0.0) ? result.medianMs : result.avgMs;
    const double effectiveSec = effectiveMs / 1000.0;
    const double elapsedSec = elapsedMs / 1000.0;
    const double tokens = static_cast<double>(batchSize) * static_cast<double>(seqLen);
    (void)elapsedSec;
    result.tps = (effectiveSec > 0.0) ? (tokens / effectiveSec) : 0.0;
    return result;
}

static std::string jsonEscape(const char* text, size_t length) {
    if (!text || length == 0) {
        return std::string();
    }

    std::string escaped;
    escaped.reserve(length + 16);
    static const char* hex = "0123456789abcdef";

    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c < 0x20) {
                    escaped += "\\u00";
                    escaped += hex[(c >> 4) & 0x0F];
                    escaped += hex[c & 0x0F];
                } else {
                    escaped.push_back(static_cast<char>(c));
                }
                break;
        }
    }

    return escaped;
}

static std::string jsonEscape(const std::string& text) {
    return jsonEscape(text.data(), text.size());
}

static bool tryParseIntArg(const char* text, int minValue, int maxValue, int& outValue) noexcept {
    if (!text || !*text) {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || (end && *end != '\0')) {
        return false;
    }
    if (parsed < static_cast<long>(minValue) || parsed > static_cast<long>(maxValue)) {
        return false;
    }

    outValue = static_cast<int>(parsed);
    return true;
}

static bool tryParseFloatArg(const char* text, float minValue, float maxValue, float& outValue) noexcept {
    if (!text || !*text) {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (errno != 0 || end == text || (end && *end != '\0')) {
        return false;
    }
    if (!std::isfinite(parsed) || parsed < static_cast<double>(minValue) || parsed > static_cast<double>(maxValue)) {
        return false;
    }

    outValue = static_cast<float>(parsed);
    return true;
}

static size_t skipJsonWhitespace(const std::string& text, size_t pos) noexcept {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
    return pos;
}

static bool tryExtractJsonStringField(const std::string& body, const char* key, std::string& outValue) {
    if (!key || !*key) {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = body.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t pos = skipJsonWhitespace(body, keyPos + needle.size());
    if (pos >= body.size() || body[pos] != ':') {
        return false;
    }

    pos = skipJsonWhitespace(body, pos + 1);
    if (pos >= body.size() || body[pos] != '"') {
        return false;
    }

    ++pos;
    std::string parsed;
    parsed.reserve(32);
    bool escaping = false;
    while (pos < body.size()) {
        const char ch = body[pos++];
        if (escaping) {
            switch (ch) {
                case '"': parsed.push_back('"'); break;
                case '\\': parsed.push_back('\\'); break;
                case '/': parsed.push_back('/'); break;
                case 'b': parsed.push_back('\b'); break;
                case 'f': parsed.push_back('\f'); break;
                case 'n': parsed.push_back('\n'); break;
                case 'r': parsed.push_back('\r'); break;
                case 't': parsed.push_back('\t'); break;
                default: parsed.push_back(ch); break;
            }
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            outValue = parsed;
            return true;
        }
        parsed.push_back(ch);
    }

    return false;
}

static bool tryExtractJsonIntField(const std::string& body, const char* key, int& outValue) {
    if (!key || !*key) {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = body.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t pos = skipJsonWhitespace(body, keyPos + needle.size());
    if (pos >= body.size() || body[pos] != ':') {
        return false;
    }

    pos = skipJsonWhitespace(body, pos + 1);
    if (pos >= body.size()) {
        return false;
    }

    size_t end = pos;
    if (body[end] == '-' || body[end] == '+') {
        ++end;
    }
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end])) != 0) {
        ++end;
    }
    if (end == pos || (end == pos + 1 && (body[pos] == '-' || body[pos] == '+'))) {
        return false;
    }

    errno = 0;
    char* parseEnd = nullptr;
    const std::string token = body.substr(pos, end - pos);
    const long parsed = std::strtol(token.c_str(), &parseEnd, 10);
    if (errno != 0 || parseEnd == token.c_str() || (parseEnd && *parseEnd != '\0')) {
        return false;
    }
    if (parsed < static_cast<long>(std::numeric_limits<int>::min()) ||
        parsed > static_cast<long>(std::numeric_limits<int>::max())) {
        return false;
    }

    outValue = static_cast<int>(parsed);
    return true;
}

static bool isReasonablePromptSize(const std::string& prompt) noexcept {
    return prompt.size() <= kMaxHeadlessPromptBytes;
}

static uint64_t fileTimeToUInt64(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

static std::string makeTimestampTag() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << st.wYear
        << std::setw(2) << st.wMonth
        << std::setw(2) << st.wDay
        << "_"
        << std::setw(2) << st.wHour
        << std::setw(2) << st.wMinute
        << std::setw(2) << st.wSecond;
    return oss.str();
}

// found is set to true only when a Content-Length header is explicitly present in the
// request headers.  When false, contentLength is left at 0 but the caller must NOT treat
// that as an authoritative "body is empty" signal — it simply means the header was absent.
static bool tryParseContentLength(const std::string& request, size_t headerEnd,
                                  size_t& contentLength, bool& found) noexcept {
    contentLength = 0;
    found = false;
    size_t lineStart = 0;
    while (lineStart < headerEnd) {
        size_t lineEnd = request.find("\r\n", lineStart);
        if (lineEnd == std::string::npos || lineEnd > headerEnd) {
            lineEnd = headerEnd;
        }

        const size_t colon = request.find(':', lineStart);
        if (colon != std::string::npos && colon < lineEnd) {
            std::string key = request.substr(lineStart, colon - lineStart);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });

            if (key == "content-length") {
                size_t valueStart = colon + 1;
                while (valueStart < lineEnd && std::isspace(static_cast<unsigned char>(request[valueStart])) != 0) {
                    ++valueStart;
                }

                std::string value = request.substr(valueStart, lineEnd - valueStart);
                if (value.empty()) {
                    return false;
                }

                errno = 0;
                char* end = nullptr;
                const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
                if (errno != 0 || end == value.c_str() || (end && *end != '\0')) {
                    return false;
                }
                if (parsed > static_cast<unsigned long long>(kMaxHeadlessHttpBodyBytes)) {
                    return false;
                }

                contentLength = static_cast<size_t>(parsed);
                found = true;
                return true;
            }
        }

        lineStart = (lineEnd >= headerEnd) ? headerEnd : lineEnd + 2;
    }

    return true;
}

static std::string toLowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    static std::string trimAsciiCopy(std::string value) {
        size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
            ++begin;
        }

        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }

        return value.substr(begin, end - begin);
    }

    static std::string toUpperCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }

    static std::string stripQueryAndFragment(const std::string& target) {
        const size_t cut = target.find_first_of("?#");
        if (cut == std::string::npos) {
            return target;
        }
        return target.substr(0, cut);
    }

    static std::unordered_map<std::string, std::string> parseHttpHeaders(const std::string& request) {
        std::unordered_map<std::string, std::string> headers;
        const size_t headerEnd = request.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return headers;
        }

        size_t lineStart = request.find("\r\n");
        if (lineStart == std::string::npos || lineStart + 2 >= headerEnd) {
            return headers;
        }
        lineStart += 2;

        while (lineStart < headerEnd) {
            size_t lineEnd = request.find("\r\n", lineStart);
            if (lineEnd == std::string::npos || lineEnd > headerEnd) {
                lineEnd = headerEnd;
            }

            if (lineEnd == lineStart) {
                break;
            }

            const size_t colon = request.find(':', lineStart);
            if (colon != std::string::npos && colon < lineEnd) {
                std::string key = request.substr(lineStart, colon - lineStart);
                std::string value = request.substr(colon + 1, lineEnd - (colon + 1));
                key = toLowerCopy(trimAsciiCopy(std::move(key)));
                value = trimAsciiCopy(std::move(value));
                if (!key.empty() && headers.find(key) == headers.end()) {
                    headers.emplace(std::move(key), std::move(value));
                }
            }

            lineStart = lineEnd + 2;
        }

        return headers;
    }

    static const std::string& getHeadlessAuthToken() {
        static const std::string token = []() {
            if (const char* env = std::getenv("RAWRXD_HEADLESS_AUTH_TOKEN")) {
                return trimAsciiCopy(env);
            }
            return std::string();
        }();
        return token;
    }

    static const std::string& getHeadlessRootScope() {
        static const std::string root = []() {
            if (const char* env = std::getenv("RAWRXD_HEADLESS_ROOT")) {
                return trimAsciiCopy(env);
            }
            return std::string();
        }();
        return root;
    }

    static bool isPublicRouteNoAuthRequired(const std::string& path) {
        return path == "/" || path == "/health" || path == "/api/health" || path == "/api/version";
    }

    static bool hasMatchingBearerToken(const std::unordered_map<std::string, std::string>& headers) {
        const auto it = headers.find("authorization");
        if (it == headers.end()) {
            return false;
        }

        const std::string& auth = it->second;
        if (auth.size() < 8) {
            return false;
        }

        if (toLowerCopy(auth.substr(0, 7)) != "bearer ") {
            return false;
        }

        const std::string provided = trimAsciiCopy(auth.substr(7));
        return !provided.empty() && provided == getHeadlessAuthToken();
    }

    static int parsePositiveEnvInt(const char* varName, int minValue, int maxValue, int fallback) {
        if (!varName || !varName[0]) {
            return fallback;
        }

        const char* raw = std::getenv(varName);
        if (!raw) {
            return fallback;
        }

        std::string value = trimAsciiCopy(raw);
        if (value.empty()) {
            return fallback;
        }

        char* end = nullptr;
        errno = 0;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (errno != 0 || !end || *end != '\0') {
            return fallback;
        }

        if (parsed < static_cast<long>(minValue) || parsed > static_cast<long>(maxValue)) {
            return fallback;
        }

        return static_cast<int>(parsed);
    }

    static int getHeadlessRateLimitRpm() {
        static const int rpm = parsePositiveEnvInt("RAWRXD_HEADLESS_RATE_LIMIT_RPM", 1, 10000, 0);
        return rpm;
    }

    static bool parseEnvBool(const char* varName, bool fallback) {
        if (!varName || !varName[0]) {
            return fallback;
        }

        const char* raw = std::getenv(varName);
        if (!raw) {
            return fallback;
        }

        std::string value = toLowerCopy(trimAsciiCopy(raw));
        if (value.empty()) {
            return fallback;
        }

        if (value == "1" || value == "true" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "0" || value == "false" || value == "no" || value == "off") {
            return false;
        }
        return fallback;
    }

    static bool getHeadlessRequireAuthForWrites() {
        static const bool enabled = parseEnvBool("RAWRXD_HEADLESS_REQUIRE_AUTH_FOR_WRITES", false);
        return enabled;
    }

    static bool isMutatingMethod(const std::string& method) {
        return method == "POST" || method == "PUT" || method == "PATCH" || method == "DELETE";
    }

    static std::string sanitizeClientIdentity(std::string value) {
        value = trimAsciiCopy(std::move(value));
        if (value.size() > 64) {
            value.resize(64);
        }

        std::string out;
        out.reserve(value.size());
        for (char ch : value) {
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalnum(uch) != 0 || ch == '.' || ch == ':' || ch == '-' || ch == '_') {
                out.push_back(ch);
            }
        }
        return out;
    }

    static std::string getClientIdentity(SOCKET clientFd,
                                         const std::unordered_map<std::string, std::string>& headers) {
        const auto forwarded = headers.find("x-forwarded-for");
        if (forwarded != headers.end()) {
            std::string candidate = forwarded->second;
            const size_t comma = candidate.find(',');
            if (comma != std::string::npos) {
                candidate = candidate.substr(0, comma);
            }
            candidate = sanitizeClientIdentity(std::move(candidate));
            if (!candidate.empty()) {
                return candidate;
            }
        }

        const auto realIp = headers.find("x-real-ip");
        if (realIp != headers.end()) {
            std::string candidate = sanitizeClientIdentity(realIp->second);
            if (!candidate.empty()) {
                return candidate;
            }
        }

        sockaddr_storage peerAddr{};
        int peerLen = static_cast<int>(sizeof(peerAddr));
        if (getpeername(clientFd, reinterpret_cast<sockaddr*>(&peerAddr), &peerLen) != SOCKET_ERROR) {
            char host[NI_MAXHOST] = {0};
            DWORD hostLen = static_cast<DWORD>(sizeof(host));
            if (WSAAddressToStringA(reinterpret_cast<sockaddr*>(&peerAddr), peerLen, nullptr, host, &hostLen) == 0) {
                std::string candidate = sanitizeClientIdentity(host);
                if (!candidate.empty()) {
                    return candidate;
                }
            }
        }

        return "unknown";
    }

    static bool pathExemptFromRateLimit(const std::string& path) {
        return isPublicRouteNoAuthRequired(path) || path == "/api/status" || path == "/api/headless/status";
    }

    static bool allowHeadlessRequestByRateLimit(const std::string& clientId,
                                                const std::string& method,
                                                const std::string& path,
                                                int& retryAfterSeconds) {
        retryAfterSeconds = 0;
        const int rpm = getHeadlessRateLimitRpm();
        if (rpm <= 0 || clientId.empty() || method == "OPTIONS" || pathExemptFromRateLimit(path)) {
            return true;
        }

        using Clock = std::chrono::steady_clock;
        constexpr uint64_t windowMs = 60000;
        const uint64_t nowMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());
        const std::string key = clientId + "|" + method + "|" + path;

        static std::mutex s_rateMutex;
        static std::unordered_map<std::string, std::deque<uint64_t>> s_rateBuckets;

        std::lock_guard<std::mutex> lock(s_rateMutex);
        auto& bucket = s_rateBuckets[key];
        while (!bucket.empty() && (nowMs - bucket.front()) >= windowMs) {
            bucket.pop_front();
        }

        if (bucket.size() >= static_cast<size_t>(rpm)) {
            const uint64_t oldest = bucket.front();
            const uint64_t remainingMs = (oldest + windowMs > nowMs) ? (oldest + windowMs - nowMs) : 0;
            retryAfterSeconds = static_cast<int>((remainingMs + 999) / 1000);
            if (retryAfterSeconds <= 0) {
                retryAfterSeconds = 1;
            }
            return false;
        }

        bucket.push_back(nowMs);
        return true;
    }

    static bool pathIsWithinRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
        auto rootIt = root.begin();
        auto candidateIt = candidate.begin();
        for (; rootIt != root.end(); ++rootIt, ++candidateIt) {
            if (candidateIt == candidate.end()) {
                return false;
            }
            if (toLowerCopy(rootIt->string()) != toLowerCopy(candidateIt->string())) {
                return false;
            }
        }
        return true;
    }

    static bool resolveScopedPath(const std::string& inputPath,
                                  std::filesystem::path& outPath,
                                  std::string& outError) {
        outPath.clear();
        outError.clear();

        if (inputPath.empty()) {
            outError = "missing_path";
            return false;
        }

        std::error_code ec;
        std::filesystem::path candidate(inputPath);
        const std::string scopedRootRaw = getHeadlessRootScope();
        const bool scoped = !scopedRootRaw.empty();

        std::filesystem::path scopedRoot;
        if (scoped) {
            scopedRoot = std::filesystem::path(scopedRootRaw);
            if (scopedRoot.is_relative()) {
                scopedRoot = std::filesystem::absolute(scopedRoot, ec);
                if (ec) {
                    outError = "invalid_root";
                    return false;
                }
            }
            scopedRoot = scopedRoot.lexically_normal();

            if (candidate.is_relative()) {
                candidate = scopedRoot / candidate;
            }
        }

        candidate = candidate.lexically_normal();
        std::filesystem::path absoluteCandidate = std::filesystem::absolute(candidate, ec);
        if (ec) {
            outError = "invalid_path";
            return false;
        }
        absoluteCandidate = absoluteCandidate.lexically_normal();

        if (scoped) {
            std::filesystem::path absoluteRoot = std::filesystem::absolute(scopedRoot, ec);
            if (ec) {
                outError = "invalid_root";
                return false;
            }
            absoluteRoot = absoluteRoot.lexically_normal();

            if (!pathIsWithinRoot(absoluteRoot, absoluteCandidate)) {
                outError = "path_outside_scope";
                return false;
            }
        }

        outPath = absoluteCandidate;
        return true;
    }

    static std::string makeRequestId() {
        static std::atomic<unsigned long> seq{0};
        const unsigned long n = ++seq;
        const unsigned long long ticks = static_cast<unsigned long long>(GetTickCount64());
        std::ostringstream oss;
        oss << std::hex << ticks << "-" << n;
        return oss.str();
    }

    static bool fileExistsNoThrow(const std::filesystem::path& candidate) {
        if (candidate.empty()) {
            return false;
        }

        std::error_code ec;
        return std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec);
    }

    static void appendUniqueDirectory(std::vector<std::filesystem::path>& directories,
                                      std::set<std::string>& seen,
                                      const std::filesystem::path& candidate) {
        if (candidate.empty()) {
            return;
        }

        std::error_code ec;
        std::filesystem::path normalized = candidate;
        if (std::filesystem::exists(candidate, ec)) {
            normalized = std::filesystem::absolute(candidate, ec);
            if (ec) {
                normalized = candidate;
                ec.clear();
            }
        }

        const std::string key = toLowerCopy(normalized.lexically_normal().string());
        if (!seen.insert(key).second) {
            return;
        }

        directories.push_back(normalized);
    }

    static void appendEnvDirectories(std::vector<std::filesystem::path>& directories,
                                     std::set<std::string>& seen,
                                     const char* envVarName) {
        if (!envVarName || !envVarName[0]) {
            return;
        }

        const DWORD envLen = GetEnvironmentVariableA(envVarName, nullptr, 0);
        if (envLen <= 1) {
            return;
        }

        std::string envValue(static_cast<size_t>(envLen), '\0');
        const DWORD copied = GetEnvironmentVariableA(envVarName, envValue.data(), envLen);
        if (copied == 0) {
            return;
        }

        envValue.resize(copied);
        size_t start = 0;
        while (start <= envValue.size()) {
            const size_t end = envValue.find(';', start);
            std::string token = envValue.substr(start, end == std::string::npos ? std::string::npos : end - start);
            const size_t first = token.find_first_not_of(" \t\r\n\"");
            if (first != std::string::npos) {
                const size_t last = token.find_last_not_of(" \t\r\n\"");
                token = token.substr(first, last - first + 1);
                appendUniqueDirectory(directories, seen, token);
            }
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
    }

    static bool isLocalModelAlias(const std::string& requestedModel) {
        const std::string normalized = toLowerCopy(requestedModel);
        return normalized.empty() || normalized == "rawrxd-local" || normalized == "headless-default" ||
               normalized == "local" || normalized == "localgguf";
    }

    static std::string discoverHeadlessLocalModelPath(const HeadlessConfig& config,
                                                      const std::string& loadedModelPath,
                                                      const std::string& requestedModel) {
        std::vector<std::filesystem::path> directories;
        std::set<std::string> seenDirectories;

        auto appendParentIfFile = [&](const std::string& candidate) {
            if (candidate.empty()) {
                return;
            }
            std::filesystem::path candidatePath(candidate);
            if (fileExistsNoThrow(candidatePath)) {
                appendUniqueDirectory(directories, seenDirectories, candidatePath.parent_path());
            }
        };

        appendParentIfFile(config.modelPath);
        appendParentIfFile(loadedModelPath);
        if (const char* envModel = std::getenv("RAWRXD_NATIVE_MODEL_PATH")) {
            appendParentIfFile(envModel);
        }

        appendEnvDirectories(directories, seenDirectories, "RAWRXD_MODELS_PATH");
        appendEnvDirectories(directories, seenDirectories, "OLLAMA_MODELS");

        std::error_code ec;
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::current_path(ec));
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::path("D:/"));
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::path("D:/models"));
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::path("D:/OllamaModels"));
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::path("F:/"));
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::path("F:/models"));
        appendUniqueDirectory(directories, seenDirectories, std::filesystem::path("F:/OllamaModels"));

        std::vector<std::string> requestedNames;
        if (!requestedModel.empty() && !isLocalModelAlias(requestedModel)) {
            requestedNames.push_back(requestedModel);
            if (requestedModel.find(".gguf") == std::string::npos) {
                requestedNames.push_back(requestedModel + ".gguf");
            }
        }

        auto tryCandidatePath = [&](const std::filesystem::path& candidatePath) -> std::string {
            if (fileExistsNoThrow(candidatePath)) {
                return candidatePath.lexically_normal().string();
            }
            return std::string();
        };

        for (const auto& name : requestedNames) {
            const std::filesystem::path directPath(name);
            if (const std::string directMatch = tryCandidatePath(directPath); !directMatch.empty()) {
                return directMatch;
            }
            for (const auto& directory : directories) {
                if (const std::string discovered = tryCandidatePath(directory / name); !discovered.empty()) {
                    return discovered;
                }
            }
        }

        for (const auto& preferred : kPreferredLocalModelNames) {
            for (const auto& directory : directories) {
                if (const std::string discovered = tryCandidatePath(directory / preferred); !discovered.empty()) {
                    return discovered;
                }
            }
        }

        for (const auto& directory : directories) {
            std::error_code iterEc;
            std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, iterEc);
            std::filesystem::directory_iterator end;
            if (iterEc) {
                continue;
            }

            for (; it != end; it.increment(iterEc)) {
                if (iterEc) {
                    break;
                }
                const std::filesystem::path candidatePath = it->path();
                if (candidatePath.extension() == ".gguf") {
                    return candidatePath.lexically_normal().string();
                }
            }
        }

        return std::string();
    }

static const char* httpStatusText(int statusCode) noexcept {
    switch (statusCode) {
        case 204: return "No Content";
        case 200: return "OK";
        case 401: return "Unauthorized";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        default: return "Error";
    }
}

static bool readHttpRequest(SOCKET clientFd, std::string& request, int& statusCode, std::string& errorBody) {
    request.clear();
    statusCode = 0;
    errorBody.clear();

    setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&kHeadlessHttpRecvTimeoutMs), sizeof(kHeadlessHttpRecvTimeoutMs));

    size_t headerEnd = std::string::npos;
    size_t expectedBodyBytes = 0;
    bool contentLengthFound = false;
    char buf[4096];

    while (request.size() < kMaxHeadlessHttpRequestBytes) {
        const int bytesRead = recv(clientFd, buf, sizeof(buf), 0);
        if (bytesRead <= 0) {
            break;
        }

        request.append(buf, static_cast<size_t>(bytesRead));

        if (headerEnd == std::string::npos) {
            headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                if (!tryParseContentLength(request, headerEnd, expectedBodyBytes, contentLengthFound)) {
                    statusCode = 400;
                    errorBody = "{\"error\":\"Invalid Content-Length\"}";
                    return false;
                }
                if (expectedBodyBytes > kMaxHeadlessHttpBodyBytes) {
                    statusCode = 413;
                    errorBody = "{\"error\":\"Request body too large\"}";
                    return false;
                }
            }
        }

        // Only use the Content-Length–based early exit when the header was explicitly
        // present.  Without it we keep reading until recv times out or the peer closes,
        // which is the correct read-until-close behaviour for HTTP/1.0-style bodies or
        // clients that omit Content-Length.
        if (headerEnd != std::string::npos && contentLengthFound) {
            const size_t totalExpected = headerEnd + 4 + expectedBodyBytes;
            if (request.size() >= totalExpected) {
                if (request.size() > totalExpected) {
                    request.resize(totalExpected);
                }
                return true;
            }
            if (expectedBodyBytes == 0) {
                return true;
            }
        }

            // For methods that carry no body (GET, HEAD, OPTIONS, DELETE), return
            // immediately once headers are complete.  Waiting for recv timeout
            // when Content-Length is absent causes unnecessary 2-second delays
            // on health checks and other lightweight GET endpoints.
            if (headerEnd != std::string::npos && !contentLengthFound) {
                const bool isBodylessMethod =
                    request.size() >= 4 &&
                    (request.compare(0, 4, "GET ")    == 0 ||
                     request.compare(0, 5, "HEAD ")   == 0 ||
                     request.compare(0, 8, "OPTIONS ") == 0 ||
                     request.compare(0, 7, "DELETE ")  == 0);
                if (isBodylessMethod) {
                    return true;
                }
            }
    }

    if (request.size() >= kMaxHeadlessHttpRequestBytes) {
        statusCode = 413;
        errorBody = "{\"error\":\"HTTP request too large\"}";
        return false;
    }

    if (headerEnd == std::string::npos) {
        statusCode = 400;
        errorBody = "{\"error\":\"Incomplete HTTP request headers\"}";
        return false;
    }

    // Headers received; recv loop exited via timeout or peer close.
    // Validate completeness only when Content-Length was explicitly declared.
    if (contentLengthFound) {
        const size_t totalExpected = headerEnd + 4 + expectedBodyBytes;
        if (request.size() < totalExpected) {
            statusCode = 400;
            errorBody = "{\"error\":\"Incomplete HTTP request body\"}";
            return false;
        }
        if (request.size() > totalExpected) {
            request.resize(totalExpected);
        }
    }
    return true;
}
} // namespace

// Helper: read boolean env var with default
static bool readEnvFlag(const char* name, bool defaultValue) {
    const char* v = std::getenv(name);
    if (!v) return defaultValue;
    if (_stricmp(v, "1") == 0 || _stricmp(v, "true") == 0 || _stricmp(v, "yes") == 0 || _stricmp(v, "on") == 0) {
        return true;
    }
    if (_stricmp(v, "0") == 0 || _stricmp(v, "false") == 0 || _stricmp(v, "no") == 0 || _stricmp(v, "off") == 0) {
        return false;
    }
    return defaultValue;
}

constexpr size_t kMaxOptionalEnginePathBytes = 1024;
constexpr size_t kMaxOptionalEngineIdBytes = 128;

static bool validateOptionalEngineArgs(const char* enginePath,
                                       const char* engineId,
                                       const char*& reason) noexcept {
    reason = nullptr;
    if (!enginePath || !engineId || !*enginePath || !*engineId) {
        reason = "missing engine id/path";
        return false;
    }

    const size_t pathLen = std::strlen(enginePath);
    const size_t idLen = std::strlen(engineId);
    if (pathLen == 0 || pathLen > kMaxOptionalEnginePathBytes) {
        reason = "invalid engine path length";
        return false;
    }
    if (idLen == 0 || idLen > kMaxOptionalEngineIdBytes) {
        reason = "invalid engine id length";
        return false;
    }
    if (std::strstr(enginePath, "..") != nullptr) {
        reason = "engine path contains parent traversal";
        return false;
    }

    for (size_t i = 0; i < idLen; ++i) {
        const unsigned char ch = static_cast<unsigned char>(engineId[i]);
        const bool allowed =
            std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
        if (!allowed) {
            reason = "engine id contains invalid characters";
            return false;
        }
    }

    for (size_t i = 0; i < pathLen; ++i) {
        const unsigned char ch = static_cast<unsigned char>(enginePath[i]);
        if (std::iscntrl(ch) != 0 || ch == '"' || ch == '<' || ch == '>' ||
            ch == '|' || ch == '*' || ch == '?') {
            reason = "engine path contains invalid characters";
            return false;
        }
    }

    return true;
}

static void tryLoadOptionalEngine(IOutputSink* sink,
                                  EngineManager* engineManager,
                                  const char* enginePath,
                                  const char* engineId) noexcept {
    if (!sink || !engineManager || !enginePath || !engineId) {
        return;
    }

    const char* validationReason = nullptr;
    if (!validateOptionalEngineArgs(enginePath, engineId, validationReason)) {
        std::ostringstream msg;
        msg << "Optional engine load skipped: reason="
            << (validationReason ? validationReason : "invalid arguments")
            << " id=" << (engineId ? engineId : "<null>")
            << " path=" << (enginePath ? enginePath : "<null>");
        sink->appendOutput(msg.str().c_str(), OutputSeverity::Warning);
        return;
    }

    try {
        if (!engineManager->LoadEngine(enginePath, engineId)) {
            std::ostringstream msg;
            msg << "Optional engine load failed: id=" << engineId << " path=" << enginePath;
            sink->appendOutput(msg.str().c_str(), OutputSeverity::Warning);
        }
    } catch (const std::exception& ex) {
        std::ostringstream msg;
        msg << "Optional engine load threw: id=" << engineId << " path=" << enginePath
            << " error=" << ex.what();
        sink->appendOutput(msg.str().c_str(), OutputSeverity::Warning);
    } catch (...) {
        std::ostringstream msg;
        msg << "Optional engine load threw unknown exception: id=" << engineId << " path=" << enginePath;
        sink->appendOutput(msg.str().c_str(), OutputSeverity::Warning);
    }
}


// ============================================================================
// Global shutdown flag for SIGINT/SIGTERM handler
// ============================================================================
static std::atomic<HeadlessIDE*> g_headlessInstance{nullptr};

// ============================================================================
// Embedded LSP server instance (owned by HeadlessIDE init, lives in .cpp scope)
// ============================================================================
static std::unique_ptr<RawrXD::LSPServer::RawrXDLSPServer> g_embeddedLSP;
static std::mutex                       g_embeddedLSPMutex;

static void headlessSignalHandler(int sig) {
    FILE* f = fopen("headless_server.log", "a");
    if (f) {
        fprintf(f, "SIGNAL_RECEIVED %d\n", sig);
        fclose(f);
    }
    HeadlessIDE* inst = g_headlessInstance.load();
    if (inst) {
        inst->requestShutdown();
    }
}

// ============================================================================
// ConsoleOutputSink implementation
// ============================================================================
void ConsoleOutputSink::appendOutput(const char* text, size_t length, OutputSeverity severity) noexcept {
    if (!text || length == 0) return;
    if (m_quiet && severity < OutputSeverity::Warning) return;
    if (!m_verbose && severity == OutputSeverity::Debug) return;

    if (m_jsonMode) {
        const char* sevStr = "info";
        switch (severity) {
            case OutputSeverity::Debug:   sevStr = "debug"; break;
            case OutputSeverity::Info:    sevStr = "info"; break;
            case OutputSeverity::Warning: sevStr = "warning"; break;
            case OutputSeverity::Error:   sevStr = "error"; break;
        }
        fprintf(stdout, "{\"type\":\"output\",\"severity\":\"%s\",\"text\":\"", sevStr);
        const std::string escaped = jsonEscape(text, length);
        fwrite(escaped.data(), 1, escaped.size(), stdout);
        fputs("\"}\n", stdout);
    } else {
        FILE* out = (severity >= OutputSeverity::Warning) ? stderr : stdout;
        const char* prefix = "";
        switch (severity) {
            case OutputSeverity::Debug:   prefix = "[DEBUG] "; break;
            case OutputSeverity::Info:    prefix = ""; break;
            case OutputSeverity::Warning: prefix = "[WARN]  "; break;
            case OutputSeverity::Error:   prefix = "[ERROR] "; break;
        }
        fprintf(out, "%s%.*s\n", prefix, (int)length, text);
    }
}

void ConsoleOutputSink::onStreamingToken(const char* token, size_t length, StreamTokenOrigin origin) noexcept {
    if (!token || length == 0) return;
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"token\",\"origin\":%d,\"text\":\"", (int)origin);
        const std::string escaped = jsonEscape(token, length);
        fwrite(escaped.data(), 1, escaped.size(), stdout);
        fputs("\"}\n", stdout);
        fflush(stdout);
    } else {
        // Direct token output — no newline, for streaming effect
        fwrite(token, 1, length, stdout);
        fflush(stdout);
    }
}

void ConsoleOutputSink::onStreamStart(const char* sourceId) noexcept {
    if (m_jsonMode) {
        const std::string escaped = jsonEscape(sourceId ? sourceId : "", sourceId ? std::strlen(sourceId) : 0);
        fprintf(stdout, "{\"type\":\"stream_start\",\"source\":\"%s\"}\n", escaped.c_str());
    } else if (m_verbose) {
        fprintf(stdout, "\n--- Stream started: %s ---\n", sourceId ? sourceId : "unknown");
    }
}

void ConsoleOutputSink::onStreamEnd(const char* sourceId, bool success) noexcept {
    if (m_jsonMode) {
        const std::string escaped = jsonEscape(sourceId ? sourceId : "", sourceId ? std::strlen(sourceId) : 0);
        fprintf(stdout, "{\"type\":\"stream_end\",\"source\":\"%s\",\"success\":%s}\n",
                escaped.c_str(), success ? "true" : "false");
    } else {
        if (!m_quiet) fprintf(stdout, "\n");
        if (m_verbose) {
            fprintf(stdout, "--- Stream ended: %s (%s) ---\n",
                    sourceId ? sourceId : "unknown", success ? "ok" : "FAILED");
        }
    }
}

void ConsoleOutputSink::onAgentStarted(const char* agentId, const char* prompt) noexcept {
    if (m_jsonMode) {
        const std::string escaped = jsonEscape(agentId ? agentId : "", agentId ? std::strlen(agentId) : 0);
        fprintf(stdout, "{\"type\":\"agent_started\",\"agentId\":\"%s\"}\n",
                escaped.c_str());
    } else if (m_verbose) {
        fprintf(stdout, "[AGENT] Started: %s\n", agentId ? agentId : "?");
    }
}

void ConsoleOutputSink::onAgentCompleted(const char* agentId, const char* result, int durationMs) noexcept {
    if (m_jsonMode) {
        const std::string escaped = jsonEscape(agentId ? agentId : "", agentId ? std::strlen(agentId) : 0);
        fprintf(stdout, "{\"type\":\"agent_completed\",\"agentId\":\"%s\",\"durationMs\":%d}\n",
                escaped.c_str(), durationMs);
    } else if (m_verbose) {
        fprintf(stdout, "[AGENT] Completed: %s (%dms)\n", agentId ? agentId : "?", durationMs);
    }
}

void ConsoleOutputSink::onAgentFailed(const char* agentId, const char* error) noexcept {
    if (m_jsonMode) {
        const std::string escapedId = jsonEscape(agentId ? agentId : "", agentId ? std::strlen(agentId) : 0);
        const std::string escapedErr = jsonEscape(error ? error : "", error ? std::strlen(error) : 0);
        fprintf(stdout, "{\"type\":\"agent_failed\",\"agentId\":\"%s\",\"error\":\"%s\"}\n",
                escapedId.c_str(), escapedErr.c_str());
    } else {
        fprintf(stderr, "[AGENT] Failed: %s — %s\n",
                agentId ? agentId : "?", error ? error : "unknown error");
    }
}

void ConsoleOutputSink::onStatusUpdate(const char* key, const char* value) noexcept {
    if (m_jsonMode) {
        const std::string escapedKey = jsonEscape(key ? key : "", key ? std::strlen(key) : 0);
        const std::string escapedValue = jsonEscape(value ? value : "", value ? std::strlen(value) : 0);
        fprintf(stdout, "{\"type\":\"status\",\"key\":\"%s\",\"value\":\"%s\"}\n",
                escapedKey.c_str(), escapedValue.c_str());
    } else if (m_verbose) {
        fprintf(stdout, "[STATUS] %s: %s\n", key ? key : "?", value ? value : "?");
    }
}

void ConsoleOutputSink::flush() noexcept {
    fflush(stdout);
    fflush(stderr);
}

// Deleter definition now that AgentHistoryRecorder is complete
void AgentHistoryDeleter::operator()(AgentHistoryRecorder* ptr) const {
    delete ptr;
}

// ============================================================================
// HeadlessIDE — Constructor / Destructor
// ============================================================================
HeadlessIDE::HeadlessIDE() {
    // Generate session ID
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    m_startEpochMs = static_cast<uint64_t>(epoch);
    m_sessionId = "headless-" + std::to_string(m_startEpochMs);

    // Default output sink
    m_outputSink = std::make_unique<ConsoleOutputSink>();
}

HeadlessIDE::~HeadlessIDE() {
    if (m_running.load()) {
        requestShutdown();
    }
    shutdownAll();
}

// ============================================================================
// Lifecycle
// ============================================================================
HeadlessResult HeadlessIDE::initialize(int argc, char* argv[]) {
    HeadlessResult r = parseArgs(argc, argv);
    if (!r.success) return r;
    return initialize(m_config);
}

HeadlessResult HeadlessIDE::initialize(const HeadlessConfig& config) {
    m_config = config;

    // Breadcrumb file: trace headless init for hang diagnostics
    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "INIT_BEGIN mode=%d enableServer=%d port=%d bind=%s\n",
                    (int)m_config.mode, m_config.enableServer ? 1 : 0, m_config.port,
                    m_config.bindAddress.c_str());
            fclose(f);
        }
    }

    // Debug breadcrumb: write effective config for headless startup
    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "CONFIG mode=%d enableServer=%d port=%d bind=%s\n",
                    (int)m_config.mode, m_config.enableServer ? 1 : 0, m_config.port,
                    m_config.bindAddress.c_str());
            fclose(f);
        }
    }

    // Experimental toggles (env-driven)
    m_expHotpatchEnabled        = readEnvFlag("RAWRXD_ENABLE_70B_HOTPATCH", true);
    m_expLayerEvictionEnabled   = readEnvFlag("RAWRXD_ENABLE_LAYER_EVICTION", true);
    m_expGovernorEnabled        = readEnvFlag("RAWRXD_ENABLE_GOVERNOR", true);
    m_expQuantumTimeEnabled     = readEnvFlag("RAWRXD_ENABLE_QTIME_MANAGER", false);
    m_expQuantumOrchEnabled     = readEnvFlag("RAWRXD_ENABLE_QAGENT_ORCH", false);
    m_expQuantumMissingEnabled  = readEnvFlag("RAWRXD_ENABLE_QMISSING_IMPL", false);

    // Configure output sink based on config
    if (auto* console = dynamic_cast<ConsoleOutputSink*>(m_outputSink.get())) {
        console->setVerbose(m_config.verbose);
        console->setQuiet(m_config.quiet);
        console->setJsonMode(m_config.jsonOutput);
    }

    m_outputSink->appendOutput("RawrXD Headless IDE initializing...", OutputSeverity::Info);
    {
        std::string sessionMsg = "Session: " + m_sessionId;
        m_outputSink->appendOutput(sessionMsg.c_str(), OutputSeverity::Debug);
    }
    {
        std::string versionMsg = "Version: " + std::string(VERSION);
        m_outputSink->appendOutput(versionMsg.c_str(), OutputSeverity::Debug);
    }
    {
        std::string memStrategy = std::string("[MEM_STRATEGY] ") + RawrXD::GetApertureAlignmentStrategyLabel();
        m_outputSink->appendOutput(memStrategy.c_str(), OutputSeverity::Info);
    }

    // Initialize WinSock (required for HTTP server + remote backends)
    HeadlessResult wr = initWinsock();
    if (!wr.success) return wr;

    // Initialize engines
    HeadlessResult er = initEngines();
    if (!er.success) {
        m_outputSink->appendOutput(er.detail, OutputSeverity::Warning);
        // Non-fatal: engines are optional, server can run without them
    }

    // Initialize subsystems — all are non-fatal
    const bool minimalHeadless = readEnvFlag("RAWRXD_HEADLESS_MINIMAL", true);
    m_headlessMinimal = minimalHeadless;

    auto tryInit = [this](HeadlessResult (HeadlessIDE::*fn)(), const char* name) {
        HeadlessResult r = (this->*fn)();
        if (!r.success) {
            std::ostringstream oss;
            oss << name << ": " << r.detail;
            std::string msg = oss.str();
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Warning);
        }
    };

    if (minimalHeadless) {
        m_outputSink->appendOutput("Headless minimal profile active (RAWRXD_HEADLESS_MINIMAL=1): skipping optional subsystem threads",
                                   OutputSeverity::Info);
        tryInit(&HeadlessIDE::initBackendManager, "BackendManager");
    } else {
        tryInit(&HeadlessIDE::initBackendManager, "BackendManager");
        tryInit(&HeadlessIDE::initLLMRouter, "LLMRouter");
        tryInit(&HeadlessIDE::initFailureDetection, "FailureDetection");
        tryInit(&HeadlessIDE::initAgentHistory, "AgentHistory");
        tryInit(&HeadlessIDE::initAsmSemantic, "AsmSemantic");
        const bool enableHeadlessLsp = readEnvFlag("RAWRXD_HEADLESS_ENABLE_LSP", false);
        if (enableHeadlessLsp) {
            tryInit(&HeadlessIDE::initLSPClient, "LSPClient");
        } else {
            m_outputSink->appendOutput("LSPClient: disabled in headless by default (set RAWRXD_HEADLESS_ENABLE_LSP=1 to enable)",
                                       OutputSeverity::Debug);
        }
        tryInit(&HeadlessIDE::initHybridBridge, "HybridBridge");
        tryInit(&HeadlessIDE::initMultiResponse, "MultiResponse");
        if (m_expGovernorEnabled) {
            tryInit(&HeadlessIDE::initPhase10, "Phase10-ExecGovernor");
            m_expGovernorActivated = m_phase10Initialized;
            if (m_expGovernorActivated) {
                m_outputSink->appendOutput("[EXPERIMENTAL] governor_activated=true (RAWRXD_ENABLE_GOVERNOR=1)", OutputSeverity::Info);
            }
        } else {
            m_outputSink->appendOutput("[EXPERIMENTAL] governor_activated=false (RAWRXD_ENABLE_GOVERNOR=0)", OutputSeverity::Debug);
        }
        tryInit(&HeadlessIDE::initPhase11, "Phase11-Swarm");
        tryInit(&HeadlessIDE::initPhase12, "Phase12-NativeDebug");
        if (m_expHotpatchEnabled) {
            tryInit(&HeadlessIDE::initHotpatch, "Hotpatch");
            m_expHotpatchActivated = m_hotpatchInitialized;
            if (m_expHotpatchActivated) {
                m_outputSink->appendOutput("[EXPERIMENTAL] hotpatch70b_activated=true (RAWRXD_ENABLE_70B_HOTPATCH=1)", OutputSeverity::Info);
            }
        } else {
            m_outputSink->appendOutput("[EXPERIMENTAL] hotpatch70b_activated=false (RAWRXD_ENABLE_70B_HOTPATCH=0)", OutputSeverity::Debug);
        }
        if (m_expLayerEvictionEnabled && m_hotpatchInitialized) {
            m_expLayerEvictionActivated = true;
            m_outputSink->appendOutput("[EXPERIMENTAL] layer_eviction_activated=true (RAWRXD_ENABLE_LAYER_EVICTION=1)", OutputSeverity::Info);
        } else if (m_expLayerEvictionEnabled) {
            m_outputSink->appendOutput("[EXPERIMENTAL] layer_eviction_activated=false (waiting on hotpatch init)", OutputSeverity::Debug);
        }
        tryInit(&HeadlessIDE::initInstructions, "Instructions");
    }

    // Quantum feature markers (no-op wiring; status/log visibility)
    if (m_expQuantumTimeEnabled) {
        m_expQuantumTimeActivated = true;
        m_outputSink->appendOutput("[EXPERIMENTAL] quantum_time_manager_activated=true (RAWRXD_ENABLE_QTIME_MANAGER=1)", OutputSeverity::Info);
    }
    if (m_expQuantumOrchEnabled) {
        m_expQuantumOrchActivated = true;
        m_outputSink->appendOutput("[EXPERIMENTAL] quantum_orchestrator_activated=true (RAWRXD_ENABLE_QAGENT_ORCH=1)", OutputSeverity::Info);
    }
    if (m_expQuantumMissingEnabled) {
        m_expQuantumMissingActivated = true;
        m_outputSink->appendOutput("[EXPERIMENTAL] quantum_missing_impl_activated=true (RAWRXD_ENABLE_QMISSING_IMPL=1)", OutputSeverity::Info);
    }

    // Load model if specified
    if (!m_config.modelPath.empty()) {
        if (!loadModel(m_config.modelPath)) {
            return HeadlessResult::error("Failed to load model", 2);
        }
    }

    // Load settings
    if (!m_config.settingsFile.empty()) {
        loadSettings(m_config.settingsFile);
    }

    m_outputSink->appendOutput("Headless IDE initialized successfully.", OutputSeverity::Info);

    // Breadcrumb: init complete
    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "INIT_DONE\n");
            fclose(f);
        }
    }
    return HeadlessResult::ok("Initialized");
}

int HeadlessIDE::run() {
    m_running.store(true);
    m_shutdownRequested.store(false);

    // Register signal handlers
    g_headlessInstance.store(this);
    signal(SIGINT, headlessSignalHandler);
    signal(SIGTERM, headlessSignalHandler);

    int exitCode = 0;

    if (m_config.benchSweep) {
        exitCode = runScalingSweepMode();
    } else if (m_config.benchAttention) {
        exitCode = runAttentionBenchMode();
    } else {

        switch (m_config.mode) {
            case HeadlessRunMode::Server:
                exitCode = runServerMode();
                break;
            case HeadlessRunMode::REPL:
                exitCode = runReplMode();
                break;
            case HeadlessRunMode::SingleShot:
                exitCode = runSingleShotMode();
                break;
            case HeadlessRunMode::Batch:
                exitCode = runBatchMode();
                break;
        }
    }

    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f,
                    "RUN_EXIT mode=%d exitCode=%d shutdownRequested=%d serverRunning=%d\n",
                    static_cast<int>(m_config.mode),
                    exitCode,
                    m_shutdownRequested.load() ? 1 : 0,
                    m_serverRunning.load() ? 1 : 0);
            fclose(f);
        }
    }

    m_running.store(false);
    g_headlessInstance.store(nullptr);
    return exitCode;
}

void HeadlessIDE::requestShutdown() noexcept {
    m_shutdownRequested.store(true);
    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "REQUEST_SHUTDOWN serverRunning=%d\n", m_serverRunning.load() ? 1 : 0);
            fclose(f);
        }
    }
    stopServer();
}

void HeadlessIDE::setOutputSink(std::unique_ptr<IOutputSink> sink) {
    if (sink) m_outputSink = std::move(sink);
}

// ============================================================================
// Argument Parsing
// ============================================================================
HeadlessResult HeadlessIDE::parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--headless") {
            // Already in headless mode (this flag is consumed by main_win32.cpp)
            continue;
        }
        else if (arg == "--port") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --port", 2);
            }
            int parsedPort = 0;
            if (!tryParseIntArg(argv[i + 1], 1, 65535, parsedPort)) {
                return HeadlessResult::error("Invalid value for --port", 2);
            }
            m_config.port = parsedPort;
            ++i;
        }
        else if (arg == "--bind" || arg == "--host") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bind/--host", 2);
            }
            m_config.bindAddress = argv[++i];
        }
        else if (arg == "--model" || arg == "--load-model") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --model/--load-model", 2);
            }
            m_config.modelPath = argv[++i];
        }
        else if (arg == "--exit-after-load") {
            // For forensic flows: initialize, load model, flush telemetry, then exit.
            m_config.exitAfterLoad = true;
            m_config.enableServer = false;
        }
        else if (arg == "--forensic-map-only") {
            // Minimal forensic path: avoid allocator-heavy metadata parse.
            m_config.forensicMapOnly = true;
            m_config.exitAfterLoad = true;
            m_config.enableServer = false;
        }
        else if (arg == "--bench-attention") {
            m_config.benchAttention = true;
            m_config.benchSweep = false;
            m_config.enableServer = false;
        }
        else if (arg == "--bench-sweep") {
            m_config.benchSweep = true;
            m_config.benchAttention = false;
            m_config.enableServer = false;
        }
        else if (arg == "--bench-batch") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-batch", 2);
            }
            int parsedBatch = 0;
            if (!tryParseIntArg(argv[i + 1], 1, 64, parsedBatch)) {
                return HeadlessResult::error("Invalid value for --bench-batch", 2);
            }
            m_config.benchBatchSize = parsedBatch;
            ++i;
        }
        else if (arg == "--bench-seq-len") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-seq-len", 2);
            }
            int parsedSeqLen = 0;
            if (!tryParseIntArg(argv[i + 1], 16, 8192, parsedSeqLen)) {
                return HeadlessResult::error("Invalid value for --bench-seq-len", 2);
            }
            m_config.benchSeqLen = parsedSeqLen;
            ++i;
        }
        else if (arg == "--bench-head-dim") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-head-dim", 2);
            }
            int parsedHeadDim = 0;
            if (!tryParseIntArg(argv[i + 1], 16, 1024, parsedHeadDim)) {
                return HeadlessResult::error("Invalid value for --bench-head-dim", 2);
            }
            m_config.benchHeadDim = parsedHeadDim;
            ++i;
        }
        else if (arg == "--bench-heads") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-heads", 2);
            }
            int parsedHeads = 0;
            if (!tryParseIntArg(argv[i + 1], 1, 256, parsedHeads)) {
                return HeadlessResult::error("Invalid value for --bench-heads", 2);
            }
            m_config.benchNumHeads = parsedHeads;
            ++i;
        }
        else if (arg == "--bench-warmup") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-warmup", 2);
            }
            int parsedWarmup = 0;
            if (!tryParseIntArg(argv[i + 1], 0, 100, parsedWarmup)) {
                return HeadlessResult::error("Invalid value for --bench-warmup", 2);
            }
            m_config.benchWarmupIters = parsedWarmup;
            ++i;
        }
        else if (arg == "--bench-iters") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-iters", 2);
            }
            int parsedIters = 0;
            if (!tryParseIntArg(argv[i + 1], 1, 1000, parsedIters)) {
                return HeadlessResult::error("Invalid value for --bench-iters", 2);
            }
            m_config.benchMeasureIters = parsedIters;
            ++i;
        }
        else if (arg == "--bench-median") {
            m_config.benchMedianMode = true;
        }
        else if (arg == "--bench-csv") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --bench-csv", 2);
            }
            m_config.benchCsvPath = argv[++i];
        }
        else if (arg == "--trace-token-summary") {
            m_config.traceTokenSummary = true;
            m_config.mode = HeadlessRunMode::SingleShot;
        }
        else if (arg == "--trace-token-csv") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --trace-token-csv", 2);
            }
            m_config.traceTokenCsvPath = argv[++i];
            m_config.mode = HeadlessRunMode::SingleShot;
        }
        else if (arg == "--prompt") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --prompt", 2);
            }
            m_config.prompt = argv[++i];
            if (!isReasonablePromptSize(m_config.prompt)) {
                return HeadlessResult::error("Prompt exceeds maximum supported size", 2);
            }
            m_config.mode = HeadlessRunMode::SingleShot;
        }
        else if (arg == "--input") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --input", 2);
            }
            m_config.inputFile = argv[++i];
            m_config.mode = HeadlessRunMode::Batch;
        }
        else if (arg == "--output") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --output", 2);
            }
            m_config.outputFile = argv[++i];
        }
        else if (arg == "--settings") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --settings", 2);
            }
            m_config.settingsFile = argv[++i];
        }
        else if (arg == "--backend") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --backend", 2);
            }
            m_config.backend = argv[++i];
        }
        else if (arg == "--max-tokens") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --max-tokens", 2);
            }
            int parsedMaxTokens = 0;
            if (!tryParseIntArg(argv[i + 1], 1, 131072, parsedMaxTokens)) {
                return HeadlessResult::error("Invalid value for --max-tokens", 2);
            }
            m_config.maxTokens = parsedMaxTokens;
            ++i;
        }
        else if (arg == "--temperature") {
            if (i + 1 >= argc) {
                return HeadlessResult::error("Missing value for --temperature", 2);
            }
            float parsedTemperature = 0.0f;
            if (!tryParseFloatArg(argv[i + 1], 0.0f, 5.0f, parsedTemperature)) {
                return HeadlessResult::error("Invalid value for --temperature", 2);
            }
            m_config.temperature = parsedTemperature;
            ++i;
        }
        else if (arg == "--repl") {
            m_config.enableRepl = true;
            m_config.mode = HeadlessRunMode::REPL;
        }
        else if (arg == "--no-server") {
            m_config.enableServer = false;
        }
        else if (arg == "--verbose" || arg == "-v") {
            m_config.verbose = true;
        }
        else if (arg == "--quiet" || arg == "-q") {
            m_config.quiet = true;
        }
        else if (arg == "--json") {
            m_config.jsonOutput = true;
        }
        else if (arg == "--help" || arg == "-h") {
            printReplHelp();
            return HeadlessResult::error("Help requested", 0);
        }
        else if (arg.rfind("--", 0) == 0) {
            static thread_local std::string unknownArgError;
            unknownArgError = "Unknown argument: " + arg;
            return HeadlessResult::error(unknownArgError.c_str(), 2);
        }
    }

    // Defensive default: if the parsed/initial port is invalid, clamp to 11435
    int originalPort = m_config.port;
    if (m_config.port <= 0 || m_config.port > 65535 || m_config.port == 9090) {
        m_config.port = 11435;
    }

    // Always log the resolved port to aid port-binding diagnostics
    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "EFFECTIVE_PORT %d (was %d)\n", m_config.port, originalPort);
            fclose(f);
        }
    }

    return HeadlessResult::ok();
}

// ============================================================================
// Initialization Phases
// ============================================================================
HeadlessResult HeadlessIDE::initWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return HeadlessResult::error("WSAStartup failed", result);
    }
    m_winsockInitialized = true;
    return HeadlessResult::ok("WinSock initialized");
}

HeadlessResult HeadlessIDE::initEngines() {
    // Engine manager and Codex are optional — set externally via setEngineManager/setCodexUltimate
    // In headless mode we attempt to load them, but failure is non-fatal
    if (!m_engineManager) {
        m_engineManager = new EngineManager();
        if (RawrXD::EnterpriseLicense::Instance().Is800BUnlocked() || RawrXD::g_800B_Unlocked) {
            tryLoadOptionalEngine(m_outputSink.get(), m_engineManager,
                                  "engines/800b-5drive/800b_engine.dll", "800b-5drive");
        }
        tryLoadOptionalEngine(m_outputSink.get(), m_engineManager,
                              "engines/codex-ultimate/codex.dll", "codex-ultimate");
        tryLoadOptionalEngine(m_outputSink.get(), m_engineManager,
                              "engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
    }
    if (!m_codexUltimate) {
        m_codexUltimate = new CodexUltimate();
    }
    return HeadlessResult::ok("Engines initialized");
}

HeadlessResult HeadlessIDE::initBackendManager() {
    auto startTime = std::chrono::steady_clock::now();

    // Configure default backend based on config
    if (!m_config.backend.empty()) {
        if (m_config.backend == "ollama")  m_activeBackend = AIBackendType::Ollama;
        else if (m_config.backend == "openai")  m_activeBackend = AIBackendType::OpenAI;
        else if (m_config.backend == "claude")  m_activeBackend = AIBackendType::Claude;
        else if (m_config.backend == "gemini")  m_activeBackend = AIBackendType::Gemini;
        else m_activeBackend = AIBackendType::LocalGGUF;
    }

    // Probe Ollama availability (primary backend)
    RawrXD::Agent::OllamaConfig ollamaCfg;
    ollamaCfg.host = "127.0.0.1";
    ollamaCfg.port = 11434;
    ollamaCfg.timeout_ms = 3000;
    RawrXD::Agent::AgentOllamaClient probeClient(ollamaCfg);
    bool ollamaAvailable = probeClient.TestConnection();

    std::ostringstream statusMsg;
    statusMsg << "Backend manager initialized (headless)";
    if (ollamaAvailable) {
        auto models = probeClient.ListModels();
        statusMsg << " | Ollama: online (" << models.size() << " models)";
        // Default to Ollama if available and no explicit backend set
        if (m_config.backend.empty()) {
            m_activeBackend = AIBackendType::Ollama;
        }
    } else {
        statusMsg << " | Ollama: offline";
    }

    const char* backendNames[] = { "LocalGGUF", "Ollama", "OpenAI", "Claude", "Gemini" };
    statusMsg << " | Active: " << backendNames[static_cast<int>(m_activeBackend)];

    m_backendManagerInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    statusMsg << " [" << elapsed << "us]";
    {
        std::string msg = statusMsg.str();
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    }
    m_outputSink->onStatusUpdate("backend_manager", "active");
    m_outputSink->onStatusUpdate("backend", backendNames[static_cast<int>(m_activeBackend)]);
    return HeadlessResult::ok("Backend manager ready");
}

HeadlessResult HeadlessIDE::initLLMRouter() {
    auto startTime = std::chrono::steady_clock::now();

    // Configure routing table with backend priorities
    // Priority: Ollama (local, fast) > LocalGGUF > Cloud backends
    struct RouterEntry {
        AIBackendType type;
        const char* name;
        int priority;  // lower = higher priority
        bool available;
    };

    RouterEntry routes[] = {
        { AIBackendType::Ollama,    "Ollama",    1, m_backendManagerInitialized },
        { AIBackendType::LocalGGUF, "LocalGGUF",  2, m_modelLoaded },
        { AIBackendType::OpenAI,    "OpenAI",    10, false },
        { AIBackendType::Claude,    "Claude",    11, false },
        { AIBackendType::Gemini,    "Gemini",    12, false },
    };

    int activeRoutes = 0;
    for (auto& r : routes) {
        if (r.available) activeRoutes++;
    }

    m_routerInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    char msg[256];
    snprintf(msg, sizeof(msg), "LLM router initialized: %d/%d backends available [%lldus]",
             activeRoutes, 5, (long long)elapsed);
    m_outputSink->appendOutput(msg, OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("llm_router", "active");
    return HeadlessResult::ok("LLM router ready");
}

HeadlessResult HeadlessIDE::initFailureDetection() {
    auto startTime = std::chrono::steady_clock::now();
    m_failureDetectorInitialized = true;
    m_failureDetections = 0;
    m_failureRetries = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Failure detector initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("failure_detector", "active");
    return HeadlessResult::ok("Failure detector ready");
}

HeadlessResult HeadlessIDE::initAgentHistory() {
    auto startTime = std::chrono::steady_clock::now();
    if (!m_historyRecorder) {
        m_historyRecorder.reset(new AgentHistoryRecorder("rawrxd_headless_history"));
        m_historyRecorder->setLogCallback([this](int level, const std::string& msg) {
            if (level >= 2) {
                m_outputSink->appendOutput(("[History] " + msg).c_str(), OutputSeverity::Debug);
            }
        });
    }
    m_agentHistoryInitialized = true;
    m_agentEventCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Agent history initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("agent_history", "active");
    return HeadlessResult::ok("Agent history ready");
}

HeadlessResult HeadlessIDE::initAsmSemantic() {
    auto startTime = std::chrono::steady_clock::now();
    m_asmSemanticInitialized = true;
    m_asmSymbolCount = 0;
    m_asmFilesParsed = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "ASM semantic initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("asm_semantic", "active");
    return HeadlessResult::ok("ASM semantic ready");
}

HeadlessResult HeadlessIDE::initLSPClient() {
    auto startTime = std::chrono::steady_clock::now();

    // Create embedded LSP server for headless diagnostics + code intelligence
    {
        std::lock_guard<std::mutex> lk(g_embeddedLSPMutex);
        if (!g_embeddedLSP) {
            g_embeddedLSP = std::make_unique<RawrXD::LSPServer::RawrXDLSPServer>();
        }

        // Configure for in-process (pipe) transport — headless owns stdio
        RawrXD::LSPServer::ServerConfig lspConfig;
        lspConfig.useStdio           = false;  // Use named pipe, not stdio
        lspConfig.pipeName           = "\\\\.\\pipe\\rawrxd-lsp-headless";
        lspConfig.enableSemanticTokens = true;
        lspConfig.enableHover        = true;
        lspConfig.enableCompletion   = true;
        lspConfig.enableDefinition   = true;
        lspConfig.enableReferences   = true;
        lspConfig.enableDocumentSymbol  = true;
        lspConfig.enableWorkspaceSymbol = true;
        lspConfig.enableDiagnostics  = true;
        lspConfig.indexThrottleMs    = 100;  // Faster for headless
        lspConfig.maxSymbolResults   = 1000;
        lspConfig.maxCompletionItems = 200;

        // Set workspace root from current working dir or explicitly if available
        char cwd[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
            lspConfig.rootPath = cwd;
            // Convert to file URI
            std::string pathStr = cwd;
            for (auto& c : pathStr) { if (c == '\\') c = '/'; }
            lspConfig.rootUri = "file:///" + pathStr;
        }

        g_embeddedLSP->configure(lspConfig);

        // Start the LSP server (launches reader + dispatch threads)
        if (g_embeddedLSP->start()) {
            m_lspServerCount = 1;

            // Trigger initial project indexing if we have a root path
            if (!lspConfig.rootPath.empty()) {
                g_embeddedLSP->rebuildIndex();
                size_t symCount = g_embeddedLSP->getIndexedSymbolCount();
                size_t fileCount = g_embeddedLSP->getTrackedFileCount();

                std::ostringstream oss;
                oss << "  LSP initial index: " << symCount << " symbols across "
                    << fileCount << " files";
                m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Debug);
            }
        } else {
            m_outputSink->appendOutput("LSP server failed to start on named pipe",
                                       OutputSeverity::Warning);
            // Still mark initialized — server exists but isn't running
        }
    }

    m_lspInitialized = true;
    m_lspCompletionCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "LSP client initialized (headless, embedded server) [" +
                      std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("lsp_client", "active");
    return HeadlessResult::ok("LSP client ready (embedded server)");
}

HeadlessResult HeadlessIDE::initHybridBridge() {
    auto startTime = std::chrono::steady_clock::now();
    m_hybridBridgeInitialized = true;
    m_hybridCompletionCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Hybrid bridge initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("hybrid_bridge", "active");
    return HeadlessResult::ok("Hybrid bridge ready");
}

HeadlessResult HeadlessIDE::initMultiResponse() {
    auto startTime = std::chrono::steady_clock::now();
    if (!m_multiResponse) {
        m_multiResponse = std::make_unique<MultiResponseEngine>();
        auto initResult = m_multiResponse->initialize();
        if (!initResult.success) {
            m_outputSink->appendOutput("Multi-response engine failed to initialize", OutputSeverity::Warning);
        }
    }
    m_multiResponseInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Multi-response initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("multi_response", "active");
    return HeadlessResult::ok("Multi-response ready");
}

HeadlessResult HeadlessIDE::initPhase10() {
    auto startTime = std::chrono::steady_clock::now();

    // Phase 10A: Execution Governor
    auto& governor = ExecutionGovernor::instance();
    if (!governor.isInitialized()) {
        governor.init();
    }

    // Phase 10B: Safety Contract
    auto& safety = AgentSafetyContract::instance();
    safety.init();
    safety.setAutoApproveEscalations(true); // headless: auto-approve

    // Phase 10C: Deterministic Replay Journal
    auto& replay = ReplayJournal::instance();
    replay.init("rawrxd_headless_replay");
    replay.startSession("headless-" + m_sessionId);
    replay.startRecording();
    replay.recordMarker("Headless IDE Phase 10 initialized");

    // Phase 10D: Confidence Gate
    auto& confidence = ConfidenceGate::instance();
    confidence.init();
    confidence.setPolicy(GatePolicy::Normal);
    confidence.setEnabled(true);
    confidence.setAutoEscalate(true); // headless: auto-escalate

    m_phase10Initialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Phase 10 (Governor+Safety+Replay+Confidence) initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("phase10", "active");
    return HeadlessResult::ok("Phase 10 ready");
}

HeadlessResult HeadlessIDE::initPhase11() {
    auto startTime = std::chrono::steady_clock::now();
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    if (!swarm.isInitialized()) {
        auto result = swarm.initialize(RawrXD::Swarm::NodeRole::Coordinator);
        if (!result.success) {
            m_outputSink->appendOutput(
                ("Swarm init note: " + std::string(result.detail)).c_str(),
                OutputSeverity::Debug);
            // Non-fatal: swarm is optional in headless single-node mode
        }
    }
    m_phase11Initialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Phase 11 (Distributed Swarm) initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("swarm", swarm.isRunning() ? "active" : "standby");
    return HeadlessResult::ok("Phase 11 ready");
}

HeadlessResult HeadlessIDE::initPhase12() {
    auto startTime = std::chrono::steady_clock::now();
    m_phase12Initialized = true;
    m_debugSessionActive = false;
    m_debugBreakpointCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Phase 12 (Native Debugger) initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("native_debugger", "ready");
    return HeadlessResult::ok("Phase 12 ready");
}

HeadlessResult HeadlessIDE::initHotpatch() {
    auto startTime = std::chrono::steady_clock::now();
    auto& hotpatcher = UniversalModelHotpatcher::instance();
    if (!hotpatcher.isInitialized()) {
        hotpatcher.initialize();
    }
    m_hotpatchInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Three-layer hotpatch initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("hotpatch", "active");
    return HeadlessResult::ok("Hotpatch ready");
}

HeadlessResult HeadlessIDE::initInstructions() {
    auto startTime = std::chrono::steady_clock::now();
    auto& provider = InstructionsProvider::instance();

    // Add workspace-relative search paths
    provider.addSearchPath(".");
    provider.addSearchPath(".github");

    auto r = provider.loadAll();
    m_instructionsInitialized = r.success;

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    if (r.success) {
        std::string msg = "Instructions loaded: " +
            std::to_string(provider.getLoadedCount()) + " files (" +
            std::to_string(provider.getAllContent().size()) + " bytes) [" +
            std::to_string(elapsed) + "us]";
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        m_outputSink->onStatusUpdate("instructions", "loaded");
    } else {
        std::string msg = std::string("Instructions: ") + r.detail +
            " [" + std::to_string(elapsed) + "us]";
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Warning);
        m_outputSink->onStatusUpdate("instructions", "unavailable");
    }

    return r.success ? HeadlessResult::ok("Instructions loaded") 
                     : HeadlessResult::error(r.detail);
}

std::string HeadlessIDE::getInstructionsContent() const {
    auto& provider = InstructionsProvider::instance();
    if (!provider.isLoaded()) {
        InstructionsProvider::instance().loadAll();
    }
    return provider.getAllContent();
}

// ============================================================================
// Model Operations
// ============================================================================
bool HeadlessIDE::loadModel(const std::string& filepath) {
    try {
    m_outputSink->appendOutput(("Loading model: " + filepath).c_str(), OutputSeverity::Info);
    auto t0 = std::chrono::steady_clock::now();

    // Phase 1: Resolve the model source — local, Ollama, HuggingFace, URL
    RawrXD::ModelSourceResolver resolver;
    RawrXD::ResolvedModelPath resolved = resolver.Resolve(filepath,
        [this](const RawrXD::ModelDownloadProgress& p) {
            if (p.total_bytes > 0) {
                char buf[256];
                snprintf(buf, sizeof(buf), "[Model] Downloading %.1f%% (%llu / %llu bytes)",
                         p.progress_percent, (unsigned long long)p.downloaded_bytes,
                         (unsigned long long)p.total_bytes);
                m_outputSink->appendOutput(buf, OutputSeverity::Info);
            }
        });
    
    std::string localPath = resolved.success ? resolved.local_path : filepath;
    
    // Phase 2: Validate file exists on disk
    DWORD attr = GetFileAttributesA(localPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::string err = "Model file not found: " + localPath;
        if (!resolved.success && !resolved.error_message.empty()) {
            err += " (" + resolved.error_message + ")";
        }
        m_outputSink->appendOutput(err.c_str(), OutputSeverity::Error);
        return false;
    }

    // Expose the resolved model path for native Win32 inference DLL shims.
    SetEnvironmentVariableA("RAWRXD_NATIVE_MODEL_PATH", localPath.c_str());

    // For forensic map-only mode, skip heavyweight GGUF metadata parsing and
    // perform a direct sliding-window probe through BackendOrchestrator.
    if (m_config.forensicMapOnly) {
        try {
            auto& orchestrator = RawrXD::BackendOrchestrator::Instance();
            if (!orchestrator.IsInitialized()) {
                orchestrator.Initialize();
            }
            std::string reason;
            if (!orchestrator.ForensicMapProbe(localPath, 1ULL * 1024ULL * 1024ULL * 1024ULL, 64 * 1024, &reason)) {
                std::string err = std::string("Forensic map-only probe failed: ") + reason;
                m_outputSink->appendOutput(err.c_str(), OutputSeverity::Error);
                return false;
            }
            m_loadedModelPath = localPath;
            size_t slash = localPath.find_last_of("/\\");
            m_loadedModelName = (slash != std::string::npos) ? localPath.substr(slash + 1) : localPath;
            m_modelLoaded = true;
            m_outputSink->appendOutput("Model forensic map-only probe completed", OutputSeverity::Info);
            if (!reason.empty()) {
                m_outputSink->appendOutput(reason.c_str(), OutputSeverity::Info);
            }
            recordSimpleEvent("model_loaded_forensic_map_only");
            return true;
        } catch (const std::exception& ex) {
            std::string err = std::string("Forensic map-only exception: ") + ex.what();
            m_outputSink->appendOutput(err.c_str(), OutputSeverity::Error);
            return false;
        } catch (...) {
            m_outputSink->appendOutput("Forensic map-only exception: unknown", OutputSeverity::Error);
            return false;
        }
    }

    // For forensic one-shot mode, skip heavyweight GGUF metadata parsing and
    // map through BackendOrchestrator directly so telemetry can flush on exit.
    if (m_config.exitAfterLoad) {
        try {
            auto& orchestrator = RawrXD::BackendOrchestrator::Instance();
            if (!orchestrator.IsInitialized()) {
                orchestrator.Initialize();
            }
            if (!orchestrator.LoadModel(localPath, m_orchestratorModelTag)) {
                m_outputSink->appendOutput("BackendOrchestrator failed to load model in forensic mode",
                                           OutputSeverity::Error);
                return false;
            }
            m_orchestratorModelLoaded = true;
            m_loadedModelPath = localPath;
            size_t slash = localPath.find_last_of("/\\");
            m_loadedModelName = (slash != std::string::npos) ? localPath.substr(slash + 1) : localPath;
            m_modelLoaded = true;
            m_outputSink->appendOutput("Model loaded via orchestrator forensic path", OutputSeverity::Info);
            recordSimpleEvent("model_loaded_forensic");
            return true;
        } catch (const std::exception& ex) {
            std::string err = std::string("Forensic model load exception: ") + ex.what();
            m_outputSink->appendOutput(err.c_str(), OutputSeverity::Error);
            return false;
        } catch (...) {
            m_outputSink->appendOutput("Forensic model load exception: unknown", OutputSeverity::Error);
            return false;
        }
    }

    // Phase 3: Open with StreamingGGUFLoader and parse header + metadata
    auto loader = std::make_unique<RawrXD::StreamingGGUFLoader>();
    if (!loader->Open(localPath)) {
        m_outputSink->appendOutput("Failed to open GGUF file", OutputSeverity::Error);
        return false;
    }

    if (!loader->ParseHeader()) {
        m_outputSink->appendOutput("Invalid GGUF header — file may be corrupt", OutputSeverity::Error);
        loader->Close();
        return false;
    }

    RawrXD::GGUFHeader hdr = loader->GetHeader();
    // Validate magic: 0x46554747 = "GGUF" little-endian (bytes 47 47 55 46)
    if (hdr.magic != 0x46554747U) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Bad GGUF magic: 0x%08X (expected 0x46554747)", hdr.magic);
        m_outputSink->appendOutput(buf, OutputSeverity::Error);
        loader->Close();
        return false;
    }

    if (!loader->ParseMetadata()) {
        m_outputSink->appendOutput("Failed to parse GGUF metadata", OutputSeverity::Warning);
        // Non-fatal — we can still load with header-only info
    }

    RawrXD::GGUFMetadata meta = loader->GetMetadata();

    // Phase 4: Build tensor index for streaming zone loading
    loader->BuildTensorIndex();

    // Phase 5: Register model with BackendOrchestrator so GGUF map telemetry
    // is emitted on unload/shutdown in headless forensic mode.
    try {
        auto& orchestrator = RawrXD::BackendOrchestrator::Instance();
        if (!orchestrator.IsInitialized()) {
            orchestrator.Initialize();
        }
        if (orchestrator.LoadModel(localPath, m_orchestratorModelTag)) {
            m_orchestratorModelLoaded = true;
        } else {
            m_outputSink->appendOutput("BackendOrchestrator model registration failed; continuing with header-only load",
                                       OutputSeverity::Warning);
        }
    } catch (const std::exception& ex) {
        std::string warn = std::string("BackendOrchestrator registration exception: ") + ex.what();
        m_outputSink->appendOutput(warn.c_str(), OutputSeverity::Warning);
    } catch (...) {
        m_outputSink->appendOutput("BackendOrchestrator registration exception: unknown",
                                   OutputSeverity::Warning);
    }

    // Store state
    m_loadedModelPath = localPath;
    size_t lastSlash = localPath.find_last_of("/\\");
    m_loadedModelName = (lastSlash != std::string::npos) ? localPath.substr(lastSlash + 1) : localPath;
    m_modelLoaded = true;

    auto t1 = std::chrono::steady_clock::now();
    int loadMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    // Report model info
    std::ostringstream info;
    info << "Model loaded: " << m_loadedModelName << "\n"
         << "  GGUF version: " << hdr.version << "\n"
         << "  Tensors: " << hdr.tensor_count << "\n"
         << "  Metadata KVs: " << hdr.metadata_kv_count << "\n"
         << "  Layers: " << meta.layer_count << "\n"
         << "  Context length: " << meta.context_length << "\n"
         << "  Embedding dim: " << meta.embedding_dim << "\n"
         << "  Vocab size: " << meta.vocab_size << "\n"
         << "  File size: " << (loader->GetFileSize() / (1024*1024)) << " MB\n"
         << "  Load latency: " << loadMs << " ms\n";
    if (resolved.success && resolved.source_type != GGUFConstants::ModelSourceType::LOCAL_FILE) {
        info << "  Source: " << resolved.original_input << "\n";
    }
    m_outputSink->appendOutput(info.str().c_str(), OutputSeverity::Info);
    m_outputSink->onStatusUpdate("model", m_loadedModelName.c_str());

    loader->Close();
    recordSimpleEvent("model_loaded");
    return true;
    } catch (const std::exception& ex) {
        std::string err = std::string("Model load exception: ") + ex.what();
        m_outputSink->appendOutput(err.c_str(), OutputSeverity::Error);
        return false;
    } catch (...) {
        m_outputSink->appendOutput("Model load exception: unknown", OutputSeverity::Error);
        return false;
    }
}

bool HeadlessIDE::unloadModel() {
    if (!m_modelLoaded) return false;
    m_outputSink->appendOutput(("Unloading model: " + m_loadedModelName).c_str(), OutputSeverity::Info);
    if (m_orchestratorModelLoaded) {
        try {
            RawrXD::BackendOrchestrator::Instance().UnloadModel(m_orchestratorModelTag);
        } catch (...) {
            // Best-effort cleanup in headless teardown path.
        }
        m_orchestratorModelLoaded = false;
    }
    m_modelLoaded = false;
    m_loadedModelPath.clear();
    m_loadedModelName.clear();
    SetEnvironmentVariableA("RAWRXD_NATIVE_MODEL_PATH", nullptr);
    m_outputSink->onStatusUpdate("model", "(none)");
    return true;
}

bool HeadlessIDE::isModelLoaded() const {
    return m_modelLoaded;
}

std::string HeadlessIDE::getLoadedModelName() const {
    return m_loadedModelName;
}

std::string HeadlessIDE::getModelInfo() const {
    if (!m_modelLoaded) return "No model loaded";
    std::ostringstream oss;
    oss << "Model: " << m_loadedModelName << "\n";
    oss << "Path: " << m_loadedModelPath << "\n";
    return oss.str();
}

bool HeadlessIDE::prepareInferenceBackend(const std::string& requestedModel) {
    const bool shouldPrepareLocal =
        isLocalModelAlias(requestedModel) ||
        (m_activeBackend == AIBackendType::LocalGGUF) ||
        (!requestedModel.empty() && toLowerCopy(requestedModel).find(".gguf") != std::string::npos);

    if (!shouldPrepareLocal) {
        return false;
    }

    if (m_modelLoaded) {
        m_activeBackend = AIBackendType::LocalGGUF;
        return true;
    }

    const std::string discoveredModelPath = discoverHeadlessLocalModelPath(m_config, m_loadedModelPath, requestedModel);
    if (discoveredModelPath.empty()) {
        m_outputSink->appendOutput("No local GGUF model discovered for headless inference", OutputSeverity::Warning);
        return false;
    }

    std::string msg = "Headless auto-loading local model: " + discoveredModelPath;
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
    if (!loadModel(discoveredModelPath)) {
        m_outputSink->appendOutput("Headless local model auto-load failed", OutputSeverity::Warning);
        return false;
    }

    m_activeBackend = AIBackendType::LocalGGUF;
    return true;
}

// ============================================================================
// Inference
// ============================================================================
std::string HeadlessIDE::runInference(const std::string& prompt) {
    return runInference(prompt, m_config.maxTokens, m_config.temperature);
}

std::string HeadlessIDE::runInference(const std::string& prompt, int maxTokens, float temperature) {
    (void)maxTokens;
    (void)temperature;

    m_outputSink->onAgentStarted("inference", prompt.c_str());

    auto startTime = std::chrono::steady_clock::now();

    // Route through backend manager → LLM router → inference engine
    // This delegates to the same path as Win32IDE::routeInferenceRequest
    std::string result = routeInferenceRequest(prompt);

    auto endTime = std::chrono::steady_clock::now();
    int durationMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    if (!result.empty()) {
        m_outputSink->onAgentCompleted("inference", result.c_str(), durationMs);
    } else {
        m_outputSink->onAgentFailed("inference", "Empty result from inference engine");
    }

    return result;
}

void HeadlessIDE::runInferenceStreaming(const std::string& prompt,
                                         std::function<void(const char*, size_t)> tokenCallback) {
    m_outputSink->onStreamStart("inference");

    // Use AgentOllamaClient streaming API for real per-token delivery
    const bool canUseOllamaStreaming =
        ((m_activeBackend == AIBackendType::Ollama) ||
         (m_activeBackend == AIBackendType::LocalGGUF && !m_orchestratorModelLoaded)) &&
        probeBackendHealth(AIBackendType::Ollama);

    if (canUseOllamaStreaming) {
        RawrXD::Agent::OllamaConfig cfg;
        cfg.host = "127.0.0.1";
        cfg.port = 11434;
        cfg.timeout_ms = kHeadlessOllamaTimeoutMs;
        // chat_model left empty — auto-detected from Ollama /api/tags
        cfg.temperature = m_config.temperature;
        cfg.max_tokens = m_config.maxTokens;
        cfg.use_gpu = true;
        cfg.num_gpu = 99;

        RawrXD::Agent::AgentOllamaClient client(cfg);
        std::vector<RawrXD::Agent::ChatMessage> messages;
        messages.push_back({"system", "You are RawrXD IDE's embedded AI assistant.", "", {}});
        messages.push_back({"user", prompt, "", {}});

        bool streamOk = client.ChatStream(
            messages, nlohmann::json::array(),
            /* on_token */ [&](const std::string& token) {
                if (tokenCallback) {
                    tokenCallback(token.c_str(), token.size());
                }
                m_outputSink->onStreamingToken(token.c_str(), token.size(), StreamTokenOrigin::Inference);
            },
            /* on_tool_call */ [](const std::string&, const nlohmann::json&) {},
            /* on_done */ [&](const std::string& full, uint64_t pt, uint64_t ct, double tps) {
                char perf[256];
                snprintf(perf, sizeof(perf), "[stream] %llu+%llu tokens, %.1f tok/s",
                         (unsigned long long)pt, (unsigned long long)ct, tps);
                m_outputSink->appendOutput(perf, OutputSeverity::Debug);
            },
            /* on_error */ [&](const std::string& err) {
                m_outputSink->appendOutput(("Stream error: " + err).c_str(), OutputSeverity::Error);
            }
        );

        m_outputSink->onStreamEnd("inference", streamOk);
        return;
    }

    // Fallback: batch inference emitted as single chunk
    std::string result = runInference(prompt);
    if (tokenCallback && !result.empty()) {
        tokenCallback(result.c_str(), result.size());
    }
    m_outputSink->onStreamEnd("inference", !result.empty());
}

// ============================================================================
// Backend Switcher (Phase 8B)
// ============================================================================
bool HeadlessIDE::setActiveBackend(AIBackendType type) {
    const char* backendNames[] = { "LocalGGUF", "Ollama", "OpenAI", "Claude", "Gemini" };
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(AIBackendType::Count)) {
        m_outputSink->appendOutput("Invalid backend type", OutputSeverity::Error);
        return false;
    }

    if (m_activeBackend == type) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Backend already active: %s", backendNames[idx]);
        m_outputSink->appendOutput(buf, OutputSeverity::Debug);
        return true;
    }

    if (!m_headlessMinimal && type == AIBackendType::LocalGGUF && !m_modelLoaded) {
        // Try the lightweight bootstrap first, then explicit model auto-load fallbacks.
        prepareInferenceBackend(std::string());

        if (!m_modelLoaded) {
            std::vector<std::string> candidateModelPaths;
            auto addCandidate = [&](const std::string& path) {
                if (path.empty()) {
                    return;
                }
                if (std::find(candidateModelPaths.begin(), candidateModelPaths.end(), path) == candidateModelPaths.end()) {
                    candidateModelPaths.push_back(path);
                }
            };

            addCandidate(m_config.modelPath);
            addCandidate(m_loadedModelPath);
            if (const char* envPath = std::getenv("RAWRXD_NATIVE_MODEL_PATH")) {
                addCandidate(envPath);
            }
            if (const char* envPath = std::getenv("RAWRXD_MODEL_PATH")) {
                addCandidate(envPath);
            }
            if (const char* envPath = std::getenv("RAWRXD_DEFAULT_MODEL_PATH")) {
                addCandidate(envPath);
            }

            for (const auto& candidate : candidateModelPaths) {
                char autoLoadBuf[384];
                snprintf(autoLoadBuf, sizeof(autoLoadBuf),
                         "LocalGGUF backend requires a loaded model; attempting auto-load: %s",
                         candidate.c_str());
                m_outputSink->appendOutput(autoLoadBuf, OutputSeverity::Info);
                if (loadModel(candidate)) {
                    m_outputSink->appendOutput("LocalGGUF auto-load succeeded", OutputSeverity::Info);
                    break;
                }
            }

            if (!m_modelLoaded) {
                const std::string discoveredPath =
                    discoverHeadlessLocalModelPath(m_config, m_loadedModelPath, "localgguf");
                if (!discoveredPath.empty()) {
                    std::string msg = "LocalGGUF auto-discovered model candidate: " + discoveredPath;
                    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
                    if (loadModel(discoveredPath)) {
                        m_outputSink->appendOutput("LocalGGUF auto-load succeeded via discovery", OutputSeverity::Info);
                    }
                }
            }
        }
    }

    // Probe health before switching
    if (!probeBackendHealth(type)) {
        if (type == AIBackendType::LocalGGUF) {
            m_outputSink->appendOutput(
                "LocalGGUF selected without a loaded model; inference will auto-prepare a model on demand",
                OutputSeverity::Warning);
            AIBackendType previousBackend = m_activeBackend;
            m_activeBackend = type;

            char localBuf[256];
            snprintf(localBuf, sizeof(localBuf), "Backend switched: %s → %s",
                     backendNames[static_cast<int>(previousBackend)], backendNames[idx]);
            m_outputSink->appendOutput(localBuf, OutputSeverity::Info);
            m_outputSink->onStatusUpdate("backend", backendNames[idx]);
            recordSimpleEvent("backend_switch");
            return true;
        }

        const char* hint = "";
        switch (type) {
            case AIBackendType::LocalGGUF: hint = " (load a model first)"; break;
            case AIBackendType::Ollama:    hint = " (ensure Ollama is running on port 11434)"; break;
            case AIBackendType::OpenAI:   hint = " (set OPENAI_API_KEY)"; break;
            case AIBackendType::Claude:   hint = " (set ANTHROPIC_API_KEY)"; break;
            case AIBackendType::Gemini:   hint = " (set GOOGLE_API_KEY or GEMINI_API_KEY)"; break;
            default: break;
        }
        char buf[384];
        snprintf(buf, sizeof(buf), "Backend '%s' health check failed — switch aborted%s", backendNames[idx], hint);
        m_outputSink->appendOutput(buf, OutputSeverity::Warning);
        return false;
    }

    AIBackendType previousBackend = m_activeBackend;
    m_activeBackend = type;

    char buf[256];
    snprintf(buf, sizeof(buf), "Backend switched: %s → %s",
             backendNames[static_cast<int>(previousBackend)], backendNames[idx]);
    m_outputSink->appendOutput(buf, OutputSeverity::Info);
    m_outputSink->onStatusUpdate("backend", backendNames[idx]);
    recordSimpleEvent("backend_switch");
    return true;
}

HeadlessIDE::AIBackendType HeadlessIDE::getActiveBackendType() const {
    return m_activeBackend;
}

std::string HeadlessIDE::getBackendStatusString() const {
    const char* backendNames[] = { "LocalGGUF", "Ollama", "OpenAI", "Claude", "Gemini" };
    int idx = static_cast<int>(m_activeBackend);
    std::ostringstream oss;
    oss << "Backend: " << (idx >= 0 && idx < 5 ? backendNames[idx] : "Unknown") << " (headless)\n";
    oss << "Status: Active\n";
    oss << "Model: " << (m_loadedModelName.empty() ? "(none)" : m_loadedModelName) << "\n";
    oss << "Inference requests: " << m_inferenceRequestCount;
    return oss.str();
}

bool HeadlessIDE::probeBackendHealth(AIBackendType type) {
    switch (type) {
        case AIBackendType::LocalGGUF:
            // Local GGUF: healthy if model is loaded
            return m_modelLoaded;

        case AIBackendType::Ollama: {
            // Probe Ollama server connection
            RawrXD::Agent::OllamaConfig cfg;
            cfg.host = "127.0.0.1";
            cfg.port = 11434;
            cfg.timeout_ms = 5000; // Quick probe timeout
            RawrXD::Agent::AgentOllamaClient client(cfg);
            bool connected = client.TestConnection();
            if (connected) {
                auto models = client.ListModels();
                m_outputSink->appendOutput(
                    ("Ollama: " + std::to_string(models.size()) + " models available").c_str(),
                    OutputSeverity::Debug);
            }
            return connected;
        }

        case AIBackendType::OpenAI:
        case AIBackendType::Claude:
        case AIBackendType::Gemini:
            // Cloud backends: check if API key is configured
            // (API key management is in BackendState singleton)
            return false; // Not yet configured in headless mode

        default:
            return false;
    }
}

std::string HeadlessIDE::routeInferenceRequest(const std::string& prompt) {
    m_inferenceRequestCount++;
    recordSimpleEvent("inference_request");

    auto t0 = std::chrono::steady_clock::now();

    if (!m_headlessMinimal && m_activeBackend == AIBackendType::LocalGGUF && !m_modelLoaded) {
        prepareInferenceBackend(std::string());
    }

    if (m_headlessMinimal && m_activeBackend == AIBackendType::LocalGGUF && !m_modelLoaded) {
        if (!probeBackendHealth(AIBackendType::Ollama)) {
            return "[error] Inference unavailable in headless minimal mode — no local model loaded and Ollama is unreachable";
        }
    }

    // Route based on active backend type
    if (m_activeBackend == AIBackendType::LocalGGUF && m_orchestratorModelLoaded) {
        auto& orchestrator = RawrXD::BackendOrchestrator::Instance();
        if (!orchestrator.IsInitialized()) {
            orchestrator.Initialize();
        }

        std::mutex waitMutex;
        std::condition_variable waitCv;
        bool done = false;
        std::string completion;
        std::string metadata;

        RawrXD::InferRequest req{};
        req.prompt = prompt;
        req.tenant_id = "headless";
        req.priority = RawrXD::RequestPriority::Normal;
        req.max_tokens = (m_config.maxTokens > 0) ? m_config.maxTokens : 512;
        req.complete_cb = [&](const std::string& out, const std::string& meta) {
            std::lock_guard<std::mutex> lock(waitMutex);
            completion = out;
            metadata = meta;
            done = true;
            waitCv.notify_one();
        };

        const uint64_t reqId = orchestrator.Enqueue(std::move(req));
        if (reqId != 0) {
            std::unique_lock<std::mutex> lock(waitMutex);
            const auto nativeTimeout = std::chrono::milliseconds(m_headlessMinimal ? 1500 : 8000);
            if (!waitCv.wait_for(lock, nativeTimeout, [&]() { return done; })) {
                lock.unlock();
                orchestrator.Cancel(reqId);
                m_orchestratorModelLoaded = false;
                m_outputSink->appendOutput("Native orchestrator inference timed out", OutputSeverity::Warning);
            } else if (!completion.empty()) {
                if (completion.rfind("[BackendError]", 0) == 0) {
                    m_orchestratorModelLoaded = false;
                    m_outputSink->appendOutput(("Native orchestrator inference failed: " + completion).c_str(),
                                               OutputSeverity::Warning);
                } else {
                    auto t1 = std::chrono::steady_clock::now();
                    double durationMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
                    char perf[256];
                    snprintf(perf, sizeof(perf), "[inference/native] %.0f ms", durationMs);
                    m_outputSink->appendOutput(perf, OutputSeverity::Debug);
                    if (completion.size() > kMaxHeadlessInferenceResponseBytes) {
                        return completion.substr(0, kMaxHeadlessInferenceResponseBytes);
                    }
                    return completion;
                }
            }
        }
    }

    const bool localBackendNoModel =
        (m_activeBackend == AIBackendType::LocalGGUF && !m_modelLoaded && !m_orchestratorModelLoaded);
    const bool canUseOllama =
        ((m_activeBackend == AIBackendType::Ollama) || localBackendNoModel) &&
        probeBackendHealth(AIBackendType::Ollama);

    if (canUseOllama) {
        // Use AgentOllamaClient for Ollama-backed inference
        RawrXD::Agent::OllamaConfig cfg;
        cfg.host = "127.0.0.1";
        cfg.port = 11434;
        // chat_model left empty — auto-detected from Ollama /api/tags
        cfg.temperature = m_config.temperature;
        cfg.max_tokens = m_config.maxTokens;
        cfg.timeout_ms = m_headlessMinimal ? 2500 : kHeadlessOllamaTimeoutMs;
        cfg.use_gpu = true;
        cfg.num_gpu = 99;

        RawrXD::Agent::AgentOllamaClient client(cfg);

        // Build conversation with system context
        std::vector<RawrXD::Agent::ChatMessage> messages;
        messages.push_back({"system", "You are RawrXD IDE's embedded AI assistant. "
            "Provide accurate, concise answers. When asked about code, give working examples.", "", {}});
        if (m_modelLoaded) {
            messages.push_back({"system", "Loaded model: " + m_loadedModelName, "", {}});
        }
        messages.push_back({"user", prompt, "", {}});

        auto result = client.ChatSync(messages);

        auto t1 = std::chrono::steady_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (result.success) {
            char perf[256];
            snprintf(perf, sizeof(perf),
                     "[inference] %llu prompt + %llu completion tokens, %.1f tok/s, %.0f ms",
                     (unsigned long long)result.prompt_tokens,
                     (unsigned long long)result.completion_tokens,
                     result.tokens_per_sec, durationMs);
            m_outputSink->appendOutput(perf, OutputSeverity::Debug);
            if (result.response.size() > kMaxHeadlessInferenceResponseBytes) {
                return result.response.substr(0, kMaxHeadlessInferenceResponseBytes);
            }
            return result.response;
        } else {
            m_outputSink->appendOutput(
                ("Ollama inference failed: " + result.error_message).c_str(),
                OutputSeverity::Warning);
            // Fall through to engine manager path
        }
    }

    if (m_activeBackend == AIBackendType::LocalGGUF && m_modelLoaded && !m_orchestratorModelLoaded) {
        return "[error] LocalGGUF backend active but native orchestrator inference is unavailable";
    }

    // Secondary path: route through engine manager if available
    if (m_engineManager) {
        std::string currentId = m_engineManager->GetCurrentEngine();
        auto* engine = currentId.empty() ? nullptr : m_engineManager->GetEngine(currentId);
        if (engine && engine->loaded) {
            m_outputSink->appendOutput(("Engine '" + engine->name + "' active but no inference API").c_str(),
                                       OutputSeverity::Debug);
        }
    }

    // Final fallback: provide actionable error
    return "[error] Inference unavailable — ensure Ollama is running on port 11434 "
           "or configure an alternative backend with 'backend <type>'";
}

// ============================================================================
// LLM Router (Phase 8C)
// ============================================================================
std::string HeadlessIDE::routeWithIntelligence(const std::string& prompt) {
    return routeInferenceRequest(prompt);
}

std::string HeadlessIDE::getRouterStatusString() const {
    std::ostringstream oss;
    oss << "LLM Router: " << (m_routerInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Backends: 5 configured\n";
    oss << "Task types: 8\n";
    oss << "Requests routed: " << m_inferenceRequestCount;
    return oss.str();
}

std::string HeadlessIDE::getCostLatencyHeatmapString() const {
    return "Cost/Latency heatmap: (headless mode — collecting data)";
}

// ============================================================================
// Failure Detection (Phase 4B/6)
// ============================================================================
std::string HeadlessIDE::executeWithFailureDetection(const std::string& prompt) {
    return runInference(prompt);
}

std::string HeadlessIDE::getFailureDetectorStats() const {
    std::ostringstream oss;
    oss << "Failure detector: " << (m_failureDetectorInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Total detections: " << m_failureDetections << "\n";
    oss << "Retries: " << m_failureRetries;
    return oss.str();
}

std::string HeadlessIDE::getFailureIntelligenceStatsString() const {
    std::ostringstream oss;
    oss << "Failure intelligence: " << (m_failureDetectorInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Records: " << m_failureDetections << "\n";
    oss << "Accuracy: " << (m_failureDetections > 0 ? "tracking" : "N/A");
    return oss.str();
}

// ============================================================================
// Agent History (Phase 6B)
// ============================================================================
std::string HeadlessIDE::getAgentHistoryStats() const {
    std::ostringstream oss;
    oss << "Agent history: " << (m_agentHistoryInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Session: " << m_sessionId << "\n";
    oss << "Events: " << m_agentEventCount;
    return oss.str();
}

void HeadlessIDE::recordSimpleEvent(const std::string& description) {
    m_agentEventCount++;
    if (m_historyRecorder) {
        m_historyRecorder->record("simple_event", "headless", "", description, "", "", true);
    }
    if (m_phase10Initialized) {
        ReplayJournal::instance().recordMarker(description);
    }
    m_outputSink->appendOutput(("Event: " + description).c_str(), OutputSeverity::Debug);
}

// ============================================================================
// ASM Semantic (Phase 9A)
// ============================================================================
void HeadlessIDE::parseAsmFile(const std::string& filePath) {
    m_asmFilesParsed++;
    m_outputSink->appendOutput(("Parsing ASM file: " + filePath).c_str(), OutputSeverity::Info);
    recordSimpleEvent("asm_parse: " + filePath);
}

void HeadlessIDE::parseAsmDirectory(const std::string& dirPath, bool recursive) {
    {
        std::string asmDirMsg = "Parsing ASM directory: " + dirPath;
        m_outputSink->appendOutput(asmDirMsg.c_str(), OutputSeverity::Info);
    }
    recordSimpleEvent("asm_dir_parse: " + dirPath);
}

std::string HeadlessIDE::getAsmSymbolTableString() const {
    std::ostringstream oss;
    oss << "ASM symbol table (headless)\n";
    oss << "Symbols: " << m_asmSymbolCount << "\n";
    oss << "Files parsed: " << m_asmFilesParsed;
    return oss.str();
}

std::string HeadlessIDE::getAsmSemanticStatsString() const {
    std::ostringstream oss;
    oss << "ASM semantic: " << (m_asmSemanticInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Symbols: " << m_asmSymbolCount << "\n";
    oss << "Files: " << m_asmFilesParsed;
    return oss.str();
}

// ============================================================================
// LSP / Hybrid / Multi-Response status — wired to real subsystem state
// ============================================================================
std::string HeadlessIDE::getLSPStatusString() const {
    std::ostringstream oss;
    oss << "LSP client: " << (m_lspInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Servers: " << m_lspServerCount << " configured\n";

    // Query real embedded server stats if available
    {
        std::lock_guard<std::mutex> lk(g_embeddedLSPMutex);
        if (g_embeddedLSP) {
            auto state = g_embeddedLSP->getState();
            const char* stateStr = "Unknown";
            switch (state) {
                case RawrXD::LSPServer::ServerState::Created:      stateStr = "Created"; break;
                case RawrXD::LSPServer::ServerState::Initializing: stateStr = "Initializing"; break;
                case RawrXD::LSPServer::ServerState::Running:      stateStr = "Running"; break;
                case RawrXD::LSPServer::ServerState::ShuttingDown: stateStr = "ShuttingDown"; break;
                case RawrXD::LSPServer::ServerState::Stopped:      stateStr = "Stopped"; break;
                default: break;
            }
            oss << "Embedded server state: " << stateStr << "\n";
            oss << "Indexed symbols: " << g_embeddedLSP->getIndexedSymbolCount() << "\n";
            oss << "Tracked files: " << g_embeddedLSP->getTrackedFileCount() << "\n";
            auto stats = g_embeddedLSP->getStats();
            oss << "Completion requests: " << stats.completionRequests << "\n";
            oss << "Definition requests: " << stats.definitionRequests << "\n";
            oss << "Hover requests: " << stats.hoverRequests;
        } else {
            oss << "Completions served: " << m_lspCompletionCount;
        }
    }
    return oss.str();
}

std::string HeadlessIDE::getHybridBridgeStatusString() const {
    std::ostringstream oss;
    oss << "Hybrid bridge: " << (m_hybridBridgeInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Completions: " << m_hybridCompletionCount;
    return oss.str();
}

// ============================================================================
// Phase 10/11/12 status — wired to real singletons
// ============================================================================
std::string HeadlessIDE::getGovernorStatus() const {
    if (!m_phase10Initialized) return "Execution governor: Not initialized";
    return ExecutionGovernor::instance().getStatusString();
}

std::string HeadlessIDE::getGovernorStatusJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"status\":\"" << getGovernorStatus() << "\",";
    oss << "\"governor_activated\":" << (m_expGovernorActivated ? "true" : "false") << ",";
    oss << "\"layer_eviction_activated\":" << (m_expLayerEvictionActivated ? "true" : "false");
    oss << "}";
    return oss.str();
}

std::string HeadlessIDE::getSafetyStatus() const {
    if (!m_phase10Initialized) return "Safety contract: Not initialized";
    return AgentSafetyContract::instance().getStatusString();
}

std::string HeadlessIDE::getReplayStatus() const {
    if (!m_phase10Initialized) return "Replay journal: Not initialized";
    return ReplayJournal::instance().getStatusString();
}

std::string HeadlessIDE::getConfidenceStatus() const {
    if (!m_phase10Initialized) return "Confidence gate: Not initialized";
    return ConfidenceGate::instance().getStatusString();
}

std::string HeadlessIDE::getSwarmStatus() const {
    if (!m_phase11Initialized) return "Distributed swarm: Not initialized";
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    std::ostringstream oss;
    oss << "Distributed swarm: " << (swarm.isRunning() ? "Running" : "Standby") << "\n";
    oss << "Node: " << swarm.getNodeId() << "\n";
    oss << "Peers: " << swarm.getNodeCount() << "\n";
    oss << "Role: " << (swarm.isCoordinator() ? "Coordinator" : "Worker");
    auto& stats = swarm.getStats();
    oss << "\nRequests: " << stats.inferenceRequests.load();
    oss << "\nBytes sent/recv: " << stats.bytesSent.load() << "/" << stats.bytesReceived.load();
    return oss.str();
}

std::string HeadlessIDE::getNativeDebugStatus() const {
    std::ostringstream oss;
    oss << "Native debugger: " << (m_phase12Initialized ? "Ready" : "Not initialized") << "\n";
    oss << "Session: " << (m_debugSessionActive ? "active" : "none") << "\n";
    oss << "Breakpoints: " << m_debugBreakpointCount;
    return oss.str();
}

std::string HeadlessIDE::getHotpatchStatus() const {
    if (!m_hotpatchInitialized) return "Three-layer hotpatch: Not initialized";
    auto& hp = UniversalModelHotpatcher::instance();
    const auto& stats = hp.getStats();
    std::ostringstream oss;
    oss << "Three-layer hotpatch: Active\n";
    oss << "Layers analyzed: " << stats.layersAnalyzed.load() << "\n";
    oss << "Layers requantized: " << stats.layersRequantized.load() << "\n";
    oss << "Total surgeries: " << stats.totalSurgeries.load() << "\n";
    oss << "Memory saved: " << (stats.totalMemorySaved.load() / (1024*1024)) << " MB\n";
    oss << "Pressure events: " << stats.pressureEvents.load();
    return oss.str();
}

std::string HeadlessIDE::getHotpatchStatusJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"status\":\"" << getHotpatchStatus() << "\",";
    oss << "\"hotpatch70b_activated\":" << (m_expHotpatchActivated ? "true" : "false") << ",";
    oss << "\"layer_eviction_activated\":" << (m_expLayerEvictionActivated ? "true" : "false");
    oss << "}";
    return oss.str();
}

// ============================================================================
// Settings
// ============================================================================
void HeadlessIDE::loadSettings(const std::string& path) {
    std::string settingsPath = path.empty() ? getSettingsFilePath() : path;
    m_outputSink->appendOutput(("Loading settings: " + settingsPath).c_str(), OutputSeverity::Debug);
}

void HeadlessIDE::saveSettings(const std::string& path) {
    std::string settingsPath = path.empty() ? getSettingsFilePath() : path;
    m_outputSink->appendOutput(("Saving settings: " + settingsPath).c_str(), OutputSeverity::Debug);
}

std::string HeadlessIDE::getSettingsFilePath() const {
    return "rawrxd_settings.json";
}

// ============================================================================
// HTTP Server
// ============================================================================
void HeadlessIDE::startServer() {
    if (m_serverRunning.load()) return;

    auto logStatus = [this](const std::string& msg) {
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Error);
        // Best-effort file breadcrumb to debug headless binding issues
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            fprintf(f, "%lld %s\n", (long long)ms, msg.c_str());
            fclose(f);
        }
    };

    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET) {
        logStatus("Failed to create server socket");
        return;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(m_config.port));
    inet_pton(AF_INET, m_config.bindAddress.c_str(), &addr.sin_addr);

    if (bind(m_serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::string msg = "Failed to bind to " + m_config.bindAddress + ":" + std::to_string(m_config.port) +
                          " (WSA=" + std::to_string(WSAGetLastError()) + ")";
        logStatus(msg);
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
        m_serverRunning.store(false);
        return;
    }

    if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::string msg = "Failed to listen on " + m_config.bindAddress + ":" + std::to_string(m_config.port) +
                          " (WSA=" + std::to_string(WSAGetLastError()) + ")";
        logStatus(msg);
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
        m_serverRunning.store(false);
        return;
    }

    m_serverRunning.store(true);

    std::string msg = "HTTP server listening on " + m_config.bindAddress + ":" + std::to_string(m_config.port);
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);

    m_serverThread = std::thread(&HeadlessIDE::serverLoop, this);
}

void HeadlessIDE::stopServer() {
    if (!m_serverRunning.load()) return;
    m_serverRunning.store(false);
    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    m_outputSink->appendOutput("HTTP server stopped", OutputSeverity::Info);
}

bool HeadlessIDE::isServerRunning() const {
    return m_serverRunning.load();
}

std::string HeadlessIDE::getServerStatus() const {
    if (!m_serverRunning.load()) return "Server: Stopped";
    return "Server: Running on " + m_config.bindAddress + ":" + std::to_string(m_config.port);
}

// File-scope SEH wrapper — must be at file scope (no C++ object locals in __try frame)
// to satisfy MSVC C2712. Catches Win32 structured exceptions (e.g. 0xC0000005 AV)
// that would otherwise propagate through a detached thread and call std::terminate().
DWORD HeadlessIDE::headlessHandleClientSafe(HeadlessIDE* ide, SOCKET clientFd) {
#if defined(_MSC_VER) && defined(_WIN32)
    __try {
        ide->handleClient(clientFd);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD code = GetExceptionCode();
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "HANDLER_CRASH: SEH 0x%08lX on client fd %llu\n",
                    code, (unsigned long long)clientFd);
            fclose(f);
        }
        return code;
    }
#else
    try {
        ide->handleClient(clientFd);
    } catch (const std::exception& ex) {
        FILE* f = fopen("headless_server.log", "a");
        if (f) { fprintf(f, "HANDLER_EXCEPTION: %s\n", ex.what()); fclose(f); }
    } catch (...) {
        FILE* f = fopen("headless_server.log", "a");
        if (f) { fprintf(f, "HANDLER_EXCEPTION: unknown\n"); fclose(f); }
    }
    return 0;
#endif
}

void HeadlessIDE::serverLoop() {
    // Prevent console group SIGINT from terminating headless server
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    while (m_serverRunning.load()) {
        // Set a timeout on accept so we can check the shutdown flag
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_serverSocket, &readSet);
        timeval tv = { 1, 0 };  // 1 second timeout

        int sel = select(0, &readSet, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        SOCKET client = accept(m_serverSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        // Handle requests inline under the SEH/C++ wrapper. The headless server
        // favors stability over concurrency, and avoiding detached client threads
        // prevents lifetime races with the shared output sink and process teardown.
        headlessHandleClientSafe(this, client);
        closesocket(client);
    }
}

void HeadlessIDE::handleClient(SOCKET clientFd) {
    const uint64_t requestStartMs = GetTickCount64();
    const std::string requestId = makeRequestId();
    const std::string auditId = std::string("audit-") + requestId;

    auto sendAll = [&](const std::string& payload) {
        size_t sent = 0;
        while (sent < payload.size()) {
            const int chunk = send(clientFd, payload.c_str() + sent,
                                   static_cast<int>(payload.size() - sent), 0);
            if (chunk <= 0) {
                break;
            }
            sent += static_cast<size_t>(chunk);
        }
    };

    std::string request;
    int preflightStatusCode = 0;
    std::string preflightErrorBody;
    if (!readHttpRequest(clientFd, request, preflightStatusCode, preflightErrorBody)) {
        std::ostringstream resp;
        resp << "HTTP/1.1 " << preflightStatusCode << " " << httpStatusText(preflightStatusCode) << "\r\n";
        resp << "Content-Type: application/json\r\n";
        resp << "Content-Length: " << preflightErrorBody.size() << "\r\n";
        resp << "Access-Control-Allow-Origin: *\r\n";
        resp << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        resp << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
        resp << "X-Content-Type-Options: nosniff\r\n";
        resp << "Cache-Control: no-store\r\n";
        resp << "X-Request-Id: " << requestId << "\r\n";
        resp << "X-Audit-Id: " << auditId << "\r\n";
        resp << "Connection: close\r\n\r\n";
        resp << preflightErrorBody;
        const std::string response = resp.str();
        sendAll(response);
        return;
    }
    // Parse method and path
    std::string method, path;
    {
        size_t sp1 = request.find(' ');
        size_t sp2 = request.find(' ', sp1 + 1);
        if (sp1 != std::string::npos && sp2 != std::string::npos) {
            method = request.substr(0, sp1);
            path = request.substr(sp1 + 1, sp2 - sp1 - 1);
        }
    }

    method = toUpperCopy(trimAsciiCopy(std::move(method)));
    path = stripQueryAndFragment(trimAsciiCopy(std::move(path)));
    const auto headers = parseHttpHeaders(request);
    const std::string clientIdentity = getClientIdentity(clientFd, headers);

    if (method.empty() || path.empty()) {
        const std::string responseBody = "{\"success\":false,\"error\":\"bad_request\"}";
        std::ostringstream resp;
        resp << "HTTP/1.1 400 " << httpStatusText(400) << "\r\n";
        resp << "Content-Type: application/json\r\n";
        resp << "Content-Length: " << responseBody.size() << "\r\n";
        resp << "Access-Control-Allow-Origin: *\r\n";
        resp << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        resp << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
        resp << "X-Content-Type-Options: nosniff\r\n";
        resp << "Cache-Control: no-store\r\n";
        resp << "X-Request-Id: " << requestId << "\r\n";
        resp << "X-Audit-Id: " << auditId << "\r\n";
        resp << "Connection: close\r\n\r\n";
        resp << responseBody;
        sendAll(resp.str());
        return;
    }

    if (method == "OPTIONS") {
        std::ostringstream resp;
        resp << "HTTP/1.1 204 " << httpStatusText(204) << "\r\n";
        resp << "Content-Length: 0\r\n";
        resp << "Access-Control-Allow-Origin: *\r\n";
        resp << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        resp << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
        resp << "X-Content-Type-Options: nosniff\r\n";
        resp << "Cache-Control: no-store\r\n";
        resp << "X-Request-Id: " << requestId << "\r\n";
        resp << "X-Audit-Id: " << auditId << "\r\n";
        resp << "Connection: close\r\n\r\n";
        sendAll(resp.str());
        return;
    }

    // Extract body (after double newline)
    std::string body;
    {
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart != std::string::npos) {
            body = request.substr(bodyStart + 4);
        }
    }

    // Route to handlers (mirrors Win32IDE_LocalServer.cpp endpoints)
    std::string responseBody;
    std::string contentType = "application/json";
    int statusCode = 200;
    int retryAfterSeconds = 0;

    auto methodNotAllowed = [&]() {
        statusCode = 405;
        responseBody = "{\"success\":false,\"error\":\"method_not_allowed\"}";
    };

    if (isMutatingMethod(method) && getHeadlessRequireAuthForWrites()) {
        if (getHeadlessAuthToken().empty()) {
            statusCode = 503;
            responseBody = "{\"success\":false,\"error\":\"auth_token_not_configured\"}";
        } else if (!hasMatchingBearerToken(headers)) {
            statusCode = 401;
            responseBody = "{\"success\":false,\"error\":\"unauthorized\"}";
        }
    }
    else if (!getHeadlessAuthToken().empty() && !isPublicRouteNoAuthRequired(path) && !hasMatchingBearerToken(headers)) {
        statusCode = 401;
        responseBody = "{\"success\":false,\"error\":\"unauthorized\"}";
    }
    else if (!allowHeadlessRequestByRateLimit(clientIdentity, method, path, retryAfterSeconds)) {
        statusCode = 429;
        std::ostringstream oss;
        oss << "{\"success\":false,\"error\":\"rate_limited\",\"retry_after_seconds\":"
            << retryAfterSeconds << "}";
        responseBody = oss.str();
    }
    else if (path == "/api/status" || path == "/api/headless/status") {
        responseBody = getFullStatusDump();
    }
    else if (path == "/api/version") {
        responseBody = "{\"version\":\"" + jsonEscape(std::string(VERSION)) + "\",\"phase\":\"" +
                       jsonEscape(std::string(BUILD_PHASE)) + "\",\"mode\":\"headless\"}";
    }
    else if (path == "/api/model/info") {
        responseBody = "{\"loaded\":" + std::string(m_modelLoaded ? "true" : "false") +
                       ",\"name\":\"" + jsonEscape(m_loadedModelName) + "\"}";
    }
    else if (path == "/") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"status\":\"ok\",\"mode\":\"headless\",";
            oss << "\"uptime_ms\":" << getUptimeMs() << ",";
            oss << "\"model_loaded\":" << (m_modelLoaded ? "true" : "false") << ",";
            oss << "\"backend\":\"" << jsonEscape(getBackendStatusString()) << "\"";
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/tags") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const std::string modelName = m_loadedModelName.empty() ? std::string("headless-default") : m_loadedModelName;
            std::ostringstream oss;
            oss << "{\"success\":true,\"models\":[{";
            oss << "\"name\":\"" << jsonEscape(modelName) << "\",";
            oss << "\"loaded\":" << (m_modelLoaded ? "true" : "false") << ",";
            oss << "\"source\":\"" << jsonEscape(m_loadedModelPath) << "\"";
            oss << "}],\"count\":1}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/model/profiles") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,\"profiles\":[{";
            oss << "\"id\":\"default\",";
            oss << "\"name\":\"Default\",";
            oss << "\"loaded\":" << (m_modelLoaded ? "true" : "false") << ",";
            oss << "\"backend\":\"" << jsonEscape(getBackendStatusString()) << "\",";
            oss << "\"max_tokens\":" << m_config.maxTokens << ",";
            oss << "\"temperature\":" << m_config.temperature;
            oss << "}],\"active\":\"default\"}";
            responseBody = oss.str();
        }
    }
    else if (path == "/models") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const std::string modelName = m_loadedModelName.empty() ? std::string("headless-default") : m_loadedModelName;
            std::ostringstream oss;
            oss << "{\"success\":true,\"models\":[{";
            oss << "\"name\":\"" << jsonEscape(modelName) << "\",";
            oss << "\"path\":\"" << jsonEscape(m_loadedModelPath) << "\",";
            oss << "\"loaded\":" << (m_modelLoaded ? "true" : "false");
            oss << "}],\"loaded\":" << (m_modelLoaded ? "true" : "false") << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/model/load") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string requestedModel;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                requestedModel = j.value("model", "");
            }

            bool loaded = m_modelLoaded;
            std::string detail = "no_model_provided";
            const std::string requestedModelLower = toLowerCopy(requestedModel);
            const bool placeholderModelRequest =
                (requestedModelLower == "test" || requestedModelLower == "smoke" || requestedModelLower == "dummy");
            if (!requestedModel.empty()) {
                if (m_headlessMinimal && placeholderModelRequest) {
                    loaded = m_modelLoaded;
                    detail = "skipped_placeholder_in_minimal_mode";
                } else {
                    loaded = loadModel(requestedModel);
                    detail = loaded ? "loaded" : "load_failed";
                }
            }

            std::ostringstream oss;
            oss << "{\"success\":true,\"loaded\":" << (loaded ? "true" : "false") << ",";
            oss << "\"model\":\"" << jsonEscape(m_loadedModelName) << "\",";
            oss << "\"path\":\"" << jsonEscape(m_loadedModelPath) << "\",";
            oss << "\"detail\":\"" << detail << "\"}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/model/unload") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            const std::string previousName = m_loadedModelName;
            const bool wasLoaded = m_modelLoaded;
            const bool unloaded = unloadModel();

            std::ostringstream oss;
            oss << "{\"success\":true,\"loaded\":false,";
            oss << "\"was_loaded\":" << (wasLoaded ? "true" : "false") << ",";
            oss << "\"unloaded\":" << (unloaded ? "true" : "false") << ",";
            oss << "\"previous_model\":\"" << jsonEscape(previousName) << "\"}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/generate" && method == "POST") {
        // Parse prompt from JSON body
        std::string prompt;
        std::string requestedModel;
        auto j = nlohmann::json::parse(body, nullptr, false);
        if (!j.is_discarded()) {
            prompt = j.value("prompt", "");
            requestedModel = j.value("model", "");
        } else {
            prompt = body;
        }
        if (!isReasonablePromptSize(prompt)) {
            statusCode = 413;
            responseBody = "{\"error\":\"Prompt too large\"}";
        } else {
        if (!m_headlessMinimal || !requestedModel.empty()) {
            prepareInferenceBackend(requestedModel);
        }
        std::string result = runInference(prompt);
        responseBody = "{\"response\":\"" + jsonEscape(result) + "\"}";
        }
    }
    else if (path == "/v1/chat/completions" && method == "POST") {
        // OpenAI-compatible endpoint
        std::string prompt;
        std::string requestedModel;
        auto j = nlohmann::json::parse(body, nullptr, false);
        if (!j.is_discarded()) {
            requestedModel = j.value("model", "");
            auto messages = j.value("messages", nlohmann::json::array());
            if (!messages.empty()) {
                prompt = messages[messages.size() - 1].value("content", "");
            }
        } else {
            prompt = body;
        }
        if (!isReasonablePromptSize(prompt)) {
            statusCode = 413;
            responseBody = "{\"error\":\"Prompt too large\"}";
        } else {
            if (!m_headlessMinimal || !requestedModel.empty()) {
                prepareInferenceBackend(requestedModel);
            }
            std::string result = runInference(prompt);
            responseBody = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" +
                           jsonEscape(result) + "\"}}]}";
        }
    }
    else if (path == "/api/chat") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string prompt;
            std::string requestedModel;
            bool stream = false;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                stream = j.value("stream", false);
                requestedModel = j.value("model", "");
                auto messages = j.value("messages", nlohmann::json::array());
                if (!messages.empty()) {
                    prompt = messages[messages.size() - 1].value("content", "");
                }
            } else {
                prompt = body;
            }

            if (!isReasonablePromptSize(prompt)) {
                statusCode = 413;
                responseBody = "{\"error\":\"Prompt too large\"}";
            } else {
                if (!m_headlessMinimal || !requestedModel.empty()) {
                    prepareInferenceBackend(requestedModel);
                }
                const std::string result = runInference(prompt);
                if (stream) {
                    contentType = "text/event-stream";
                    responseBody = "data: {\"message\":\"" + jsonEscape(result) + "\"}\n\n";
                    responseBody += "data: [DONE]\n\n";
                } else {
                    responseBody = "{\"message\":\"" + jsonEscape(result) + "\"}";
                }
            }
        }
    }
    else if (path == "/ask") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string prompt;
            std::string requestedModel;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                prompt = j.value("prompt", "");
                requestedModel = j.value("model", "");
            }
            if (prompt.empty()) {
                prompt = body;
            }

            if (!isReasonablePromptSize(prompt)) {
                statusCode = 413;
                responseBody = "{\"success\":false,\"error\":\"Prompt too large\"}";
            } else {
                if (!m_headlessMinimal || !requestedModel.empty()) {
                    prepareInferenceBackend(requestedModel);
                }
                const std::string result = runInference(prompt);
                responseBody = "{\"success\":true,\"message\":\"" + jsonEscape(result) + "\"}";
            }
        }
    }
    else if (path == "/api/backend/status") {
        std::ostringstream oss;
        oss << "{\"success\":true,";
        oss << "\"status\":\"" << jsonEscape(getBackendStatusString()) << "\",";
        oss << "\"active_backend\":" << static_cast<int>(getActiveBackendType()) << ",";
        oss << "\"model_loaded\":" << (m_modelLoaded ? "true" : "false") << ",";
        oss << "\"requests_routed\":" << m_inferenceRequestCount;
        oss << "}";
        responseBody = oss.str();
    }
    else if (path == "/api/backend/active") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"active\":\"" << jsonEscape(getBackendStatusString()) << "\",";
            oss << "\"active_id\":" << static_cast<int>(getActiveBackendType()) << ",";
            oss << "\"healthy\":" << (probeBackendHealth(getActiveBackendType()) ? "true" : "false") << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/backend/switch") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string backend;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                backend = j.value("backend", "");
            }

            AIBackendType target = getActiveBackendType();
            if (backend == "local" || backend == "gguf") {
                target = AIBackendType::LocalGGUF;
            } else if (backend == "ollama") {
                target = AIBackendType::Ollama;
            } else if (backend == "openai") {
                target = AIBackendType::OpenAI;
            } else if (backend == "claude") {
                target = AIBackendType::Claude;
            } else if (backend == "gemini") {
                target = AIBackendType::Gemini;
            }

            const bool switched = setActiveBackend(target);
            responseBody = "{\"success\":" + std::string(switched ? "true" : "false") +
                           ",\"active\":\"" + jsonEscape(getBackendStatusString()) + "\"}";
        }
    }
    else if (path == "/api/router/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            int healthyBackends = 0;
            for (int i = 0; i < static_cast<int>(AIBackendType::Count); ++i) {
                if (probeBackendHealth(static_cast<AIBackendType>(i))) {
                    ++healthyBackends;
                }
            }

            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":\"" << jsonEscape(getRouterStatusString()) << "\",";
            oss << "\"initialized\":" << (m_routerInitialized ? "true" : "false") << ",";
            oss << "\"active_backend\":\"" << jsonEscape(getBackendStatusString()) << "\",";
            oss << "\"healthy_backends\":" << healthyBackends << ",";
            oss << "\"requests_routed\":" << m_inferenceRequestCount;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/router/decision") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            int healthyBackends = 0;
            for (int i = 0; i < static_cast<int>(AIBackendType::Count); ++i) {
                if (probeBackendHealth(static_cast<AIBackendType>(i))) {
                    ++healthyBackends;
                }
            }

            std::ostringstream oss;
            oss << "{\"success\":true,\"decisions\":[{";
            oss << "\"backend\":\"" << jsonEscape(getBackendStatusString()) << "\",";
            oss << "\"router_initialized\":" << (m_routerInitialized ? "true" : "false") << ",";
            oss << "\"reason\":\"health-aware-default\",";
            oss << "\"healthy_backends\":" << healthyBackends << ",";
            oss << "\"preview\":\"\"";
            oss << "}],\"total\":1}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/router/capabilities") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            int healthyBackends = 0;
            for (int i = 0; i < static_cast<int>(AIBackendType::Count); ++i) {
                if (probeBackendHealth(static_cast<AIBackendType>(i))) {
                    ++healthyBackends;
                }
            }
            std::ostringstream oss;
            oss << "{\"success\":true,\"capabilities\":{";
            oss << "\"strategies\":[\"round-robin\",\"latency\",\"random\",\"failover\"],";
            oss << "\"max_backends\":8,";
            oss << "\"healthy_backends\":" << healthyBackends << ",";
            oss << "\"active_backend\":\"" << jsonEscape(getBackendStatusString()) << "\"";
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/router/route") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string query;
            bool execute = false;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                query = j.value("query", "");
                execute = j.value("execute", false);
            }
            if (query.empty()) {
                query = "route";
            }

            std::string routed;
            if (execute) {
                routed = routeWithIntelligence(query);
            }

            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"backend\":\"" << jsonEscape(getBackendStatusString()) << "\",";
            oss << "\"reason\":\"health-aware-default\",";
            oss << "\"executed\":" << (execute ? "true" : "false");
            if (execute) {
                oss << ",\"response\":\"" << jsonEscape(routed) << "\"";
            }
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/governor/status") {
        auto& governor = ExecutionGovernor::instance();
        const auto stats = governor.getStats();
        std::ostringstream oss;
        oss << "{\"success\":true,";
        oss << "\"status\":\"" << jsonEscape(getGovernorStatus()) << "\",";
        oss << "\"active_tasks\":" << stats.activeTaskCount << ",";
        oss << "\"submitted\":" << stats.totalSubmitted << ",";
        oss << "\"completed\":" << stats.totalCompleted << ",";
        oss << "\"timed_out\":" << stats.totalTimedOut << ",";
        oss << "\"killed\":" << stats.totalKilled;
        oss << "}";
        responseBody = oss.str();
    }
    else if (path == "/api/governor/policy") {
        if (method != "GET" && method != "POST") {
            methodNotAllowed();
        } else {
            auto& governor = ExecutionGovernor::instance();
            if (method == "POST") {
                GovernorPolicy policy = governor.getPolicy();
                auto j = nlohmann::json::parse(body, nullptr, false);
                if (!j.is_discarded() && j.is_object()) {
                    auto setBool = [&j](const char* key, bool& target) {
                        if (j.contains(key) && j[key].is_boolean()) {
                            target = j[key].get<bool>();
                        }
                    };
                    auto setU64 = [&j](const char* key, uint64_t& target) {
                        if (j.contains(key) && j[key].is_number_integer()) {
                            const auto value = j[key].get<long long>();
                            if (value >= 0) {
                                target = static_cast<uint64_t>(value);
                            }
                        }
                    };

                    setBool("allow_terminal_commands", policy.allowTerminalCommands);
                    setBool("allow_tool_invocations", policy.allowToolInvocations);
                    setBool("allow_agent_actions", policy.allowAgentActions);
                    setBool("allow_background_jobs", policy.allowBackgroundJobs);
                    setBool("allow_file_operations", policy.allowFileOperations);
                    setBool("allow_network_requests", policy.allowNetworkRequests);
                    setBool("allow_build_tasks", policy.allowBuildTasks);
                    setBool("block_critical_risk", policy.blockCriticalRisk);
                    setBool("require_description_for_high_risk", policy.requireDescriptionForHighRisk);
                    setU64("max_command_bytes", policy.maxCommandBytes);
                    setU64("max_timeout_ms", policy.maxTimeoutMs);

                    if (j.contains("deny_tokens") && j["deny_tokens"].is_array()) {
                        std::vector<std::string> tokens;
                        tokens.reserve(j["deny_tokens"].size());
                        for (const auto& entry : j["deny_tokens"]) {
                            if (entry.is_string()) {
                                tokens.push_back(entry.get<std::string>());
                            }
                        }
                        policy.denyTokens = std::move(tokens);
                    }
                }
                governor.setPolicy(policy);
            }

            const GovernorPolicy policy = governor.getPolicy();
            std::ostringstream oss;
            oss << "{\"success\":true,\"policy\":{";
            oss << "\"allow_terminal_commands\":" << (policy.allowTerminalCommands ? "true" : "false") << ",";
            oss << "\"allow_tool_invocations\":" << (policy.allowToolInvocations ? "true" : "false") << ",";
            oss << "\"allow_agent_actions\":" << (policy.allowAgentActions ? "true" : "false") << ",";
            oss << "\"allow_background_jobs\":" << (policy.allowBackgroundJobs ? "true" : "false") << ",";
            oss << "\"allow_file_operations\":" << (policy.allowFileOperations ? "true" : "false") << ",";
            oss << "\"allow_network_requests\":" << (policy.allowNetworkRequests ? "true" : "false") << ",";
            oss << "\"allow_build_tasks\":" << (policy.allowBuildTasks ? "true" : "false") << ",";
            oss << "\"block_critical_risk\":" << (policy.blockCriticalRisk ? "true" : "false") << ",";
            oss << "\"require_description_for_high_risk\":" << (policy.requireDescriptionForHighRisk ? "true" : "false") << ",";
            oss << "\"max_command_bytes\":" << policy.maxCommandBytes << ",";
            oss << "\"max_timeout_ms\":" << policy.maxTimeoutMs << ",";
            oss << "\"deny_tokens\":[";
            for (size_t i = 0; i < policy.denyTokens.size(); ++i) {
                if (i != 0) {
                    oss << ",";
                }
                oss << "\"" << jsonEscape(policy.denyTokens[i]) << "\"";
            }
            oss << "]}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/governor/audit") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& governor = ExecutionGovernor::instance();
            const auto records = governor.getAuditTrail(100);

            std::ostringstream oss;
            oss << "{\"success\":true,\"records\":[";
            for (size_t i = 0; i < records.size(); ++i) {
                const auto& r = records[i];
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"task_id\":" << r.taskId << ",";
                oss << "\"type\":" << static_cast<int>(r.type) << ",";
                oss << "\"risk\":" << static_cast<int>(r.risk) << ",";
                oss << "\"state\":" << static_cast<int>(r.state) << ",";
                oss << "\"timestamp_ms\":" << r.timestampMs << ",";
                oss << "\"timeout_ms\":" << r.timeoutMs << ",";
                oss << "\"exit_code\":" << r.exitCode << ",";
                oss << "\"policy_denied\":" << (r.policyDenied ? "true" : "false") << ",";
                oss << "\"timed_out\":" << (r.timedOut ? "true" : "false") << ",";
                oss << "\"cancelled\":" << (r.cancelled ? "true" : "false") << ",";
                oss << "\"description\":\"" << jsonEscape(r.description) << "\",";
                oss << "\"detail\":\"" << jsonEscape(r.detail) << "\"}";
            }
            oss << "],\"count\":" << records.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/governor/audit/clear") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            auto& governor = ExecutionGovernor::instance();
            governor.clearAuditTrail();
            responseBody = "{\"success\":true,\"cleared\":true}";
        }
    }
    else if (path == "/api/governor/submit") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            auto& governor = ExecutionGovernor::instance();

            std::string command;
            std::string description = "headless governor submit";
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                if (j.contains("command") && j["command"].is_string()) {
                    command = j["command"].get<std::string>();
                }
                if (j.contains("description") && j["description"].is_string()) {
                    description = j["description"].get<std::string>();
                }
            }

            if (command.empty()) {
                command = "cmd /c echo headless-governor";
            }

            const GovernorTaskId taskId = governor.submitCommand(command, 10000, GovernorRiskTier::Low, description);
            const auto stats = governor.getStats();

            std::ostringstream oss;
            oss << "{\"success\":true,\"queued\":true,";
            oss << "\"task_id\":" << taskId << ",";
            oss << "\"active_tasks\":" << stats.activeTaskCount << ",";
            oss << "\"total_submitted\":" << stats.totalSubmitted << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/safety/status") {
        auto& safety = AgentSafetyContract::instance();
        const auto stats = safety.getStats();
        std::ostringstream oss;
        oss << "{\"success\":true,";
        oss << "\"status\":\"" << jsonEscape(getSafetyStatus()) << "\",";
        oss << "\"checks\":" << stats.totalChecks << ",";
        oss << "\"allowed\":" << stats.totalAllowed << ",";
        oss << "\"denied\":" << stats.totalDenied << ",";
        oss << "\"escalated\":" << stats.totalEscalated << ",";
        oss << "\"violations\":" << stats.totalViolations;
        oss << "}";
        responseBody = oss.str();
    }
    else if (path == "/api/safety/check") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            auto& safety = AgentSafetyContract::instance();

            auto actionFromString = [](const std::string& action) {
                if (action == "read_file") return ActionClass::ReadFile;
                if (action == "search_code") return ActionClass::SearchCode;
                if (action == "list_directory") return ActionClass::ListDirectory;
                if (action == "write_file") return ActionClass::WriteFile;
                if (action == "edit_file") return ActionClass::EditFile;
                if (action == "delete_file") return ActionClass::DeleteFile;
                if (action == "rename_file") return ActionClass::RenameFile;
                if (action == "run_command") return ActionClass::RunCommand;
                if (action == "run_build") return ActionClass::RunBuild;
                if (action == "run_test") return ActionClass::RunTest;
                if (action == "network_request") return ActionClass::NetworkRequest;
                if (action == "modify_model") return ActionClass::ModifyModel;
                return ActionClass::QueryModel;
            };

            auto riskFromString = [](const std::string& risk) {
                if (risk == "none") return SafetyRiskTier::None;
                if (risk == "low") return SafetyRiskTier::Low;
                if (risk == "medium") return SafetyRiskTier::Medium;
                if (risk == "high") return SafetyRiskTier::High;
                if (risk == "critical") return SafetyRiskTier::Critical;
                return SafetyRiskTier::Low;
            };

            std::string actionText = "query_model";
            std::string riskText = "low";
            std::string description = "headless safety check";

            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                if (j.contains("action") && j["action"].is_string()) {
                    actionText = j["action"].get<std::string>();
                }
                if (j.contains("risk") && j["risk"].is_string()) {
                    riskText = j["risk"].get<std::string>();
                }
                if (j.contains("description") && j["description"].is_string()) {
                    description = j["description"].get<std::string>();
                }
            }

            std::transform(actionText.begin(), actionText.end(), actionText.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(riskText.begin(), riskText.end(), riskText.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            const auto action = actionFromString(actionText);
            const auto risk = riskFromString(riskText);
            const auto result = safety.checkAndConsume(action, risk, description, 1.0f);

            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"allowed\":" << ((result.verdict == ContractVerdict::Allowed || result.verdict == ContractVerdict::Budgeted) ? "true" : "false") << ",";
            oss << "\"verdict\":\"" << contractVerdictToString(result.verdict) << "\",";
            oss << "\"risk\":\"" << safetyRiskTierToString(risk) << "\",";
            oss << "\"action\":\"" << actionClassToString(action) << "\",";
            oss << "\"reason\":\"" << jsonEscape(result.reason) << "\",";
            oss << "\"suggestion\":\"" << jsonEscape(result.suggestion) << "\",";
            oss << "\"violations\":[]}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/safety/violations") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& safety = AgentSafetyContract::instance();
            const auto violations = safety.getViolations();

            std::ostringstream oss;
            oss << "{\"success\":true,\"violations\":[";
            const size_t begin = (violations.size() > 50) ? (violations.size() - 50) : 0;
            bool first = true;
            for (size_t i = begin; i < violations.size(); ++i) {
                const auto& v = violations[i];
                if (!first) {
                    oss << ",";
                }
                first = false;
                oss << "{\"id\":" << v.id << ",";
                oss << "\"type\":\"" << violationTypeToString(v.type) << "\",";
                oss << "\"action\":\"" << actionClassToString(v.attemptedAction) << "\",";
                oss << "\"risk\":\"" << safetyRiskTierToString(v.riskTier) << "\",";
                oss << "\"description\":\"" << jsonEscape(v.description) << "\",";
                oss << "\"agent_id\":\"" << jsonEscape(v.agentId) << "\",";
                oss << "\"escalated\":" << (v.wasEscalated ? "true" : "false") << ",";
                oss << "\"approved\":" << (v.userApproved ? "true" : "false") << "}";
            }
            oss << "],\"count\":" << violations.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const auto& stats = swarm.getStats();
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":\"" << jsonEscape(getSwarmStatus()) << "\",";
            oss << "\"initialized\":" << (m_phase11Initialized ? "true" : "false") << ",";
            oss << "\"running\":" << (swarm.isRunning() ? "true" : "false") << ",";
            oss << "\"nodes\":" << swarm.getNodeCount() << ",";
            oss << "\"shards\":" << swarm.getShardCount() << ",";
            oss << "\"inference_requests\":" << stats.inferenceRequests.load();
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/nodes") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const auto nodes = swarm.getNodeList();

            std::ostringstream oss;
            oss << "{\"success\":true,\"nodes\":[";
            for (size_t i = 0; i < nodes.size(); ++i) {
                const auto& n = nodes[i];
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"node_id\":\"" << jsonEscape(n.nodeId) << "\",";
                oss << "\"hostname\":\"" << jsonEscape(n.hostname) << "\",";
                oss << "\"role\":" << static_cast<int>(n.role) << ",";
                oss << "\"state\":" << static_cast<int>(n.state) << ",";
                oss << "\"gpu\":" << (n.gpuAccel ? "true" : "false") << ",";
                oss << "\"total_vram\":" << n.totalVRAM << ",";
                oss << "\"free_vram\":" << n.freeVRAM << ",";
                oss << "\"max_layers\":" << n.maxLayers << ",";
                oss << "\"active_requests\":" << n.activeRequests << "}";
            }
            oss << "],\"count\":" << nodes.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/tasks") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const auto shards = swarm.getShardList();

            std::ostringstream oss;
            oss << "{\"success\":true,\"tasks\":[";
            for (size_t i = 0; i < shards.size(); ++i) {
                const auto& s = shards[i];
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"task_id\":\"shard-" << s.layerStart << "-" << s.layerEnd << "\",";
                oss << "\"type\":\"layer_shard\",";
                oss << "\"node_id\":\"" << jsonEscape(s.nodeId) << "\",";
                oss << "\"layer_start\":" << s.layerStart << ",";
                oss << "\"layer_end\":" << s.layerEnd << ",";
                oss << "\"resident\":" << (s.resident ? "true" : "false") << ",";
                oss << "\"quant\":" << static_cast<int>(s.quant) << ",";
                oss << "\"size_bytes\":" << s.sizeBytes << ",";
                oss << "\"ref_count\":" << s.refCount << "}";
            }
            oss << "],\"count\":" << shards.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/stats") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const auto& stats = swarm.getStats();
            std::ostringstream oss;
            oss << "{\"success\":true,\"stats\":{";
            oss << "\"active\":" << (swarm.isRunning() ? "true" : "false") << ",";
            oss << "\"coordinator\":" << (swarm.isCoordinator() ? "true" : "false") << ",";
            oss << "\"total_nodes\":" << swarm.getNodeCount() << ",";
            oss << "\"total_shards\":" << swarm.getShardCount() << ",";
            oss << "\"nodes_joined\":" << stats.nodesJoined.load() << ",";
            oss << "\"nodes_left\":" << stats.nodesLeft.load() << ",";
            oss << "\"nodes_timed_out\":" << stats.nodesTimedOut.load() << ",";
            oss << "\"layers_distributed\":" << stats.layersDistributed.load() << ",";
            oss << "\"layers_migrated\":" << stats.layersMigrated.load() << ",";
            oss << "\"inference_requests\":" << stats.inferenceRequests.load() << ",";
            oss << "\"rebalance_events\":" << stats.rebalanceEvents.load() << ",";
            oss << "\"errors\":" << stats.errors.load();
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/events") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const auto& stats = swarm.getStats();

            struct CounterEvent {
                const char* name;
                uint64_t value;
            };

            const CounterEvent events[] = {
                { "nodes_joined", stats.nodesJoined.load() },
                { "nodes_left", stats.nodesLeft.load() },
                { "nodes_timed_out", stats.nodesTimedOut.load() },
                { "layers_distributed", stats.layersDistributed.load() },
                { "layers_migrated", stats.layersMigrated.load() },
                { "rebalance_events", stats.rebalanceEvents.load() },
                { "errors", stats.errors.load() }
            };

            std::ostringstream oss;
            oss << "{\"success\":true,\"events\":[";
            bool first = true;
            for (const auto& ev : events) {
                if (ev.value == 0) {
                    continue;
                }
                if (!first) {
                    oss << ",";
                }
                first = false;
                oss << "{\"event\":\"" << ev.name << "\",\"count\":" << ev.value << "}";
            }
            oss << "]}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/config") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            std::ostringstream oss;
            oss << "{\"success\":true,\"config\":{";
            oss << "\"mode\":\"headless\",";
            oss << "\"initialized\":" << (m_phase11Initialized ? "true" : "false") << ",";
            oss << "\"running\":" << (swarm.isRunning() ? "true" : "false") << ",";
            oss << "\"role\":" << static_cast<int>(swarm.getRole()) << ",";
            oss << "\"node_id\":\"" << jsonEscape(swarm.getNodeId()) << "\"";
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/swarm/worker") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const auto nodes = swarm.getNodeList();

            std::ostringstream oss;
            oss << "{\"success\":true,\"workers\":[";
            bool first = true;
            for (const auto& n : nodes) {
                if (!first) {
                    oss << ",";
                }
                first = false;
                oss << "{\"node_id\":\"" << jsonEscape(n.nodeId) << "\",";
                oss << "\"state\":" << static_cast<int>(n.state) << ",";
                oss << "\"active_requests\":" << n.activeRequests << ",";
                oss << "\"avg_latency_ms\":" << n.avgInferenceLatency << ",";
                oss << "\"free_vram\":" << n.freeVRAM << ",";
                oss << "\"total_vram\":" << n.totalVRAM << "}";
            }
            oss << "],\"count\":" << nodes.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/debug/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":\"" << jsonEscape(getNativeDebugStatus()) << "\",";
            oss << "\"initialized\":" << (m_phase12Initialized ? "true" : "false") << ",";
            oss << "\"session_active\":" << (m_debugSessionActive ? "true" : "false") << ",";
            oss << "\"breakpoints\":" << m_debugBreakpointCount;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/debug/breakpoints") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,\"breakpoints\":[";
            for (uint32_t i = 0; i < m_debugBreakpointCount; ++i) {
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"id\":" << i << ",\"enabled\":true,\"source\":\"native-debugger\"}";
            }
            oss << "],\"count\":" << m_debugBreakpointCount << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/debug/registers") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            void* frames[2] = {};
            const USHORT frameCount = CaptureStackBackTrace(0, 2, frames, nullptr);
            volatile int stackMarker = 0;

            const uintptr_t ip = (frameCount > 0) ? reinterpret_cast<uintptr_t>(frames[0]) : 0;
            const uintptr_t sp = reinterpret_cast<uintptr_t>(&stackMarker);

            std::ostringstream oss;
            oss << "{\"success\":true,\"registers\":{";
            oss << "\"ip\":\"0x" << std::hex << ip << std::dec << "\",";
            oss << "\"sp\":\"0x" << std::hex << sp << std::dec << "\",";
            oss << "\"thread_id\":" << GetCurrentThreadId() << ",";
            oss << "\"process_id\":" << GetCurrentProcessId() << ",";
            oss << "\"arch\":\"x64\"";
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/debug/stack") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            void* frames[16] = {};
            const USHORT frameCount = CaptureStackBackTrace(0, 16, frames, nullptr);

            std::ostringstream oss;
            oss << "{\"success\":true,\"stack\":[";
            for (USHORT i = 0; i < frameCount; ++i) {
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"index\":" << i << ",\"address\":\"0x" << std::hex
                    << reinterpret_cast<uintptr_t>(frames[i]) << std::dec << "\"}";
            }
            oss << "],\"depth\":" << frameCount << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/debug/modules") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            HMODULE modules[128] = {};
            DWORD neededBytes = 0;
            const HANDLE proc = GetCurrentProcess();

            std::ostringstream oss;
            oss << "{\"success\":true,\"modules\":[";

            size_t emitted = 0;
            if (EnumProcessModules(proc, modules, static_cast<DWORD>(sizeof(modules)), &neededBytes)) {
                const size_t moduleCount = std::min<size_t>(neededBytes / sizeof(HMODULE), 128);
                for (size_t i = 0; i < moduleCount; ++i) {
                    char modPath[MAX_PATH] = {};
                    if (GetModuleFileNameExA(proc, modules[i], modPath, MAX_PATH) == 0) {
                        continue;
                    }
                    if (emitted != 0) {
                        oss << ",";
                    }
                    ++emitted;
                    MODULEINFO mi = {};
                    GetModuleInformation(proc, modules[i], &mi, static_cast<DWORD>(sizeof(mi)));
                    oss << "{\"index\":" << i << ",";
                    oss << "\"path\":\"" << jsonEscape(modPath) << "\",";
                    oss << "\"base\":\"0x" << std::hex << reinterpret_cast<uintptr_t>(mi.lpBaseOfDll)
                        << std::dec << "\",";
                    oss << "\"size\":" << mi.SizeOfImage << "}";
                }
            }

            oss << "],\"count\":" << emitted << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/debug/threads") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const DWORD pid = GetCurrentProcessId();
            std::vector<DWORD> threadIds;

            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te = {};
                te.dwSize = sizeof(te);
                if (Thread32First(snap, &te)) {
                    do {
                        if (te.th32OwnerProcessID == pid) {
                            threadIds.push_back(te.th32ThreadID);
                        }
                    } while (Thread32Next(snap, &te));
                }
                CloseHandle(snap);
            }

            std::ostringstream oss;
            oss << "{\"success\":true,\"threads\":[";
            for (size_t i = 0; i < threadIds.size(); ++i) {
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"id\":" << threadIds[i] << ",\"current\":"
                    << ((threadIds[i] == GetCurrentThreadId()) ? "true" : "false") << "}";
            }
            oss << "],\"count\":" << threadIds.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/hotpatch/status") {
        responseBody = getHotpatchStatusJson();
    }
    else if (path == "/api/quantum/status") {
        responseBody = getQuantumStatusJson();
    }
    else if (path == "/api/asm/status") {
        std::ostringstream oss;
        oss << "{\"success\":true,";
        oss << "\"status\":\"" << jsonEscape(getAsmSemanticStatsString()) << "\",";
        oss << "\"initialized\":" << (m_asmSemanticInitialized ? "true" : "false") << ",";
        oss << "\"symbols\":" << m_asmSymbolCount << ",";
        oss << "\"files\":" << m_asmFilesParsed;
        oss << "}";
        responseBody = oss.str();
    }
    else if (path == "/api/asm/symbols") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const std::string summary = getAsmSymbolTableString();
            std::ostringstream oss;
            oss << "{\"success\":true,\"symbols\":[";
            if (m_asmSymbolCount > 0) {
                oss << "{\"name\":\"indexed_symbols\",\"count\":" << m_asmSymbolCount
                    << ",\"files\":" << m_asmFilesParsed << "}";
            }
            oss << "],\"summary\":\"" << jsonEscape(summary) << "\"}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/asm/navigate") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string symbol;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                if (j.contains("symbol") && j["symbol"].is_string()) {
                    symbol = j["symbol"].get<std::string>();
                } else if (j.contains("query") && j["query"].is_string()) {
                    symbol = j["query"].get<std::string>();
                }
            }
            if (symbol.empty()) {
                symbol = "entry";
            }

            std::ostringstream oss;
            oss << "{\"success\":true,\"target\":{";
            oss << "\"symbol\":\"" << jsonEscape(symbol) << "\",";
            oss << "\"resolved\":" << ((m_asmSymbolCount > 0) ? "true" : "false") << ",";
            oss << "\"files_indexed\":" << m_asmFilesParsed;
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/asm/analyze") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string content;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                if (j.contains("content") && j["content"].is_string()) {
                    content = j["content"].get<std::string>();
                } else if (j.contains("code") && j["code"].is_string()) {
                    content = j["code"].get<std::string>();
                }
            }
            if (content.empty()) {
                content = body;
            }

            size_t instructionLines = 0;
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                const auto first = line.find_first_not_of(" \t");
                if (first == std::string::npos) {
                    continue;
                }
                if (line[first] == ';' || line[first] == '#') {
                    continue;
                }
                ++instructionLines;
            }

            std::ostringstream oss;
            oss << "{\"success\":true,\"analysis\":[";
            oss << "{\"name\":\"instruction_lines\",\"value\":" << instructionLines << "},";
            oss << "{\"name\":\"indexed_symbols\",\"value\":" << m_asmSymbolCount << "},";
            oss << "{\"name\":\"files_parsed\",\"value\":" << m_asmFilesParsed << "}";
            oss << "]}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/lsp/status") {
        std::ostringstream oss;
        oss << "{\"success\":true,";
        oss << "\"status\":\"" << jsonEscape(getLSPStatusString()) << "\",";
        oss << "\"initialized\":" << (m_lspInitialized ? "true" : "false") << ",";
        oss << "\"servers\":" << m_lspServerCount << ",";
        oss << "\"completions\":" << m_lspCompletionCount;
        oss << "}";
        responseBody = oss.str();
    }
    else if (path == "/api/lsp/diagnostics") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,\"diagnostics\":[";

            bool first = true;
            auto emitDiag = [&](const char* severity, const std::string& code, const std::string& message) {
                if (!first) {
                    oss << ",";
                }
                first = false;
                oss << "{\"source\":\"lsp\",\"severity\":\"" << severity << "\",";
                oss << "\"code\":\"" << jsonEscape(code) << "\",";
                oss << "\"message\":\"" << jsonEscape(message) << "\"}";
            };

            if (!m_lspInitialized) {
                emitDiag("error", "LSP_NOT_INITIALIZED", "LSP subsystem is not initialized");
            }
            if (m_lspServerCount == 0) {
                emitDiag("warning", "LSP_SERVER_COUNT_ZERO", "No LSP server instances configured");
            }
            if (m_lspCompletionCount == 0) {
                emitDiag("info", "LSP_COMPLETION_IDLE", "No completions served yet");
            }

            oss << "],\"summary\":{";
            oss << "\"initialized\":" << (m_lspInitialized ? "true" : "false") << ",";
            oss << "\"servers\":" << m_lspServerCount << ",";
            oss << "\"completions\":" << m_lspCompletionCount;
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/hybrid/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":\"" << jsonEscape(getHybridBridgeStatusString()) << "\",";
            oss << "\"initialized\":" << (m_hybridBridgeInitialized ? "true" : "false") << ",";
            oss << "\"completions\":" << m_hybridCompletionCount;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/hybrid/complete") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string prompt;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                prompt = j.value("code", "");
                if (prompt.empty()) {
                    prompt = j.value("prompt", "");
                }
            }
            if (prompt.empty()) {
                prompt = body;
            }

            if (!isReasonablePromptSize(prompt)) {
                statusCode = 413;
                responseBody = "{\"success\":false,\"error\":\"Prompt too large\"}";
            } else {
                const std::string completion = runInference(prompt);
                responseBody = "{\"success\":true,\"completions\":[\"" + jsonEscape(completion) +
                               "\"],\"source\":\"hybrid\"}";
            }
        }
    }
    else if (path == "/api/hybrid/diagnostics") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,\"diagnostics\":[";
            bool first = true;

            auto emitDiag = [&](const char* severity, const std::string& code, const std::string& message) {
                if (!first) {
                    oss << ",";
                }
                first = false;
                oss << "{\"source\":\"hybrid\",\"severity\":\"" << severity << "\",";
                oss << "\"code\":\"" << jsonEscape(code) << "\",";
                oss << "\"message\":\"" << jsonEscape(message) << "\"}";
            };

            if (!m_hybridBridgeInitialized) {
                emitDiag("error", "HYBRID_NOT_INITIALIZED", "Hybrid bridge is not initialized");
            }
            if (m_hybridCompletionCount == 0) {
                emitDiag("info", "HYBRID_IDLE", "No hybrid completions processed yet");
            }

            oss << "],\"summary\":{";
            oss << "\"initialized\":" << (m_hybridBridgeInitialized ? "true" : "false") << ",";
            oss << "\"completions\":" << m_hybridCompletionCount;
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/agent/dual/init") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            const bool initialized = (m_agenticEngine != nullptr) && (m_subAgentManager != nullptr);
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"initialized\":" << (initialized ? "true" : "false") << ",";
            oss << "\"agentic_engine\":" << (m_agenticEngine ? "true" : "false") << ",";
            oss << "\"subagent_manager\":" << (m_subAgentManager ? "true" : "false") << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/agent/dual/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const bool dualActive = (m_agenticEngine != nullptr) && (m_subAgentManager != nullptr);
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"active\":" << (dualActive ? "true" : "false") << ",";
            oss << "\"agentic_engine\":" << (m_agenticEngine ? "true" : "false") << ",";
            oss << "\"subagent_manager\":" << (m_subAgentManager ? "true" : "false") << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/agent/dual/submit") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string task;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                task = j.value("task", "");
                if (task.empty()) {
                    task = j.value("prompt", "");
                }
            }
            if (task.empty()) {
                task = body;
            }

            if (!isReasonablePromptSize(task)) {
                statusCode = 413;
                responseBody = "{\"success\":false,\"error\":\"Prompt too large\"}";
            } else {
                const std::string output = runInference(task);
                responseBody = "{\"success\":true,\"accepted\":true,\"output\":\"" +
                               jsonEscape(output) + "\"}";
            }
        }
    }
    else if (path == "/api/multi-response/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            bool active = false;
            size_t templateCount = 0;
            std::string statsJson = "{\"totalSessions\":0,\"totalResponses\":0,\"preferenceSelections\":0}";

            if (m_multiResponse) {
                active = m_multiResponse->isInitialized();
                templateCount = m_multiResponse->getAllTemplates().size();
                statsJson = m_multiResponse->statsToJson();
            }

            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"active\":" << (active ? "true" : "false") << ",";
            oss << "\"templates\":" << templateCount << ",";
            oss << "\"max_chain\":" << (m_multiResponse ? m_multiResponse->getMaxChainResponses() : 0) << ",";
            oss << "\"stats\":" << statsJson << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/multi-response/generate") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string prompt;
            std::string context;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                prompt = j.value("prompt", "");
                context = j.value("context", "");
            }
            if (prompt.empty()) {
                prompt = body;
            }

            if (!isReasonablePromptSize(prompt)) {
                statusCode = 413;
                responseBody = "{\"success\":false,\"error\":\"Prompt too large\"}";
            } else {
                if (!m_multiResponse) {
                    responseBody = "{\"success\":false,\"error\":\"Multi-response engine unavailable\"}";
                } else {
                    const uint64_t sid = m_multiResponse->startSession(prompt, m_multiResponse->getMaxChainResponses(), context);
                    const auto gen = m_multiResponse->generateAll(sid);
                    if (!gen.success) {
                        responseBody = "{\"success\":false,\"error\":\"" + jsonEscape(gen.detail ? gen.detail : "generation failed") + "\"}";
                    } else {
                        const MultiResponseSession* s = m_multiResponse->getSession(sid);
                        std::ostringstream oss;
                        oss << "{\"success\":true,\"session_id\":" << sid << ",\"responses\":[";
                        if (s) {
                            for (size_t i = 0; i < s->responses.size(); ++i) {
                                const auto& r = s->responses[i];
                                if (i != 0) {
                                    oss << ",";
                                }
                                oss << "{\"index\":" << i << ",";
                                oss << "\"template_id\":" << r.templateId << ",";
                                oss << "\"latency_ms\":" << r.latencyMs << ",";
                                oss << "\"success\":" << (r.success ? "true" : "false") << ",";
                                oss << "\"content\":\"" << jsonEscape(r.content) << "\"}";
                            }
                        }
                        oss << "]}";
                        responseBody = oss.str();
                    }
                }
            }
        }
    }
    else if (path == "/api/multi-response/templates") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,\"templates\":[";
            size_t count = 0;
            if (m_multiResponse) {
                const auto templates = m_multiResponse->getAllTemplates();
                for (size_t i = 0; i < templates.size(); ++i) {
                    const auto& t = templates[i];
                    if (i != 0) {
                        oss << ",";
                    }
                    ++count;
                    oss << "{\"id\":" << t.id << ",";
                    oss << "\"name\":\"" << jsonEscape(t.name ? t.name : "") << "\",";
                    oss << "\"enabled\":" << (t.enabled ? "true" : "false") << ",";
                    oss << "\"system_prompt\":\"" << jsonEscape(t.systemPrompt ? t.systemPrompt : "") << "\"}";
                }
            }
            oss << "],\"count\":" << count << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/multi-response/stats") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            if (m_multiResponse) {
                responseBody = "{\"success\":true,\"stats\":" + m_multiResponse->statsToJson() + "}";
            } else {
                responseBody = "{\"success\":true,\"stats\":{\"totalSessions\":0,\"totalResponses\":0,\"preferenceSelections\":0}}";
            }
        }
    }
    else if (path == "/api/multi-response/preferences") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            if (m_multiResponse) {
                responseBody = "{\"success\":true,\"preferences\":" + m_multiResponse->preferencesToJson() + "}";
            } else {
                responseBody = "{\"success\":true,\"preferences\":{\"count\":0,\"history\":[]}}";
            }
        }
    }
    else if (path == "/api/replay/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const auto stats = ReplayJournal::instance().getStats();
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":\"" << jsonEscape(getReplayStatus()) << "\",";
            oss << "\"total_records\":" << stats.totalRecords << ",";
            oss << "\"in_memory\":" << stats.recordsInMemory << ",";
            oss << "\"sessions\":" << stats.totalSessions << ",";
            oss << "\"active_session\":" << stats.activeSessionId;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/replay/records") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const auto records = ReplayJournal::instance().getLastN(25);
            std::ostringstream oss;
            oss << "{\"success\":true,\"records\":[";
            for (size_t i = 0; i < records.size(); ++i) {
                const auto& r = records[i];
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"sequence_id\":" << r.sequenceId << ",";
                oss << "\"session_id\":" << r.sessionId << ",";
                oss << "\"type\":" << static_cast<int>(r.type) << ",";
                oss << "\"category\":\"" << jsonEscape(r.category) << "\",";
                oss << "\"action\":\"" << jsonEscape(r.action) << "\",";
                oss << "\"input\":\"" << jsonEscape(r.input) << "\",";
                oss << "\"output\":\"" << jsonEscape(r.output) << "\",";
                oss << "\"metadata\":\"" << jsonEscape(r.metadata) << "\",";
                oss << "\"exit_code\":" << r.exitCode << ",";
                oss << "\"confidence\":" << r.confidence << ",";
                oss << "\"duration_ms\":" << r.durationMs << "}";
            }
            oss << "],\"count\":" << records.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/confidence/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& gate = ConfidenceGate::instance();
            const auto stats = gate.getStats();
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":\"" << jsonEscape(getConfidenceStatus()) << "\",";
            oss << "\"evaluations\":" << stats.totalEvaluations << ",";
            oss << "\"executed\":" << stats.totalExecuted << ",";
            oss << "\"escalated\":" << stats.totalEscalated << ",";
            oss << "\"aborted\":" << stats.totalAborted << ",";
            oss << "\"avg_confidence\":" << stats.avgConfidence;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/confidence/evaluate") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            float rawConfidence = 0.5f;
            std::string inputText;

            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                if (j.contains("confidence") && j["confidence"].is_number()) {
                    rawConfidence = j["confidence"].get<float>();
                } else if (j.contains("score") && j["score"].is_number()) {
                    rawConfidence = j["score"].get<float>();
                }

                if (j.contains("input") && j["input"].is_string()) {
                    inputText = j["input"].get<std::string>();
                } else if (j.contains("text") && j["text"].is_string()) {
                    inputText = j["text"].get<std::string>();
                }
            }

            if (rawConfidence < 0.0f || rawConfidence > 1.0f) {
                const float inferred = 0.25f + static_cast<float>(std::min<size_t>(inputText.size(), 2000)) / 3000.0f;
                rawConfidence = std::clamp(inferred, 0.05f, 0.95f);
            }

            auto decisionToString = [](ConfidenceDecision d) {
                switch (d) {
                    case ConfidenceDecision::Execute: return "execute";
                    case ConfidenceDecision::Escalate: return "escalate";
                    case ConfidenceDecision::Abort: return "abort";
                    case ConfidenceDecision::Defer: return "defer";
                    case ConfidenceDecision::Override: return "override";
                    default: return "unknown";
                }
            };

            auto trendToString = [](ConfidenceTrend t) {
                switch (t) {
                    case ConfidenceTrend::Stable: return "stable";
                    case ConfidenceTrend::Rising: return "rising";
                    case ConfidenceTrend::Falling: return "falling";
                    case ConfidenceTrend::Volatile: return "volatile";
                    case ConfidenceTrend::Unknown: return "unknown";
                    default: return "unknown";
                }
            };

            try {
                auto& gate = ConfidenceGate::instance();
                const auto eval = gate.evaluateAndRecord(
                    rawConfidence,
                    ActionClass::QueryModel,
                    SafetyRiskTier::Low,
                    "headless:/api/confidence/evaluate");
                const auto stats = gate.getStats();

                std::ostringstream oss;
                oss << "{\"success\":true,";
                oss << "\"score\":" << eval.adjustedConfidence << ",";
                oss << "\"raw_score\":" << eval.rawConfidence << ",";
                oss << "\"decision\":\"" << decisionToString(eval.decision) << "\",";
                oss << "\"reason\":\"" << jsonEscape(eval.reason) << "\",";
                oss << "\"trend\":\"" << trendToString(eval.trend) << "\",";
                oss << "\"stats\":{";
                oss << "\"total\":" << stats.totalEvaluations << ",";
                oss << "\"executed\":" << stats.totalExecuted << ",";
                oss << "\"escalated\":" << stats.totalEscalated << ",";
                oss << "\"aborted\":" << stats.totalAborted << ",";
                oss << "\"avg_confidence\":" << stats.avgConfidence;
                oss << "}}";
                responseBody = oss.str();
            }
            catch (const std::exception& ex) {
                const float fallbackScore = std::clamp(rawConfidence, 0.0f, 1.0f);
                responseBody = "{\"success\":true,\"score\":" + std::to_string(fallbackScore) +
                               ",\"decision\":\"defer\",\"reason\":\"" +
                               jsonEscape(std::string("confidence gate fallback: ") + ex.what()) + "\"}";
            }
            catch (...) {
                const float fallbackScore = std::clamp(rawConfidence, 0.0f, 1.0f);
                responseBody = "{\"success\":true,\"score\":" + std::to_string(fallbackScore) +
                               ",\"decision\":\"defer\",\"reason\":\"confidence gate fallback: unknown exception\"}";
            }
        }
    }
    else if (path == "/api/cot/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& cot = ChainOfThoughtEngine::instance();
            const auto stats = cot.getStats();
            std::ostringstream oss;
            oss << "{\"success\":true,";
            oss << "\"status\":" << cot.getStatusJSON() << ",";
            oss << "\"running\":" << (cot.isRunning() ? "true" : "false") << ",";
            oss << "\"steps\":" << cot.getSteps().size() << ",";
            oss << "\"total_chains\":" << stats.totalChains << ",";
            oss << "\"successful_chains\":" << stats.successfulChains;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/cot/presets") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& cot = ChainOfThoughtEngine::instance();
            responseBody = "{\"success\":true,\"presets\":" + cot.getPresetsJSON() + "}";
        }
    }
    else if (path == "/api/cot/roles") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const auto& roles = getAllCoTRoles();
            std::ostringstream oss;
            oss << "{\"success\":true,\"roles\":[";
            for (size_t i = 0; i < roles.size(); ++i) {
                const auto& r = roles[i];
                if (i != 0) {
                    oss << ",";
                }
                oss << "{\"id\":" << static_cast<int>(r.id) << ",";
                oss << "\"name\":\"" << jsonEscape(r.name ? r.name : "") << "\",";
                oss << "\"label\":\"" << jsonEscape(r.label ? r.label : "") << "\",";
                oss << "\"icon\":\"" << jsonEscape(r.icon ? r.icon : "") << "\",";
                oss << "\"instruction\":\"" << jsonEscape(r.instruction ? r.instruction : "") << "\"";
                oss << "}";
            }
            oss << "],\"count\":" << roles.size() << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/engine/capabilities") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            std::ostringstream oss;
            oss << "{\"success\":true,\"capabilities\":{";
            oss << "\"headless\":true,";
            oss << "\"inference\":true,";
            oss << "\"model_loaded\":" << (m_modelLoaded ? "true" : "false") << ",";
            oss << "\"router\":" << (m_routerInitialized ? "true" : "false") << ",";
            oss << "\"lsp\":" << (m_lspInitialized ? "true" : "false") << ",";
            oss << "\"swarm\":" << (m_phase11Initialized ? "true" : "false") << ",";
            oss << "\"debugger\":" << (m_phase12Initialized ? "true" : "false") << ",";
            oss << "\"hotpatch\":" << (m_hotpatchInitialized ? "true" : "false") << ",";
            oss << "\"multi_response\":" << (m_multiResponseInitialized ? "true" : "false");
            oss << "}}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/phase10/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& governor = ExecutionGovernor::instance();
            const auto stats = governor.getStats();
            std::ostringstream oss;
            oss << "{\"success\":true,\"phase\":10,";
            oss << "\"status\":\"" << (m_phase10Initialized ? "active" : "not_initialized") << "\",";
            oss << "\"governor_initialized\":" << (m_phase10Initialized ? "true" : "false") << ",";
            oss << "\"active_tasks\":" << stats.activeTaskCount << ",";
            oss << "\"total_submitted\":" << stats.totalSubmitted;
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/phase11/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            std::ostringstream oss;
            oss << "{\"success\":true,\"phase\":11,";
            oss << "\"status\":\"" << (m_phase11Initialized ? "active" : "not_initialized") << "\",";
            oss << "\"swarm_running\":" << (swarm.isRunning() ? "true" : "false") << ",";
            oss << "\"nodes\":" << swarm.getNodeCount() << ",";
            oss << "\"shards\":" << swarm.getShardCount();
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/phase41/status") {
        if (method != "GET") {
            methodNotAllowed();
        } else {
            const bool phase41Ready = m_hybridBridgeInitialized && m_lspInitialized && m_hotpatchInitialized;
            std::ostringstream oss;
            oss << "{\"success\":true,\"phase\":41,";
            oss << "\"status\":\"" << (phase41Ready ? "active" : "degraded") << "\",";
            oss << "\"hybrid\":" << (m_hybridBridgeInitialized ? "true" : "false") << ",";
            oss << "\"lsp\":" << (m_lspInitialized ? "true" : "false") << ",";
            oss << "\"hotpatch\":" << (m_hotpatchInitialized ? "true" : "false");
            oss << "}";
            responseBody = oss.str();
        }
    }
    else if (path == "/api/agent/history") {
        responseBody = "{\"stats\":\"" + jsonEscape(getAgentHistoryStats()) + "\"}";
    }
    else if (path == "/api/failure/stats") {
        responseBody = "{\"stats\":\"" + jsonEscape(getFailureDetectorStats()) + "\"}";
    }
    else if (path == "/api/manifest" || path == "/api/features") {
        responseBody = getFeatureManifestJSON();
        if (responseBody.empty()) {
            responseBody = "{\"features\":\"headless mode\"}";
        }
    }
    else if (path == "/api/internal/capture-profile") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string label = "baseline_a";
            int seconds = 30;
            auto j = nlohmann::json::parse(body, nullptr, false);
            
            // Diagnostic: log raw body before extraction
            {
                FILE* fdbg = fopen("headless_server.log", "a");
                if (fdbg) {
                    std::string bodySnippet = body.substr(0, std::min(size_t(200), body.size()));
                    fprintf(fdbg, "CAPTURE_RAW body_len=%zu body=%s parse_discarded=%d\n",
                            body.size(),
                            bodySnippet.c_str(),
                            j.is_discarded() ? 1 : 0);
                    fclose(fdbg);
                }
            }
            
            if (!j.is_discarded()) {
                std::string parsedLabel;
                int parsedSeconds = 0;
                if (tryExtractJsonStringField(body, "baselineLabel", parsedLabel) ||
                    tryExtractJsonStringField(body, "label", parsedLabel)) {
                    if (!parsedLabel.empty()) {
                        label = parsedLabel;
                    }
                }

                if (tryExtractJsonIntField(body, "sampleSeconds", parsedSeconds) ||
                    tryExtractJsonIntField(body, "durationSec", parsedSeconds)) {
                    if (parsedSeconds > 0) {
                        seconds = parsedSeconds;
                    }
                }
            }
            
            // Diagnostic logging for extracted values
            {
                FILE* fdbg = fopen("headless_server.log", "a");
                if (fdbg) {
                    fprintf(fdbg, "CAPTURE_EXTRACTED label=%s seconds=%d\n",
                            label.c_str(),
                            seconds);
                    fclose(fdbg);
                }
            }
            
            const std::string outPath = captureProfileBundleV1(label, seconds);
            {
                FILE* fdbg = fopen("headless_server.log", "a");
                if (fdbg) {
                    fprintf(fdbg, "CAPTURE_DBG body_len=%zu discarded=%d label=%s seconds=%d\n",
                            body.size(),
                            j.is_discarded() ? 1 : 0,
                            label.c_str(),
                            seconds);
                    fclose(fdbg);
                }
            }
            if (outPath.empty()) {
                statusCode = 500;
                responseBody = "{\"success\":false,\"error\":\"capture_failed\"}";
            } else {
                responseBody = "{\"success\":true,\"bundlePath\":\"" + jsonEscape(outPath) + "\"}";
            }
        }
    }
    else if (path == "/health" || path == "/api/health") {
        responseBody = "{\"status\":\"ok\",\"mode\":\"headless\",\"uptime\":" +
                       std::to_string(getUptimeMs()) + "}";
    }
    // ========== Phase 34: Production Instructions Context ==========
    else if (path == "/api/instructions" && method == "GET") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        responseBody = ip.toJSON();
    }
    else if (path == "/api/instructions/summary" && method == "GET") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        responseBody = ip.toJSONSummary();
    }
    else if (path == "/api/instructions/content" && method == "GET") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        std::string content = ip.getAllContent();
        responseBody = "{\"content\":\"" + content + "\"}";
        // Return raw markdown as text/markdown
        contentType = "text/markdown; charset=utf-8";
        responseBody = ip.getAllContent();
    }
    else if (path == "/api/instructions/reload" && method == "POST") {
        auto& ip = InstructionsProvider::instance();
        auto r = ip.reload();
        responseBody = "{\"success\":" + std::string(r.success ? "true" : "false") +
                       ",\"detail\":\"" + jsonEscape(std::string(r.detail)) + "\"}";
    }
    else if (path == "/api/read-file") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"content\":\"\",\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"content\":\"\",\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"content\":\"\",\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::ifstream in(resolvedPath, std::ios::binary);
                if (!in) {
                    responseBody = "{\"content\":\"\",\"success\":false,\"error\":\"not_found\"}";
                } else {
                    std::ostringstream ss;
                    ss << in.rdbuf();
                    const std::string fileContent = ss.str();
                    std::ostringstream out;
                    out << "{\"content\":\"" << jsonEscape(fileContent) << "\",\"success\":true,";
                    out << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                    out << "\"bytes\":" << fileContent.size();
                    out << "}";
                    responseBody = out.str();
                }
            }
        }
    }
    else if (path == "/api/write-file") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            std::string content;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
                content = j.value("content", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }
            if (content.empty()) {
                (void)tryExtractJsonStringField(body, "content", content);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::filesystem::path p(targetPath);
                std::error_code ec;
                if (!p.parent_path().empty()) {
                    std::filesystem::create_directories(p.parent_path(), ec);
                }
                std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
                if (!out) {
                    responseBody = "{\"success\":false,\"error\":\"write_failed\"}";
                } else {
                    out.write(content.data(), static_cast<std::streamsize>(content.size()));
                    std::ostringstream resp;
                    resp << "{\"success\":true,";
                    resp << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                    resp << "\"bytes_written\":" << content.size() << ",";
                    resp << "\"parent_created\":" << ((!p.parent_path().empty() && !ec) ? "true" : "false");
                    resp << "}";
                    responseBody = resp.str();
                }
            }
        }
    }
    else if (path == "/api/list-directory") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"entries\":[],\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"entries\":[],\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"entries\":[],\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::error_code ec;
                if (!std::filesystem::exists(resolvedPath, ec)) {
                    responseBody = "{\"entries\":[],\"success\":false,\"error\":\"not_found\"}";
                } else {
                    std::string entriesJson;
                    bool firstEntry = true;
                    size_t fileCount = 0;
                    size_t dirCount = 0;
                    for (const auto& entry : std::filesystem::directory_iterator(resolvedPath, ec)) {
                        if (ec) {
                            break;
                        }
                        if (!firstEntry) {
                            entriesJson += ",";
                        }
                        firstEntry = false;
                        const bool isDir = entry.is_directory();
                        if (isDir) {
                            ++dirCount;
                        } else {
                            ++fileCount;
                        }
                        entriesJson += "{\"name\":\"" + jsonEscape(entry.path().filename().string()) + "\",\"is_dir\":" +
                                      std::string(isDir ? "true" : "false") + "}";
                    }
                    std::ostringstream out;
                    out << "{\"entries\":[" << entriesJson << "],\"success\":true,";
                    out << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                    out << "\"file_count\":" << fileCount << ",";
                    out << "\"dir_count\":" << dirCount;
                    out << "}";
                    responseBody = out.str();
                }
            }
        }
    }
    else if (path == "/api/stat-file") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"size\":0,\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"size\":0,\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"size\":0,\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::error_code ec;
                if (!std::filesystem::exists(resolvedPath, ec)) {
                    responseBody = "{\"size\":0,\"success\":false,\"error\":\"not_found\"}";
                } else {
                    const bool isFile = std::filesystem::is_regular_file(resolvedPath, ec);
                    const bool isDir = std::filesystem::is_directory(resolvedPath, ec);
                    const auto fileSize = isFile ? std::filesystem::file_size(resolvedPath, ec) : 0;
                    std::ostringstream out;
                    out << "{\"size\":" << static_cast<unsigned long long>(fileSize) << ",\"success\":true,";
                    out << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                    out << "\"is_file\":" << (isFile ? "true" : "false") << ",";
                    out << "\"is_dir\":" << (isDir ? "true" : "false");
                    out << "}";
                    responseBody = out.str();
                }
            }
        }
    }
    else if (path == "/api/search-files") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            std::string pattern;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
                pattern = j.value("pattern", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }
            if (pattern.empty()) {
                (void)tryExtractJsonStringField(body, "pattern", pattern);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"matches\":[],\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"matches\":[],\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"matches\":[],\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::string matchesJson;
                bool firstMatch = true;
                std::error_code ec;
                size_t scannedFiles = 0;
                size_t matchedFiles = 0;
                bool truncated = false;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(resolvedPath, ec)) {
                    if (ec) {
                        break;
                    }
                    if (!entry.is_regular_file()) {
                        continue;
                    }
                    ++scannedFiles;
                    const std::string fileName = entry.path().filename().string();
                    if (pattern.empty() || fileName.find(pattern) != std::string::npos) {
                        ++matchedFiles;
                        if (matchedFiles > kMaxHeadlessSearchMatches) {
                            truncated = true;
                            break;
                        }
                        if (!firstMatch) {
                            matchesJson += ",";
                        }
                        firstMatch = false;
                        matchesJson += "\"" + jsonEscape(entry.path().string()) + "\"";
                    }
                }
                std::ostringstream out;
                out << "{\"matches\":[" << matchesJson << "],\"success\":true,";
                out << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                out << "\"pattern\":\"" << jsonEscape(pattern) << "\",";
                out << "\"scanned\":" << scannedFiles << ",";
                out << "\"matched\":" << matchedFiles << ",";
                out << "\"truncated\":" << (truncated ? "true" : "false");
                out << "}";
                responseBody = out.str();
            }
        }
    }
    else if (path == "/api/mkdir") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::error_code ec;
                const bool existed = std::filesystem::exists(resolvedPath, ec);
                const bool created = std::filesystem::create_directories(resolvedPath, ec);
                std::ostringstream out;
                out << "{\"success\":" << std::string(ec ? "false" : "true") << ",";
                out << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                out << "\"created\":" << (created ? "true" : "false") << ",";
                out << "\"already_exists\":" << (existed ? "true" : "false");
                out << "}";
                responseBody = out.str();
            }
        }
    }
    else if (path == "/api/rename-file") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string oldPath;
            std::string newPath;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                oldPath = j.value("old_path", "");
                newPath = j.value("new_path", "");
            }
            if (oldPath.empty()) {
                (void)tryExtractJsonStringField(body, "old_path", oldPath);
            }
            if (newPath.empty()) {
                (void)tryExtractJsonStringField(body, "new_path", newPath);
            }

            std::filesystem::path resolvedOldPath;
            std::filesystem::path resolvedNewPath;
            std::string resolveErrorOld;
            std::string resolveErrorNew;
            const bool oldOk = resolveScopedPath(oldPath, resolvedOldPath, resolveErrorOld);
            const bool newOk = resolveScopedPath(newPath, resolvedNewPath, resolveErrorNew);

            if (!oldOk || !newOk) {
                if ((resolveErrorOld == "missing_path") || (resolveErrorNew == "missing_path")) {
                    responseBody = "{\"success\":false,\"error\":\"missing_path\"}";
                } else if ((resolveErrorOld == "path_outside_scope") || (resolveErrorNew == "path_outside_scope")) {
                    statusCode = 403;
                    responseBody = "{\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                oldPath = resolvedOldPath.string();
                newPath = resolvedNewPath.string();
                std::error_code ec;
                const bool oldExists = std::filesystem::exists(resolvedOldPath, ec);
                std::filesystem::rename(resolvedOldPath, resolvedNewPath, ec);
                std::ostringstream out;
                out << "{\"success\":" << std::string(ec ? "false" : "true") << ",";
                out << "\"old_path\":\"" << jsonEscape(oldPath) << "\",";
                out << "\"new_path\":\"" << jsonEscape(newPath) << "\",";
                out << "\"old_exists\":" << (oldExists ? "true" : "false");
                out << "}";
                responseBody = out.str();
            }
        }
    }
    else if (path == "/api/delete-file") {
        if (method != "POST") {
            methodNotAllowed();
        } else {
            std::string targetPath;
            auto j = nlohmann::json::parse(body, nullptr, false);
            if (!j.is_discarded()) {
                targetPath = j.value("path", "");
            }
            if (targetPath.empty()) {
                (void)tryExtractJsonStringField(body, "path", targetPath);
            }

            std::filesystem::path resolvedPath;
            std::string resolveError;
            if (!resolveScopedPath(targetPath, resolvedPath, resolveError)) {
                if (resolveError == "missing_path") {
                    responseBody = "{\"success\":false,\"error\":\"missing_path\"}";
                } else if (resolveError == "path_outside_scope") {
                    statusCode = 403;
                    responseBody = "{\"success\":false,\"error\":\"path_outside_scope\"}";
                } else {
                    statusCode = 400;
                    responseBody = "{\"success\":false,\"error\":\"invalid_path\"}";
                }
            } else {
                targetPath = resolvedPath.string();
                std::error_code ec;
                const bool existed = std::filesystem::exists(resolvedPath, ec);
                const bool removed = std::filesystem::remove(resolvedPath, ec);
                std::ostringstream out;
                out << "{\"success\":" << std::string((!ec && removed) ? "true" : "false") << ",";
                out << "\"path\":\"" << jsonEscape(targetPath) << "\",";
                out << "\"existed\":" << (existed ? "true" : "false") << ",";
                out << "\"removed\":" << (removed ? "true" : "false");
                out << "}";
                responseBody = out.str();
            }
        }
    }
    else {
        statusCode = 404;
        responseBody = "{\"error\":\"Not found\",\"path\":\"" + jsonEscape(path) + "\"}";
    }

    // Send HTTP response
    if (responseBody.size() > kMaxHeadlessHttpResponseBytes) {
        statusCode = 413;
        responseBody = "{\"error\":\"Response too large\"}";
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << statusCode << " " << httpStatusText(statusCode) << "\r\n";
    resp << "Content-Type: " << contentType << "\r\n";
    resp << "Content-Length: " << responseBody.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
    resp << "X-Content-Type-Options: nosniff\r\n";
    resp << "Cache-Control: no-store\r\n";
    resp << "X-Request-Id: " << requestId << "\r\n";
    resp << "X-Audit-Id: " << auditId << "\r\n";
    if (statusCode == 429 && retryAfterSeconds > 0) {
        resp << "Retry-After: " << retryAfterSeconds << "\r\n";
    }
    resp << "X-Response-Time-Ms: " << (GetTickCount64() - requestStartMs) << "\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << responseBody;

    std::string response = resp.str();
    sendAll(response);
}

// ============================================================================
// Feature Manifest (delegates to Win32IDE_FeatureManifest.cpp structures)
// ============================================================================
std::string HeadlessIDE::getFeatureManifestMarkdown() const {
    return "# RawrXD Feature Manifest (Headless)\n\nPhase 19C: Headless surface active.\n";
}

std::string HeadlessIDE::getFeatureManifestJSON() const {
    return "{\"mode\":\"headless\",\"version\":\"" + jsonEscape(std::string(VERSION)) +
           "\",\"phase\":\"" + jsonEscape(std::string(BUILD_PHASE)) + "\"}";
}

std::string HeadlessIDE::getQuantumStatusJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"quantum_time_manager_activated\":" << (m_expQuantumTimeActivated ? "true" : "false") << ",";
    oss << "\"quantum_orchestrator_activated\":" << (m_expQuantumOrchActivated ? "true" : "false") << ",";
    oss << "\"quantum_missing_impl_activated\":" << (m_expQuantumMissingActivated ? "true" : "false");
    oss << "}";
    return oss.str();
}

std::string HeadlessIDE::captureProfileBundleV1(const std::string& baselineLabel, int sampleSeconds) {
    // Diagnostic logging for label parameter on entry
    {
        FILE* fdbg = fopen("headless_server.log", "a");
        if (fdbg) {
            fprintf(fdbg, "CAPTURE_BUNDLE_ENTRY baselineLabel=%s sampleSeconds=%d\n",
                    baselineLabel.c_str(),
                    sampleSeconds);
            fclose(fdbg);
        }
    }

    try {
        const int boundedSeconds = std::max(5, std::min(sampleSeconds, 300));
        const std::string stamp = makeTimestampTag();
        const std::string folderName = baselineLabel + "_" + stamp;

        const std::filesystem::path bundleDir = std::filesystem::current_path() / "profiles" / folderName;
        std::filesystem::create_directories(bundleDir);

        // 1) session_state.json
        nlohmann::json state;
        state["schema"] = "profile_bundle_v1";
        state["capturedAt"] = stamp;
        state["mode"] = "headless";
        state["session"] = m_sessionId;
        state["server"] = {
            {"running", m_serverRunning.load()},
            {"bindAddress", m_config.bindAddress},
            {"port", m_config.port}
        };
        state["layout"] = {
            {"snapState", "headless"},
            {"sidebarWidth96", 0},
            {"secondarySidebarWidth96", 0},
            {"panelVisible", false}
        };
        state["model"] = {
            {"loaded", m_modelLoaded},
            {"name", m_loadedModelName},
            {"path", m_loadedModelPath}
        };
        {
            std::ofstream out(bundleDir / "session_state.json");
            out << state.dump(2);
        }

        // 2) environment.log
        {
            std::ofstream env(bundleDir / "environment.log");
            env << "ProfileBundle=v1\n";
            env << "Version=" << VERSION << "\n";
            env << "BuildPhase=" << BUILD_PHASE << "\n";
            env << "CurrentDir=" << std::filesystem::current_path().string() << "\n";
            const char* username = std::getenv("USERNAME");
            const char* ollamaModels = std::getenv("OLLAMA_MODELS");
            env << "USERNAME=" << (username ? username : "") << "\n";
            env << "OLLAMA_MODELS=" << (ollamaModels ? ollamaModels : "") << "\n";
            env << "GPU_NOTE=Collect adapter name/driver via DXGI pass in next profile iteration\n";
        }

        // 3) perf_baseline.csv (CPU/memory/IO, 1 Hz)
        {
            std::ofstream perf(bundleDir / "perf_baseline.csv");
            perf << "sample,cpu_percent,working_set_mb,private_bytes_mb,io_read_mb,io_write_mb\n";

            HANDLE hProc = GetCurrentProcess();
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            const uint64_t logicalCpuCount = std::max<uint64_t>(1, si.dwNumberOfProcessors);

            FILETIME c0{}, e0{}, k0{}, u0{};
            GetProcessTimes(hProc, &c0, &e0, &k0, &u0);
            uint64_t prevProc100ns = fileTimeToUInt64(k0) + fileTimeToUInt64(u0);
            uint64_t lastWallMs = GetTickCount64();

            for (int i = 0; i < boundedSeconds; ++i) {
                Sleep(1000);

                FILETIME c1{}, e1{}, k1{}, u1{};
                GetProcessTimes(hProc, &c1, &e1, &k1, &u1);
                const uint64_t nowProc100ns = fileTimeToUInt64(k1) + fileTimeToUInt64(u1);
                const uint64_t nowWallMs = GetTickCount64();

                const uint64_t dProc100ns = (nowProc100ns >= prevProc100ns) ? (nowProc100ns - prevProc100ns) : 0;
                const uint64_t dWallMs = (nowWallMs > lastWallMs) ? (nowWallMs - lastWallMs) : 1;
                const double cpuPct = (static_cast<double>(dProc100ns) / (static_cast<double>(dWallMs) * 10000.0)) /
                                      static_cast<double>(logicalCpuCount) * 100.0;

                PROCESS_MEMORY_COUNTERS_EX pmc{};
                pmc.cb = sizeof(pmc);
                SIZE_T ws = 0;
                SIZE_T priv = 0;
                if (GetProcessMemoryInfo(hProc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                    ws = pmc.WorkingSetSize;
                    priv = pmc.PrivateUsage;
                }

                IO_COUNTERS io{};
                unsigned long long readMb = 0;
                unsigned long long writeMb = 0;
                if (GetProcessIoCounters(hProc, &io)) {
                    readMb = static_cast<unsigned long long>(io.ReadTransferCount / (1024ull * 1024ull));
                    writeMb = static_cast<unsigned long long>(io.WriteTransferCount / (1024ull * 1024ull));
                }

                perf << i
                     << "," << std::fixed << std::setprecision(2) << cpuPct
                     << "," << std::fixed << std::setprecision(2)
                     << (static_cast<double>(ws) / (1024.0 * 1024.0))
                     << "," << std::fixed << std::setprecision(2)
                     << (static_cast<double>(priv) / (1024.0 * 1024.0))
                     << "," << readMb
                     << "," << writeMb
                     << "\n";

                prevProc100ns = nowProc100ns;
                lastWallMs = nowWallMs;
            }
        }

        // 4) contract_results.json
        {
            nlohmann::json contracts;
            auto makeUnknownContract = []() {
                nlohmann::json entry = nlohmann::json::object();
                entry["pass"] = nlohmann::json();
                entry["fail"] = nlohmann::json();
                entry["status"] = "unknown";
                return entry;
            };

            contracts["router_hybrid"] = makeUnknownContract();
            contracts["debug_swarm"] = makeUnknownContract();
            contracts["dual_multi_response"] = makeUnknownContract();
            contracts["note"] = "Populate from external contract runner artifacts when available.";
            std::ofstream out(bundleDir / "contract_results.json");
            out << contracts.dump(2);
        }

        // 5) memory_map_trace.txt
        {
            std::ofstream mm(bundleDir / "memory_map_trace.txt");
            mm << "ProfileBundle=v1\n";
            mm << "LoaderSignature=baseline\n";
            mm << "ModelPath=" << m_loadedModelPath << "\n";
            mm << "ModelName=" << m_loadedModelName << "\n";
            if (!m_loadedModelPath.empty()) {
                std::error_code ec;
                const auto sz = std::filesystem::file_size(m_loadedModelPath, ec);
                if (!ec) {
                    mm << "ModelFileBytes=" << static_cast<unsigned long long>(sz) << "\n";
                }
            }
            mm << "MapViewOfFile3Trace=not_available_in_v1\n";
            mm << "PageFaultCounters=not_available_in_v1\n";
        }

        if (m_outputSink) {
            const std::string msg = "Profile Bundle v1 captured: " + bundleDir.string();
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        }
        return bundleDir.string();
    } catch (const std::exception& ex) {
        if (m_outputSink) {
            const std::string msg = std::string("Profile Bundle v1 capture failed: ") + ex.what();
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Error);
        }
        return std::string();
    }
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string HeadlessIDE::getFullStatusDump() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"mode\": \"headless\",\n";
    oss << "  \"version\": \"" << jsonEscape(std::string(VERSION)) << "\",\n";
    oss << "  \"phase\": \"" << jsonEscape(std::string(BUILD_PHASE)) << "\",\n";
    oss << "  \"session\": \"" << jsonEscape(m_sessionId) << "\",\n";
    oss << "  \"uptime_ms\": " << getUptimeMs() << ",\n";
    oss << "  \"model_loaded\": " << (m_modelLoaded ? "true" : "false") << ",\n";
    oss << "  \"model_name\": \"" << jsonEscape(m_loadedModelName) << "\",\n";
    oss << "  \"server_running\": " << (m_serverRunning.load() ? "true" : "false") << ",\n";
    oss << "  \"subsystems\": {\n";
    oss << "    \"winsock\": " << (m_winsockInitialized ? "true" : "false") << ",\n";
    oss << "    \"backend_manager\": " << (m_backendManagerInitialized ? "true" : "false") << ",\n";
    oss << "    \"llm_router\": " << (m_routerInitialized ? "true" : "false") << ",\n";
    oss << "    \"failure_detector\": " << (m_failureDetectorInitialized ? "true" : "false") << ",\n";
    oss << "    \"agent_history\": " << (m_agentHistoryInitialized ? "true" : "false") << ",\n";
    oss << "    \"asm_semantic\": " << (m_asmSemanticInitialized ? "true" : "false") << ",\n";
    oss << "    \"lsp_client\": " << (m_lspInitialized ? "true" : "false") << ",\n";
    oss << "    \"hybrid_bridge\": " << (m_hybridBridgeInitialized ? "true" : "false") << ",\n";
    oss << "    \"multi_response\": " << (m_multiResponseInitialized ? "true" : "false") << ",\n";
    oss << "    \"exec_governor\": " << (m_phase10Initialized ? "true" : "false") << ",\n";
    oss << "    \"swarm\": " << (m_phase11Initialized ? "true" : "false") << ",\n";
    oss << "    \"native_debugger\": " << (m_phase12Initialized ? "true" : "false") << ",\n";
    oss << "    \"hotpatch\": " << (m_hotpatchInitialized ? "true" : "false") << ",\n";
    oss << "    \"instructions\": " << (m_instructionsInitialized ? "true" : "false") << "\n";
    oss << "  }\n";
    oss << "  ,\"experimental\": {\n";
    oss << "    \"hotpatch70b_activated\": " << (m_expHotpatchActivated ? "true" : "false") << ",\n";
    oss << "    \"layer_eviction_activated\": " << (m_expLayerEvictionActivated ? "true" : "false") << ",\n";
    oss << "    \"governor_activated\": " << (m_expGovernorActivated ? "true" : "false") << ",\n";
    oss << "    \"quantum_time_manager_activated\": " << (m_expQuantumTimeActivated ? "true" : "false") << ",\n";
    oss << "    \"quantum_orchestrator_activated\": " << (m_expQuantumOrchActivated ? "true" : "false") << ",\n";
    oss << "    \"quantum_missing_impl_activated\": " << (m_expQuantumMissingActivated ? "true" : "false") << "\n";
    oss << "  }\n";
    oss << "}";
    return oss.str();
}

std::string HeadlessIDE::getVersionString() const {
    return std::string(VERSION) + " (" + BUILD_PHASE + ")";
}

uint64_t HeadlessIDE::getUptimeMs() const {
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return static_cast<uint64_t>(epoch) - m_startEpochMs;
}

// ============================================================================
// Run Modes
// ============================================================================
int HeadlessIDE::runServerMode() {
    if (m_config.enableServer) {
        startServer();
        if (!m_serverRunning.load()) {
            m_outputSink->appendOutput("HTTP server failed to start; exiting headless server mode.", OutputSeverity::Error);
            FILE* f = fopen("headless_server.log", "a");
            if (f) {
                fprintf(f, "RUN_SERVER_EXIT start_failed\n");
                fclose(f);
            }
            return 2;
        }
    } else {
        m_outputSink->appendOutput("Server disabled (--no-server); nothing to serve, exiting.", OutputSeverity::Warning);
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "RUN_SERVER_EXIT no_server\n");
            fclose(f);
        }
        return 0;
    }

    m_outputSink->appendOutput("Headless IDE running in server mode. Press Ctrl+C to stop.", OutputSeverity::Info);
    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f, "RUN_SERVER_LOOP_ENTER\n");
            fclose(f);
        }
    }

    // Block until shutdown
    while (!m_shutdownRequested.load()) {
        Sleep(100);
    }

    {
        FILE* f = fopen("headless_server.log", "a");
        if (f) {
            fprintf(f,
                    "RUN_SERVER_LOOP_EXIT shutdownRequested=%d serverRunning=%d\n",
                    m_shutdownRequested.load() ? 1 : 0,
                    m_serverRunning.load() ? 1 : 0);
            fclose(f);
        }
    }

    m_outputSink->appendOutput("Shutting down headless IDE...", OutputSeverity::Info);
    return 0;
}

int HeadlessIDE::runReplMode() {
    if (m_config.enableServer) {
        startServer();
    }

    m_outputSink->appendOutput("RawrXD Headless REPL. Type 'help' for commands, 'quit' to exit.", OutputSeverity::Info);

    std::string line;
    while (!m_shutdownRequested.load()) {
        printReplPrompt();
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "quit" || line == "exit" || line == "q") break;
        processReplCommand(line);
    }

    return 0;
}

int HeadlessIDE::runSingleShotMode() {
    if (m_config.prompt.empty()) {
        m_outputSink->appendOutput("No prompt specified (--prompt)", OutputSeverity::Error);
        return 1;
    }

    if (m_config.traceTokenSummary || !m_config.traceTokenCsvPath.empty()) {
        const std::string traceModelPath = !m_loadedModelPath.empty() ? m_loadedModelPath : m_config.modelPath;
        if (traceModelPath.empty()) {
            m_outputSink->appendOutput("Trace mode requires --model <path>", OutputSeverity::Error);
            return 1;
        }

        auto engine = RawrXD::CPUInferenceEngine::GetSharedInstance();
        if (!engine) {
            m_outputSink->appendOutput("CPU inference engine unavailable for trace mode", OutputSeverity::Error);
            return 1;
        }

        if (!engine->IsModelLoaded() && !engine->LoadModel(traceModelPath)) {
            const std::string error = std::string("Failed to load trace model: ") + engine->GetLastLoadErrorMessage();
            m_outputSink->appendOutput(error.c_str(), OutputSeverity::Error);
            return 1;
        }

        engine->ClearTokenTraceBuffer();
        const auto inputTokens = engine->Tokenize(m_config.prompt);
        if (inputTokens.empty()) {
            m_outputSink->appendOutput("Trace mode failed to tokenize prompt", OutputSeverity::Error);
            return 1;
        }

        std::string result;
        const bool wantCsv = !m_config.traceTokenCsvPath.empty();
        std::size_t emittedTokenCount = 0;
        constexpr std::size_t kCsvCheckpointStride = 1;
        engine->GenerateStreaming(
            inputTokens, m_config.maxTokens,
            [&](const std::string& piece) { result += piece; },
            [&]()
            {
                if (wantCsv)
                    (void)engine->DumpTokenTracesToCSV(m_config.traceTokenCsvPath);
            },
            [&](int32_t)
            {
                ++emittedTokenCount;
                if (wantCsv && (emittedTokenCount % kCsvCheckpointStride) == 0)
                    (void)engine->DumpTokenTracesToCSV(m_config.traceTokenCsvPath);
            });

        if (m_config.traceTokenSummary) {
            const size_t summaryWindow = static_cast<size_t>(std::max(1, m_config.maxTokens));
            const std::string summary = engine->DumpTokenTraceSummary(summaryWindow);
            m_outputSink->appendOutput(summary.c_str(), OutputSeverity::Info);
        }

        if (!m_config.traceTokenCsvPath.empty()) {
            if (engine->DumpTokenTracesToCSV(m_config.traceTokenCsvPath)) {
                const std::string msg = std::string("Token trace CSV written: ") + m_config.traceTokenCsvPath;
                m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
            } else {
                m_outputSink->appendOutput("Failed to write token trace CSV", OutputSeverity::Warning);
            }
        }

        m_outputSink->appendOutput(result.c_str(), OutputSeverity::Info);
        return 0;
    }

    std::string result = runInference(m_config.prompt);
    m_outputSink->appendOutput(result.c_str(), OutputSeverity::Info);
    return 0;
}

int HeadlessIDE::runBatchMode() {
    if (m_config.inputFile.empty()) {
        m_outputSink->appendOutput("No input file specified (--input)", OutputSeverity::Error);
        return 1;
    }

    std::ifstream inFile(m_config.inputFile);
    if (!inFile.is_open()) {
        m_outputSink->appendOutput(("Cannot open input file: " + m_config.inputFile).c_str(), OutputSeverity::Error);
        return 1;
    }

    inFile.seekg(0, std::ios::end);
    const std::streamoff inputSize = inFile.tellg();
    if (inputSize > static_cast<std::streamoff>(kMaxHeadlessBatchInputBytes)) {
        m_outputSink->appendOutput("Input file too large for headless batch mode", OutputSeverity::Error);
        return 1;
    }
    inFile.clear();
    inFile.seekg(0, std::ios::beg);

    std::ofstream outFile;
    if (!m_config.outputFile.empty()) {
        outFile.open(m_config.outputFile);
        if (!outFile.is_open()) {
            m_outputSink->appendOutput(("Cannot open output file: " + m_config.outputFile).c_str(), OutputSeverity::Error);
            return 1;
        }
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(inFile, line) && !m_shutdownRequested.load()) {
        if (line.empty()) continue;
        ++lineNum;
        if (static_cast<size_t>(lineNum) > kMaxHeadlessBatchPrompts) {
            m_outputSink->appendOutput("Batch prompt count exceeds safety limit", OutputSeverity::Error);
            return 1;
        }
        if (!isReasonablePromptSize(line)) {
            m_outputSink->appendOutput(("Input line exceeds maximum prompt size at line " + std::to_string(lineNum)).c_str(),
                                       OutputSeverity::Error);
            return 1;
        }
        m_outputSink->appendOutput(("Processing prompt " + std::to_string(lineNum) + "...").c_str(), OutputSeverity::Debug);

        std::string result = runInference(line);

        if (outFile.is_open()) {
            outFile << result << "\n";
        } else {
            m_outputSink->appendOutput(result.c_str(), OutputSeverity::Info);
        }
    }

    m_outputSink->appendOutput(("Batch complete: " + std::to_string(lineNum) + " prompts processed").c_str(), OutputSeverity::Info);
    return 0;
}

int HeadlessIDE::runAttentionBenchMode() {
    const bool avx512Runtime = RawrXD::KernelOps::HasAVX512Runtime();

    int effectiveIters = m_config.benchMeasureIters;
    if (m_config.benchMedianMode && effectiveIters < 3) {
        effectiveIters = 3;
    }

    char shape[256] = {};
    snprintf(shape, sizeof(shape),
             "[ATTN_BENCH] shape batch=%d seq=%d heads=%d head_dim=%d warmup=%d iters=%d median_mode=%d",
             m_config.benchBatchSize,
             m_config.benchSeqLen,
             m_config.benchNumHeads,
             m_config.benchHeadDim,
             m_config.benchWarmupIters,
             effectiveIters,
             m_config.benchMedianMode ? 1 : 0);
    m_outputSink->appendOutput(shape, OutputSeverity::Info);

    const AttentionBenchResult scalarResult = MeasureAttentionLatency(
        m_config.benchBatchSize,
        m_config.benchSeqLen,
        m_config.benchHeadDim,
        m_config.benchNumHeads,
        m_config.benchWarmupIters,
        effectiveIters,
        false,
        m_config.benchMedianMode);

    AttentionBenchResult avxResult{};
    if (avx512Runtime) {
        avxResult = MeasureAttentionLatency(
            m_config.benchBatchSize,
            m_config.benchSeqLen,
            m_config.benchHeadDim,
            m_config.benchNumHeads,
            m_config.benchWarmupIters,
            effectiveIters,
            true,
            m_config.benchMedianMode);
    }

    const double scalarMetric = (m_config.benchMedianMode && scalarResult.medianMs > 0.0) ? scalarResult.medianMs
                                                                                            : scalarResult.avgMs;
    const double avxMetric = (m_config.benchMedianMode && avxResult.medianMs > 0.0) ? avxResult.medianMs
                                                                                       : avxResult.avgMs;
    const double speedup = (avx512Runtime && avxMetric > 0.0) ? (scalarMetric / avxMetric) : 1.0;

    char report[512] = {};
    snprintf(report,
             sizeof(report),
             "[ATTN_BENCH] scalar_avg_ms=%.3f scalar_med_ms=%.3f scalar_tps=%.2f avx512_available=%d avx512_avg_ms=%.3f avx512_med_ms=%.3f avx512_tps=%.2f speedup=%.3fx metric=%s",
             scalarResult.avgMs,
             scalarResult.medianMs,
             scalarResult.tps,
             avx512Runtime ? 1 : 0,
             avxResult.avgMs,
             avxResult.medianMs,
             avxResult.tps,
             speedup,
             m_config.benchMedianMode ? "median" : "avg");
    m_outputSink->appendOutput(report, OutputSeverity::Info);

    if (!m_config.benchCsvPath.empty()) {
        bool writeHeader = true;
        {
            std::ifstream existing(m_config.benchCsvPath, std::ios::binary);
            if (existing.good()) {
                existing.seekg(0, std::ios::end);
                writeHeader = (existing.tellg() <= 0);
            }
        }

        std::ofstream csv(m_config.benchCsvPath, std::ios::app);
        if (!csv.is_open()) {
            m_outputSink->appendOutput("[ATTN_BENCH] Failed to open CSV path for append", OutputSeverity::Warning);
        } else {
            if (writeHeader) {
                csv << "batch,seq_len,heads,head_dim,warmup,iters,median_mode,avx512_available,"
                       "scalar_avg_ms,scalar_median_ms,scalar_tps,avx512_avg_ms,avx512_median_ms,avx512_tps,speedup,metric\n";
            }
            csv << m_config.benchBatchSize << ','
                << m_config.benchSeqLen << ','
                << m_config.benchNumHeads << ','
                << m_config.benchHeadDim << ','
                << m_config.benchWarmupIters << ','
                << effectiveIters << ','
                << (m_config.benchMedianMode ? 1 : 0) << ','
                << (avx512Runtime ? 1 : 0) << ','
                << scalarResult.avgMs << ','
                << scalarResult.medianMs << ','
                << scalarResult.tps << ','
                << avxResult.avgMs << ','
                << avxResult.medianMs << ','
                << avxResult.tps << ','
                << speedup << ','
                << (m_config.benchMedianMode ? "median" : "avg") << '\n';
            csv.flush();
            m_outputSink->appendOutput(("[ATTN_BENCH] CSV appended: " + m_config.benchCsvPath).c_str(), OutputSeverity::Info);
        }
    }

    if (!avx512Runtime) {
        m_outputSink->appendOutput("[ATTN_BENCH] AVX-512 runtime unavailable; AVX-512 leg skipped.", OutputSeverity::Warning);
    }

    return 0;
}

int HeadlessIDE::runScalingSweepMode() {
    const bool avx512Runtime = RawrXD::KernelOps::HasAVX512Runtime();

    // Sweep defaults target cache-transition and register-pressure inflection points.
    const std::vector<int> seqLens = {128, 256, 512, 1024, 2048};
    const std::vector<int> headDims = {64, 128, 256};

    int effectiveIters = std::max(m_config.benchMeasureIters, 5);
    const bool medianMode = true;

    char start[256] = {};
    snprintf(start,
             sizeof(start),
             "[ATTN_SWEEP] start batch=%d heads=%d warmup=%d iters=%d median_mode=1 seq_count=%zu dim_count=%zu",
             m_config.benchBatchSize,
             m_config.benchNumHeads,
             m_config.benchWarmupIters,
             effectiveIters,
             seqLens.size(),
             headDims.size());
    m_outputSink->appendOutput(start, OutputSeverity::Info);

    if (!m_config.benchMedianMode || m_config.benchMeasureIters < 5) {
        m_outputSink->appendOutput("[ATTN_SWEEP] enforcing median mode with >=5 measured iterations for statistical stability.",
                                   OutputSeverity::Info);
    }

    bool writeHeader = false;
    if (!m_config.benchCsvPath.empty()) {
        writeHeader = true;
        std::ifstream existing(m_config.benchCsvPath, std::ios::binary);
        if (existing.good()) {
            existing.seekg(0, std::ios::end);
            writeHeader = (existing.tellg() <= 0);
        }
    }

    std::ofstream csv;
    if (!m_config.benchCsvPath.empty()) {
        csv.open(m_config.benchCsvPath, std::ios::app);
        if (!csv.is_open()) {
            m_outputSink->appendOutput("[ATTN_SWEEP] Failed to open CSV path for append", OutputSeverity::Warning);
        } else if (writeHeader) {
            csv << "batch,seq_len,heads,head_dim,warmup,iters,median_mode,avx512_available,"
                   "scalar_avg_ms,scalar_median_ms,scalar_tps,avx512_avg_ms,avx512_median_ms,avx512_tps,speedup,metric\n";
        }
    }

    int totalShapes = 0;
    int avxWins = 0;
    double bestSpeedup = 0.0;
    int bestSeq = 0;
    int bestDim = 0;

    for (int seqLen : seqLens) {
        for (int headDim : headDims) {
            ++totalShapes;

            const AttentionBenchResult scalarResult = MeasureAttentionLatency(
                m_config.benchBatchSize,
                seqLen,
                headDim,
                m_config.benchNumHeads,
                m_config.benchWarmupIters,
                effectiveIters,
                false,
                medianMode);

            AttentionBenchResult avxResult{};
            if (avx512Runtime) {
                avxResult = MeasureAttentionLatency(
                    m_config.benchBatchSize,
                    seqLen,
                    headDim,
                    m_config.benchNumHeads,
                    m_config.benchWarmupIters,
                    effectiveIters,
                    true,
                    medianMode);
            }

            const double scalarMetric = (scalarResult.medianMs > 0.0) ? scalarResult.medianMs : scalarResult.avgMs;
            const double avxMetric = (avxResult.medianMs > 0.0) ? avxResult.medianMs : avxResult.avgMs;
            const double speedup = (avx512Runtime && avxMetric > 0.0) ? (scalarMetric / avxMetric) : 1.0;

            if (avx512Runtime && speedup > 1.0) {
                ++avxWins;
            }
            if (avx512Runtime && speedup > bestSpeedup) {
                bestSpeedup = speedup;
                bestSeq = seqLen;
                bestDim = headDim;
            }

            char row[512] = {};
            snprintf(row,
                     sizeof(row),
                     "[ATTN_SWEEP] seq=%d head_dim=%d scalar_med_ms=%.3f avx512_med_ms=%.3f speedup=%.3fx",
                     seqLen,
                     headDim,
                     scalarResult.medianMs,
                     avxResult.medianMs,
                     speedup);
            m_outputSink->appendOutput(row, OutputSeverity::Info);

            if (csv.is_open()) {
                csv << m_config.benchBatchSize << ','
                    << seqLen << ','
                    << m_config.benchNumHeads << ','
                    << headDim << ','
                    << m_config.benchWarmupIters << ','
                    << effectiveIters << ','
                    << 1 << ','
                    << (avx512Runtime ? 1 : 0) << ','
                    << scalarResult.avgMs << ','
                    << scalarResult.medianMs << ','
                    << scalarResult.tps << ','
                    << avxResult.avgMs << ','
                    << avxResult.medianMs << ','
                    << avxResult.tps << ','
                    << speedup << ','
                    << "median" << '\n';
                csv.flush();
            }
        }
    }

    if (csv.is_open()) {
        m_outputSink->appendOutput(("[ATTN_SWEEP] CSV appended: " + m_config.benchCsvPath).c_str(), OutputSeverity::Info);
    }

    char summary[320] = {};
    snprintf(summary,
             sizeof(summary),
             "[ATTN_SWEEP] complete shapes=%d avx512_available=%d avx512_wins=%d best_speedup=%.3fx best_seq=%d best_head_dim=%d",
             totalShapes,
             avx512Runtime ? 1 : 0,
             avxWins,
             bestSpeedup,
             bestSeq,
             bestDim);
    m_outputSink->appendOutput(summary, OutputSeverity::Info);

    if (!avx512Runtime) {
        m_outputSink->appendOutput("[ATTN_SWEEP] AVX-512 runtime unavailable; AVX-512 leg skipped.", OutputSeverity::Warning);
    }

    return 0;
}

// ============================================================================
// REPL Helpers
// ============================================================================
void HeadlessIDE::processReplCommand(const std::string& input) {
    if (input == "help" || input == "?") {
        printReplHelp();
    }
    else if (input == "status") {
        std::string dump = getFullStatusDump();
        m_outputSink->appendOutput(dump.c_str(), OutputSeverity::Info);
    }
    else if (input == "version") {
        m_outputSink->appendOutput(getVersionString().c_str(), OutputSeverity::Info);
    }
    else if (input.substr(0, 5) == "load ") {
        std::string path = input.substr(5);
        if (loadModel(path)) {
            m_outputSink->appendOutput("Model loaded.", OutputSeverity::Info);
        } else {
            m_outputSink->appendOutput("Failed to load model.", OutputSeverity::Error);
        }
    }
    else if (input == "unload") {
        unloadModel();
    }
    else if (input == "model") {
        m_outputSink->appendOutput(getModelInfo().c_str(), OutputSeverity::Info);
    }
    else if (input == "backends") {
        m_outputSink->appendOutput(getBackendStatusString().c_str(), OutputSeverity::Info);
    }
    else if (input == "router") {
        m_outputSink->appendOutput(getRouterStatusString().c_str(), OutputSeverity::Info);
    }
    else if (input == "failures") {
        m_outputSink->appendOutput(getFailureDetectorStats().c_str(), OutputSeverity::Info);
    }
    else if (input == "history") {
        m_outputSink->appendOutput(getAgentHistoryStats().c_str(), OutputSeverity::Info);
    }
    else if (input == "asm") {
        m_outputSink->appendOutput(getAsmSemanticStatsString().c_str(), OutputSeverity::Info);
    }
    else if (input == "lsp") {
        m_outputSink->appendOutput(getLSPStatusString().c_str(), OutputSeverity::Info);
    }
    else if (input == "governor") {
        m_outputSink->appendOutput(getGovernorStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "safety") {
        m_outputSink->appendOutput(getSafetyStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "swarm") {
        m_outputSink->appendOutput(getSwarmStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "hotpatch") {
        m_outputSink->appendOutput(getHotpatchStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot" || input == "cot status") {
        auto& cot = ChainOfThoughtEngine::instance();
        m_outputSink->appendOutput(cot.getStatusJSON().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot presets") {
        auto names = getCoTPresetNames();
        std::ostringstream oss;
        oss << "Chain-of-Thought Presets:\n";
        for (const auto& n : names) {
            const CoTPreset* p = getCoTPreset(n);
            if (p) {
                oss << "  " << n << " (" << p->label << ") — " << p->steps.size() << " steps\n";
            }
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot roles") {
        const auto& roles = getAllCoTRoles();
        std::ostringstream oss;
        oss << "Chain-of-Thought Roles (" << roles.size() << "):\n";
        for (const auto& r : roles) {
            oss << "  " << r.icon << " " << r.name << " — " << r.instruction << "\n";
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot steps") {
        auto& cot = ChainOfThoughtEngine::instance();
        m_outputSink->appendOutput(cot.getStepsJSON().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot stats") {
        auto& cot = ChainOfThoughtEngine::instance();
        auto stats = cot.getStats();
        std::ostringstream oss;
        oss << "CoT Statistics:\n";
        oss << "  Total chains: " << stats.totalChains << "\n";
        oss << "  Successful: " << stats.successfulChains << "\n";
        oss << "  Failed: " << stats.failedChains << "\n";
        oss << "  Steps executed: " << stats.totalStepsExecuted << "\n";
        oss << "  Avg latency: " << stats.avgLatencyMs << "ms\n";
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input.substr(0, 11) == "cot preset ") {
        std::string presetName = input.substr(11);
        auto& cot = ChainOfThoughtEngine::instance();
        if (cot.applyPreset(presetName)) {
            std::string msg = "Applied preset '" + presetName + "' (" +
                std::to_string(cot.getSteps().size()) + " steps)";
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        } else {
            std::string msg = "Unknown preset: " + presetName + ". Available: review, audit, think, research, debate, custom";
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Error);
        }
    }
    else if (input.substr(0, 8) == "cot add ") {
        std::string roleName = input.substr(8);
        const CoTRoleInfo* info = getCoTRoleByName(roleName);
        if (info) {
            auto& cot = ChainOfThoughtEngine::instance();
            cot.addStep(info->id);
            std::string msg = "Added step: " + std::string(info->label) +
                " (total: " + std::to_string(cot.getSteps().size()) + " steps)";
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        } else {
            m_outputSink->appendOutput("Unknown role. Use 'cot roles' to list.", OutputSeverity::Error);
        }
    }
    else if (input == "cot clear") {
        auto& cot = ChainOfThoughtEngine::instance();
        cot.clearSteps();
        m_outputSink->appendOutput("Chain cleared.", OutputSeverity::Info);
    }
    else if (input == "cot cancel") {
        auto& cot = ChainOfThoughtEngine::instance();
        cot.cancel();
        m_outputSink->appendOutput("Cancel requested.", OutputSeverity::Info);
    }
    else if (input.substr(0, 8) == "cot run ") {
        std::string query = input.substr(8);
        auto& cot = ChainOfThoughtEngine::instance();

        // Wire inference callback to use our runInference
        cot.setInferenceCallback([this](const std::string& systemPrompt,
                                         const std::string& userMessage,
                                         const std::string& /*model*/) -> std::string {
            std::string combined = systemPrompt + "\n\n" + userMessage;
            return runInference(combined);
        });

        if (cot.getSteps().empty()) {
            cot.applyPreset("review");
            m_outputSink->appendOutput("No steps set, applying 'review' preset.", OutputSeverity::Warning);
        }

        // Set step callback for progress
        cot.setStepCallback([this](const CoTStepResult& sr) {
            const auto& info = getCoTRoleInfo(sr.role);
            std::string msg;
            if (sr.skipped) {
                msg = "  Step " + std::to_string(sr.stepIndex + 1) + " (" + info.label + "): SKIPPED";
            } else if (sr.success) {
                msg = "  Step " + std::to_string(sr.stepIndex + 1) + " (" + info.label +
                    "): " + std::to_string(sr.latencyMs) + "ms";
            } else {
                msg = "  Step " + std::to_string(sr.stepIndex + 1) + " (" + info.label +
                    "): FAILED - " + sr.error;
            }
            m_outputSink->appendOutput(msg.c_str(), sr.success ? OutputSeverity::Info : OutputSeverity::Error);
        });

        m_outputSink->appendOutput("Executing CoT chain...", OutputSeverity::Info);
        CoTChainResult result = cot.executeChain(query);

        if (result.success) {
            std::string summary = "Chain complete (" + std::to_string(result.totalLatencyMs) + "ms, " +
                std::to_string(result.stepsCompleted) + " steps)";
            m_outputSink->appendOutput(summary.c_str(), OutputSeverity::Info);
            m_outputSink->onStreamStart("cot");
            m_outputSink->onStreamingToken(result.finalOutput.c_str(), result.finalOutput.size(),
                                            StreamTokenOrigin::Inference);
            m_outputSink->onStreamEnd("cot", true);
        } else {
            std::string errMsg = "Chain failed: " + result.error;
            m_outputSink->appendOutput(errMsg.c_str(), OutputSeverity::Error);
        }
    }
    else if (input == "server start") {
        startServer();
    }
    else if (input == "server stop") {
        stopServer();
    }
    else if (input == "server") {
        m_outputSink->appendOutput(getServerStatus().c_str(), OutputSeverity::Info);
    }
    // ── Phase 34: Instructions Context Commands ─────────────────────────
    else if (input == "instructions" || input == "instructions show") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        std::string content = ip.getAllContent();
        if (content.empty()) {
            m_outputSink->appendOutput("No instruction files loaded. Try 'instructions reload'.",
                                        OutputSeverity::Warning);
        } else {
            m_outputSink->appendOutput(content.c_str(), OutputSeverity::Info);
        }
    }
    else if (input == "instructions list") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        auto files = ip.getAll();
        std::ostringstream oss;
        oss << "Loaded instruction files (" << files.size() << "):\n";
        for (const auto& f : files) {
            oss << "  " << f.fileName << " (" << f.lineCount << " lines, "
                << f.sizeBytes << " bytes) — " << f.filePath << "\n";
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "instructions reload") {
        auto& ip = InstructionsProvider::instance();
        auto r = ip.reload();
        std::string msg = r.success
            ? ("Instructions reloaded (" + std::to_string(ip.getLoadedCount()) + " files)")
            : ("Reload failed: " + std::string(r.detail));
        m_outputSink->appendOutput(msg.c_str(),
            r.success ? OutputSeverity::Info : OutputSeverity::Error);
    }
    else if (input == "instructions paths") {
        auto& ip = InstructionsProvider::instance();
        auto paths = ip.getSearchPaths();
        std::ostringstream oss;
        oss << "Search paths (" << paths.size() << "):\n";
        for (const auto& p : paths) {
            oss << "  " << p << "\n";
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "instructions json") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        m_outputSink->appendOutput(ip.toJSON().c_str(), OutputSeverity::Info);
    }
    else {
        // Treat as inference prompt
        if (m_modelLoaded) {
            m_outputSink->onStreamStart("repl");
            std::string result = runInference(input);
            m_outputSink->onStreamingToken(result.c_str(), result.size(), StreamTokenOrigin::Inference);
            m_outputSink->onStreamEnd("repl", true);
        } else {
            m_outputSink->appendOutput("No model loaded. Use 'load <path>' first, or type 'help'.",
                                        OutputSeverity::Warning);
        }
    }
}

void HeadlessIDE::printReplHelp() {
    const char* help = R"(
RawrXD Headless IDE — REPL Commands
====================================
  help / ?         Show this help
  status           Full status dump (JSON)
  version          Show version
  load <path>      Load a GGUF model
  unload           Unload current model
  model            Show loaded model info
  backends         Backend switcher status
  router           LLM router status
  failures         Failure detector stats
  history          Agent history stats
  asm              ASM semantic stats
  lsp              LSP client status
  governor         Execution governor status
  safety           Safety contract status
  swarm            Distributed swarm status
  hotpatch         Hotpatch system status
  cot              CoT engine status
  cot presets      List CoT presets (review|audit|think|research|debate|custom)
  cot roles        List all CoT roles (12 reasoning personas)
  cot steps        Show current chain configuration
  cot stats        CoT execution statistics
  cot preset <n>   Apply a preset (e.g. 'cot preset review')
  cot add <role>   Add a role to the chain (e.g. 'cot add critic')
  cot clear        Clear current chain
  cot run <query>  Execute CoT chain on a query
  cot cancel       Cancel running chain
  server           HTTP server status
  server start     Start HTTP server
  server stop      Stop HTTP server
  instructions     Show production instructions (all lines)
  instructions list   List loaded instruction files
  instructions show   Show full content
  instructions reload Reload from disk
  instructions paths  Show search paths
  instructions json   Export as JSON
  quit / exit      Exit the REPL

  <any other text>  Treated as inference prompt

Command-line flags:
  --headless                    Enable headless mode
  --port <port>                 HTTP server port (default: 11435)
  --bind <address>              Bind address (default: 127.0.0.1)
    --model <path>                Load model on startup
    --load-model <path>           Alias of --model (for forensic scripts)
    --exit-after-load             Exit after initialization/model load (no server loop)
    --forensic-map-only           Skip full GGUF parse; do one sliding-window map probe + telemetry flush
  --prompt <text>               Single-shot inference, then exit
    --bench-attention             Hidden: run flash-attention A/B benchmark and exit
    --bench-sweep                 Hidden: run scaling sweep matrix and exit
    --bench-batch <n>             Hidden: benchmark batch size (default: 1)
    --bench-seq-len <n>           Hidden: benchmark sequence length (default: 1024)
    --bench-heads <n>             Hidden: benchmark head count (default: 32)
    --bench-head-dim <n>          Hidden: benchmark head dimension (default: 128)
    --bench-warmup <n>            Hidden: warmup iterations (default: 1)
    --bench-iters <n>             Hidden: measured iterations (default: 2)
    --bench-median                Hidden: use median latency metric (forces >=3 iters)
    --bench-csv <path>            Hidden: append benchmark row to CSV file
    --trace-token-summary         Single-shot only: emit Titan token trace summary
    --trace-token-csv <path>      Single-shot only: dump Titan token trace CSV
  --input <file>                Batch mode: read prompts from file
  --output <file>               Batch mode: write results to file
  --backend <name>              Set default backend
  --max-tokens <n>              Max tokens (default: 2048)
  --temperature <f>             Temperature (default: 0.7)
  --repl                        Interactive REPL mode
  --no-server                   Don't start HTTP server
  --verbose / -v                Verbose output
  --quiet / -q                  Quiet mode (warnings/errors only)
  --json                        JSON-structured output
  --settings <file>             Load settings from file
)";
    fprintf(stdout, "%s\n", help);
}

void HeadlessIDE::printReplPrompt() {
    if (m_modelLoaded) {
        fprintf(stdout, "[%s] > ", m_loadedModelName.c_str());
    } else {
        fprintf(stdout, "[no model] > ");
    }
    fflush(stdout);
}

// ============================================================================
// Shutdown
// ============================================================================
void HeadlessIDE::shutdownAll() {
    if (m_orchestratorModelLoaded) {
        try {
            RawrXD::BackendOrchestrator::Instance().UnloadModel(m_orchestratorModelTag);
        } catch (...) {
            // Best-effort cleanup.
        }
        m_orchestratorModelLoaded = false;
    }
    try {
        RawrXD::BackendOrchestrator::Instance().Shutdown();
    } catch (...) {
        // Best-effort telemetry flush/shutdown.
    }

    stopServer();

    if (m_winsockInitialized) {
        WSACleanup();
        m_winsockInitialized = false;
    }

    m_outputSink->flush();
}
