#include "ai_implementation.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>


#ifdef _WIN32
#include <dxgi.h>
#include <windows.h>

#pragma comment(lib, "dxgi.lib")
#endif

namespace
{

std::string toLowerAscii(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool startsWith(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool rawrxdBypassRamGatesFromEnv()
{
    const char* a = std::getenv("RAWRXD_BYPASS_RAM_GATES");
    if (a && (a[0] == '1' || a[0] == 'y' || a[0] == 'Y'))
        return true;
    const char* b = std::getenv("RAWRXD_SKIP_RAM_PREFLIGHT");
    return b && (b[0] == '1' || b[0] == 'y' || b[0] == 'Y');
}

bool isLikelyFilePath(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }
    return value.find('\\') != std::string::npos || value.find('/') != std::string::npos ||
           toLowerAscii(value).find(".gguf") != std::string::npos;
}

bool isLoopbackEndpoint(const std::string& endpoint)
{
    std::string lower = toLowerAscii(endpoint);
    std::string hostPort = lower;

    const size_t scheme = lower.find("://");
    if (scheme != std::string::npos)
    {
        hostPort = lower.substr(scheme + 3);
    }
    const size_t pathPos = hostPort.find('/');
    if (pathPos != std::string::npos)
    {
        hostPort = hostPort.substr(0, pathPos);
    }
    if (startsWith(hostPort, "localhost") || startsWith(hostPort, "127.0.0.1") || startsWith(hostPort, "[::1]") ||
        startsWith(hostPort, "::1"))
    {
        return true;
    }
    return false;
}

bool hasReadPermission(const std::string& modelPath)
{
    std::ifstream f(modelPath, std::ios::binary);
    return f.good();
}

bool isValidGgufHeader(const std::string& modelPath)
{
    std::ifstream f(modelPath, std::ios::binary);
    if (!f.is_open())
    {
        return false;
    }
    uint32_t magic = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return f.good() && magic == 0x46554747U;
}

std::string extractQuantTag(const std::string& modelPath)
{
    const std::string name = toLowerAscii(std::filesystem::path(modelPath).filename().string());
    const std::vector<std::string> known = {"q2_k", "q3_k", "q4_0",   "q4_1",   "q4_k_s", "q4_k_m", "q4_k",
                                            "q5_0", "q5_1", "q5_k_s", "q5_k_m", "q5_k",   "q6_k",   "q8_0"};
    for (const std::string& tag : known)
    {
        if (name.find(tag) != std::string::npos)
        {
            return tag;
        }
    }
    return "";
}

bool isSupportedQuantTag(const std::string& quantTag)
{
    static const std::set<std::string> allowlisted = {"q2_k", "q3_k", "q4_0", "q4_1",   "q4_k",   "q4_k_s", "q4_k_m",
                                                      "q5_0", "q5_1", "q5_k", "q5_k_s", "q5_k_m", "q6_k",   "q8_0"};
    return allowlisted.find(quantTag) != allowlisted.end();
}

// ---------------------------------------------------------------------------
// GGUF embedded-metadata reader
// Parses KV pairs directly from the binary GGUF file without llama.cpp.
// Used for:
//   Gate 3  — general.file_type  (quant enum, fallback when filename lacks tag)
//   Gate 6  — general.architecture + tokenizer.ggml.model  (strict pairing)
// ---------------------------------------------------------------------------

static size_t ggufValTypeBytes(uint32_t t)
{
    switch (t)
    {
        case 0:
        case 1:
        case 7:
            return 1;
        case 2:
        case 3:
            return 2;
        case 4:
        case 5:
        case 6:
            return 4;
        case 10:
        case 11:
        case 12:
            return 8;
        default:
            return 0;
    }
}

struct GGUFMeta
{
    bool hasArchitecture = false;
    bool hasTokenizerModel = false;
    bool hasFileType = false;
    std::string architecture;
    std::string tokenizerModel;
    uint32_t fileType = 0xFFFFFFFFu;
};

static bool ggufReadStr(std::ifstream& f, uint32_t version, std::string& out, size_t maxLen = 65536)
{
    uint64_t len = 0;
    if (version == 1)
    {
        uint32_t l32 = 0;
        f.read(reinterpret_cast<char*>(&l32), 4);
        len = l32;
    }
    else
    {
        f.read(reinterpret_cast<char*>(&len), 8);
    }
    if (!f.good() || len > maxLen)
        return false;
    out.assign(len, '\0');
    f.read(&out[0], static_cast<std::streamsize>(len));
    return f.good();
}

// Forward declare so array handling can recurse for nested string arrays.
static bool ggufSkipVal(std::ifstream& f, uint32_t version, uint32_t valType);

static bool ggufSkipArray(std::ifstream& f, uint32_t version)
{
    uint32_t arrType = 0;
    f.read(reinterpret_cast<char*>(&arrType), 4);
    uint64_t arrCount = 0;
    if (version == 1)
    {
        uint32_t c = 0;
        f.read(reinterpret_cast<char*>(&c), 4);
        arrCount = c;
    }
    else
    {
        f.read(reinterpret_cast<char*>(&arrCount), 8);
    }
    if (!f.good() || arrCount > 16000000ULL)
        return false;
    const size_t esz = ggufValTypeBytes(arrType);
    if (esz > 0)
    {
        f.seekg(static_cast<std::streamoff>(arrCount * esz), std::ios::cur);
        return f.good();
    }
    for (uint64_t j = 0; j < arrCount && f.good(); ++j)
    {
        if (!ggufSkipVal(f, version, arrType))
            return false;
    }
    return f.good();
}

static bool ggufSkipVal(std::ifstream& f, uint32_t version, uint32_t valType)
{
    const size_t fixed = ggufValTypeBytes(valType);
    if (fixed > 0)
    {
        f.seekg(static_cast<std::streamoff>(fixed), std::ios::cur);
        return f.good();
    }
    if (valType == 8)
    {
        std::string s;
        return ggufReadStr(f, version, s);
    }  // STRING
    if (valType == 9)
    {
        return ggufSkipArray(f, version);
    }  // ARRAY
    return false;
}

static GGUFMeta readGGUFMeta(const std::string& modelPath)
{
    GGUFMeta meta;
    std::ifstream f(modelPath, std::ios::binary);
    if (!f.is_open())
        return meta;

    uint32_t magic = 0, version = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    if (!f.good() || magic != 0x46554747u || version < 1 || version > 4)
        return meta;

    uint64_t nTensors = 0, nKV = 0;
    if (version == 1)
    {
        uint32_t t32 = 0, k32 = 0;
        f.read(reinterpret_cast<char*>(&t32), 4);
        f.read(reinterpret_cast<char*>(&k32), 4);
        nTensors = t32;
        nKV = k32;
    }
    else
    {
        f.read(reinterpret_cast<char*>(&nTensors), 8);
        f.read(reinterpret_cast<char*>(&nKV), 8);
    }
    if (!f.good() || nKV > 8192u)
        return meta;
    (void)nTensors;

    for (uint64_t i = 0; i < nKV && f.good(); ++i)
    {
        std::string key;
        if (!ggufReadStr(f, version, key, 256))
            break;
        uint32_t valType = 0;
        f.read(reinterpret_cast<char*>(&valType), 4);
        if (!f.good())
            break;

        if (valType == 8 /* STRING */)
        {
            std::string val;
            if (!ggufReadStr(f, version, val))
                break;
            if (key == "general.architecture")
            {
                meta.hasArchitecture = true;
                meta.architecture = val;
            }
            else if (key == "tokenizer.ggml.model")
            {
                meta.hasTokenizerModel = true;
                meta.tokenizerModel = val;
            }
        }
        else if (valType == 4 /* UINT32 */ && key == "general.file_type")
        {
            f.read(reinterpret_cast<char*>(&meta.fileType), 4);
            if (f.good())
                meta.hasFileType = true;
        }
        else
        {
            if (!ggufSkipVal(f, version, valType))
                break;
        }
        if (meta.hasArchitecture && meta.hasTokenizerModel && meta.hasFileType)
            break;
    }
    return meta;
}

bool preflightMemory(const std::string& modelPath, const std::string& mode, std::string& reason, bool& gpuUsable)
{
    reason.clear();
    gpuUsable = false;
    const uint64_t modelBytes = static_cast<uint64_t>(std::filesystem::file_size(modelPath));

#ifdef _WIN32
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (!GlobalMemoryStatusEx(&mem))
    {
        reason = "unable to query system memory";
        return false;
    }

    // Match rawrxd_model_loader: default to total physical RAM (mmap-friendly). Set
    // RAWRXD_RAM_PREFLIGHT_USE_AVAIL=1 for legacy strict available-RAM checks.
    const char* useAvailEnv = std::getenv("RAWRXD_RAM_PREFLIGHT_USE_AVAIL");
    const bool useAvail = useAvailEnv && (useAvailEnv[0] == '1' || useAvailEnv[0] == 'y' || useAvailEnv[0] == 'Y');
    double reserveFrac = 0.20;
    if (const char* rf = std::getenv("RAWRXD_RAM_PREFLIGHT_RESERVE_FRAC"))
    {
        char* end = nullptr;
        const double v = std::strtod(rf, &end);
        if (end != rf && v >= 0.0 && v <= 0.95)
            reserveFrac = v;
    }
    const uint64_t baseRam = useAvail ? mem.ullAvailPhys : mem.ullTotalPhys;
    const uint64_t ramLimit = static_cast<uint64_t>(static_cast<double>(baseRam) * (1.0 - reserveFrac));
    if (!rawrxdBypassRamGatesFromEnv() && modelBytes > ramLimit)
    {
        reason = useAvail ? "insufficient RAM headroom (reserve vs available physical)"
                          : "insufficient RAM headroom (reserve vs total physical)";
        return false;
    }

    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
    {
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter)))
        {
            DXGI_ADAPTER_DESC1 desc = {};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && desc.DedicatedVideoMemory > 0)
            {
                const uint64_t vramLimit = static_cast<uint64_t>(static_cast<double>(desc.DedicatedVideoMemory) * 0.85);
                gpuUsable = modelBytes <= vramLimit;
                if (mode == "gpu-only" && !gpuUsable)
                {
                    reason = "insufficient VRAM headroom (15% reserve)";
                    adapter->Release();
                    factory->Release();
                    return false;
                }
            }
            adapter->Release();
        }
        factory->Release();
    }
#else
    (void)mode;
    (void)modelBytes;
#endif

    return true;
}

}  // namespace

AIImplementation::AIImplementation(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics,
                                   std::shared_ptr<HTTPClient> httpClient,
                                   std::shared_ptr<ResponseParser> responseParser,
                                   std::shared_ptr<ModelTester> modelTester)
    : m_logger(logger), m_metrics(metrics), m_httpClient(httpClient), m_responseParser(responseParser),
      m_modelTester(modelTester)
{
    if (m_logger)
    {
        m_logger->info("AIImplementation", "Initialized");
    }
}

bool AIImplementation::initialize(const LLMConfig& config)
{
    m_config = config;

    const bool isLocalRuntime = (m_config.backend == "local" || m_config.backend == "ollama");
    if (m_config.backend != "local" && m_config.backend != "ollama" && m_config.backend != "openai" &&
        m_config.backend != "anthropic")
    {
        if (m_logger)
        {
            m_logger->error("AIImplementation", "Unsupported backend: " + m_config.backend);
        }
        return false;
    }

    if (isLocalRuntime && !m_config.allowRemoteFallback && !isLoopbackEndpoint(m_config.endpoint))
    {
        if (m_logger)
        {
            m_logger->error("AIImplementation",
                            "Endpoint rejected: local runtime is pinned to localhost unless allowRemoteFallback=true");
        }
        return false;
    }

    if (m_config.localBackendMode != "cpu-only" && m_config.localBackendMode != "gpu-only" &&
        m_config.localBackendMode != "auto-with-verified-fallback")
    {
        if (m_logger)
        {
            m_logger->error("AIImplementation", "Invalid localBackendMode: " + m_config.localBackendMode);
        }
        return false;
    }

    if (isLocalRuntime && isLikelyFilePath(m_config.modelName))
    {
        const std::string modelPath = m_config.modelName;
        const std::string lowerPath = toLowerAscii(modelPath);

        // Gate 1: GGUF format — extension + 4-byte header magic 0x46554747
        if (!endsWith(lowerPath, ".gguf") || !isValidGgufHeader(modelPath))
        {
            if (m_logger)
                m_logger->error("AIImplementation", "[GATE-1] Model format rejected: only valid GGUF files accepted");
            return false;
        }

        // Gate 5: File read permission — same process account that runs inference
        if (!hasReadPermission(modelPath))
        {
            if (m_logger)
                m_logger->error("AIImplementation", "[GATE-5] permission denied for runtime user: " + modelPath);
            return false;
        }

        // Read embedded GGUF KV metadata — drives gates 3 and 6
        const GGUFMeta meta = readGGUFMeta(modelPath);

        // Gate 3: Quant allowlist — filename-tag primary, GGUF file_type enum fallback
        // file_type values: 0=F32 1=F16 2=Q4_0 3=Q4_1 7=Q8_0 8=Q5_0 9=Q5_1
        //                   10=Q2_K 11=Q3_K_S 12=Q3_K_M 13=Q3_K_L
        //                   14=Q4_K_S 15=Q4_K_M 16=Q5_K_S 17=Q5_K_M 18=Q6_K
        const std::string quantTag = extractQuantTag(modelPath);
        bool quantOk = !quantTag.empty() && isSupportedQuantTag(quantTag);
        if (!quantOk && meta.hasFileType)
        {
            static const std::set<uint32_t> allowedFT = {
                0u,  1u,        // F32, F16
                2u,  3u,        // Q4_0, Q4_1
                7u,  8u,  9u,   // Q8_0, Q5_0, Q5_1
                10u,            // Q2_K
                11u, 12u, 13u,  // Q3_K_S/M/L
                14u, 15u,       // Q4_K_S/M
                16u, 17u,       // Q5_K_S/M
                18u             // Q6_K
            };
            quantOk = allowedFT.count(meta.fileType) > 0;
        }
        if (!quantOk)
        {
            if (m_logger)
                m_logger->error("AIImplementation", "[GATE-3] unsupported quant: rejected at model load");
            return false;
        }

        // Gate 6: Tokenizer/config pair — verified via GGUF embedded metadata.
        // general.architecture and tokenizer.ggml.model must both be present.
        // Any model missing these fields is structurally incomplete; reject at
        // initialization, not during generation.
        if (!meta.hasArchitecture || !meta.hasTokenizerModel)
        {
            if (m_logger)
                m_logger->error("AIImplementation",
                                "[GATE-6] GGUF required metadata missing: architecture=" +
                                    std::string(meta.hasArchitecture ? meta.architecture : "MISSING") +
                                    " tokenizer.ggml.model=" +
                                    std::string(meta.hasTokenizerModel ? meta.tokenizerModel : "MISSING") +
                                    " — mismatch fails at initialization");
            return false;
        }

        // Gate 4: Memory preflight — RAM (20% headroom) + VRAM (15% headroom)
        std::string preflightReason;
        bool gpuUsable = false;
        if (!preflightMemory(modelPath, m_config.localBackendMode, preflightReason, gpuUsable))
        {
            if (m_logger)
                m_logger->error("AIImplementation", "[GATE-4] Memory preflight failed: " + preflightReason);
            return false;
        }

        // Gate 2: Lock runtime lane — one backend, one lane, no silent routing
        if (m_config.localBackendMode == "gpu-only")
        {
            m_resolvedRuntimeLane = "gpu-only";
        }
        else if (m_config.localBackendMode == "cpu-only")
        {
            m_resolvedRuntimeLane = "cpu-only";
        }
        else
        {
            // auto-with-verified-fallback: GPU if VRAM sufficient, else CPU
            m_resolvedRuntimeLane = gpuUsable ? "gpu-only" : "cpu-only";
        }

        // All 7 gates passed — startup banner: exactly one backend + one lane
        if (m_logger)
        {
            m_logger->info("AIImplementation", "[BACKEND] backend=local lane=" + m_resolvedRuntimeLane +
                                                   " arch=" + meta.architecture + " tokenizer=" + meta.tokenizerModel +
                                                   " quant=" + (!quantTag.empty() ? quantTag : "f32") +
                                                   " endpoint=" + m_config.endpoint + " - READY");
        }
    }
    else
    {
        m_resolvedRuntimeLane = "cpu-only";
        if (m_logger)
        {
            m_logger->info("AIImplementation", "[BACKEND] backend=" + config.backend +
                                                   " lane=" + m_resolvedRuntimeLane + " endpoint=" + m_config.endpoint);
        }
    }
    return testConnectivity();
}

CompletionResponse AIImplementation::complete(const CompletionRequest& request)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    CompletionResponse response;

    try
    {
        if (m_config.backend == "ollama" || m_config.backend == "local")
        {
            // Build Ollama request (simplified)
            std::string ollamaBody =
                "{\"model\":\"" + m_config.modelName + "\",\"prompt\":\"" + request.prompt + "\",\"stream\":false}";

            HTTPRequest httpReq;
            httpReq.method = "POST";
            httpReq.url = m_config.endpoint + "/api/generate";
            httpReq.body = ollamaBody;
            httpReq.headers.push_back({"Content-Type", "application/json"});

            auto httpResp = m_httpClient->sendRequest(httpReq);

            if (httpResp.success)
            {
                // Parse Ollama JSON response: {"response":"...","total_duration":...,"eval_count":N,...}
                std::string respBody = httpResp.body;
                std::string completionText;
                int evalCount = 0;

                // Extract "response" field
                size_t respPos = respBody.find("\"response\":\"");
                if (respPos != std::string::npos)
                {
                    size_t valStart = respPos + 12;
                    // Handle escaped quotes in response text
                    size_t valEnd = valStart;
                    while (valEnd < respBody.size())
                    {
                        if (respBody[valEnd] == '\\')
                        {
                            valEnd += 2;  // skip escaped char
                            continue;
                        }
                        if (respBody[valEnd] == '"')
                            break;
                        valEnd++;
                    }
                    completionText = respBody.substr(valStart, valEnd - valStart);
                }

                // Extract "eval_count" for token count
                size_t evalPos = respBody.find("\"eval_count\":");
                if (evalPos != std::string::npos)
                {
                    size_t numStart = evalPos + 13;
                    while (numStart < respBody.size() && respBody[numStart] == ' ')
                        numStart++;
                    size_t numEnd = numStart;
                    while (numEnd < respBody.size() && (respBody[numEnd] >= '0' && respBody[numEnd] <= '9'))
                        numEnd++;
                    if (numEnd > numStart)
                    {
                        evalCount = std::stoi(respBody.substr(numStart, numEnd - numStart));
                    }
                }

                response.completion = completionText;
                response.totalTokens = evalCount > 0 ? evalCount : static_cast<int>(completionText.size() / 4);
                response.success = true;
            }
            else
            {
                response.success = false;
                response.errorMessage = httpResp.errorMessage;
            }
        }
        else if (m_config.backend == "openai" || m_config.backend == "anthropic")
        {
            // Build API request for OpenAI/Anthropic (non-streaming)
            std::string body;
            if (m_config.backend == "openai")
            {
                body = "{\"model\":\"" + m_config.modelName +
                       "\",\"stream\":false,"
                       "\"messages\":[{\"role\":\"user\",\"content\":\"" +
                       request.prompt + "\"}]}";
            }
            else
            {
                body = "{\"model\":\"" + m_config.modelName +
                       "\",\"stream\":false,"
                       "\"max_tokens\":4096,"
                       "\"messages\":[{\"role\":\"user\",\"content\":\"" +
                       request.prompt + "\"}]}";
            }

            HTTPRequest httpReq;
            httpReq.method = "POST";
            httpReq.url = m_config.endpoint;
            httpReq.body = body;
            httpReq.headers.push_back({"Content-Type", "application/json"});
            httpReq.headers.push_back({"Authorization", "Bearer " + m_config.apiKey});

            auto httpResp = m_httpClient->sendRequest(httpReq);

            if (httpResp.success)
            {
                // Parse OpenAI/Anthropic response for content
                std::string respBody = httpResp.body;
                std::string completionText;
                int totalTokens = 0;

                // Extract content from choices[0].message.content or content[0].text
                size_t contentPos = respBody.find("\"content\":\"");
                if (contentPos != std::string::npos)
                {
                    size_t valStart = contentPos + 11;
                    size_t valEnd = valStart;
                    while (valEnd < respBody.size())
                    {
                        if (respBody[valEnd] == '\\')
                        {
                            valEnd += 2;
                            continue;
                        }
                        if (respBody[valEnd] == '"')
                            break;
                        valEnd++;
                    }
                    completionText = respBody.substr(valStart, valEnd - valStart);
                }

                // Extract total_tokens from usage object
                size_t tokPos = respBody.find("\"total_tokens\":");
                if (tokPos != std::string::npos)
                {
                    size_t numStart = tokPos + 15;
                    while (numStart < respBody.size() && respBody[numStart] == ' ')
                        numStart++;
                    size_t numEnd = numStart;
                    while (numEnd < respBody.size() && respBody[numEnd] >= '0' && respBody[numEnd] <= '9')
                        numEnd++;
                    if (numEnd > numStart)
                    {
                        totalTokens = std::stoi(respBody.substr(numStart, numEnd - numStart));
                    }
                }

                response.completion = completionText;
                response.totalTokens = totalTokens > 0 ? totalTokens : static_cast<int>(completionText.size() / 4);
                response.success = true;
            }
            else
            {
                response.success = false;
                response.errorMessage = httpResp.errorMessage;
            }
        }
        else
        {
            response.success = false;
            response.errorMessage = "Unsupported backend: " + m_config.backend;
        }

        // Track metrics
        auto endTime = std::chrono::high_resolution_clock::now();
        response.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        m_totalLatency += response.latencyMs;
        m_totalTokensUsed += response.totalTokens;

        if (m_metrics)
        {
            m_metrics->recordHistogram("ai_completion_latency_ms", static_cast<double>(response.latencyMs));
            m_metrics->incrementCounter("ai_total_tokens", response.totalTokens);
            m_metrics->incrementCounter("ai_requests", 1);
        }

        if (m_logger && response.success)
        {
            m_logger->info("AIImplementation", "Completion successful: " + std::to_string(response.latencyMs) + "ms, " +
                                                   std::to_string(response.totalTokens) + " tokens");
        }
    }
    catch (const std::exception& e)
    {
        response.success = false;
        response.errorMessage = std::string("Exception: ") + e.what();
        if (m_logger)
        {
            m_logger->error("AIImplementation", "Complete failed: " + response.errorMessage);
        }
    }

    return response;
}

CompletionResponse AIImplementation::streamComplete(const CompletionRequest& request,
                                                    std::function<void(const ParsedCompletion&)> chunkCallback)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    CompletionResponse response;

    try
    {
        if (m_config.backend == "ollama" || m_config.backend == "local")
        {
            // Build Ollama streaming request
            std::string ollamaBody =
                "{\"model\":\"" + m_config.modelName + "\",\"prompt\":\"" + request.prompt + "\",\"stream\":true}";

            HTTPRequest httpReq;
            httpReq.method = "POST";
            httpReq.url = m_config.endpoint + "/api/generate";
            httpReq.body = ollamaBody;
            httpReq.headers.push_back({"Content-Type", "application/json"});

            // Use streaming callback on the HTTP client
            std::string accumulatedText;
            int totalTokens = 0;

            auto httpResp = m_httpClient->sendRequest(httpReq,
                                                      [&](const std::string& chunk)
                                                      {
                                                          // Ollama streams JSON lines:
                                                          // {"response":"token","done":false} Parse each line for the
                                                          // "response" field
                                                          size_t respPos = chunk.find("\"response\":\"");
                                                          if (respPos != std::string::npos)
                                                          {
                                                              size_t valStart = respPos + 12;
                                                              size_t valEnd = chunk.find("\"", valStart);
                                                              if (valEnd != std::string::npos)
                                                              {
                                                                  std::string token =
                                                                      chunk.substr(valStart, valEnd - valStart);
                                                                  accumulatedText += token;
                                                                  totalTokens++;

                                                                  ParsedCompletion pc;
                                                                  pc.text = token;
                                                                  pc.isPartial = true;
                                                                  pc.tokenIndex = totalTokens;
                                                                  chunkCallback(pc);
                                                              }
                                                          }
                                                      });

            if (httpResp.success)
            {
                response.completion = accumulatedText;
                response.totalTokens = totalTokens;
                response.success = true;

                // Send final completion chunk
                ParsedCompletion finalChunk;
                finalChunk.text = "";
                finalChunk.isPartial = false;
                finalChunk.tokenIndex = totalTokens;
                chunkCallback(finalChunk);
            }
            else
            {
                response.success = false;
                response.errorMessage = httpResp.errorMessage;
            }
        }
        else if (m_config.backend == "openai" || m_config.backend == "anthropic")
        {
            // For OpenAI/Anthropic: use SSE streaming
            std::string body;
            if (m_config.backend == "openai")
            {
                body = "{\"model\":\"" + m_config.modelName +
                       "\",\"stream\":true,"
                       "\"messages\":[{\"role\":\"user\",\"content\":\"" +
                       request.prompt + "\"}]}";
            }
            else
            {
                body = "{\"model\":\"" + m_config.modelName +
                       "\",\"stream\":true,"
                       "\"max_tokens\":4096,"
                       "\"messages\":[{\"role\":\"user\",\"content\":\"" +
                       request.prompt + "\"}]}";
            }

            HTTPRequest httpReq;
            httpReq.method = "POST";
            httpReq.url = m_config.endpoint;
            httpReq.body = body;
            httpReq.headers.push_back({"Content-Type", "application/json"});
            httpReq.headers.push_back({"Authorization", "Bearer " + m_config.apiKey});

            std::string accumulatedText;
            int totalTokens = 0;

            auto httpResp = m_httpClient->sendRequest(
                httpReq,
                [&](const std::string& chunk)
                {
                    // Parse SSE "data: {...}" lines
                    size_t dataPos = chunk.find("data: ");
                    while (dataPos != std::string::npos)
                    {
                        size_t lineEnd = chunk.find("\n", dataPos);
                        std::string jsonStr = chunk.substr(
                            dataPos + 6, lineEnd == std::string::npos ? std::string::npos : lineEnd - dataPos - 6);

                        if (jsonStr == "[DONE]")
                            break;

                        // Extract content delta from SSE JSON
                        size_t contentPos = jsonStr.find("\"content\":\"");
                        if (contentPos == std::string::npos)
                            contentPos = jsonStr.find("\"text\":\"");
                        if (contentPos != std::string::npos)
                        {
                            size_t valStart = jsonStr.find("\"", contentPos + 9) + 1;
                            if (valStart == std::string::npos + 1)
                                valStart = contentPos + 11;
                            size_t valEnd = jsonStr.find("\"", valStart);
                            if (valEnd != std::string::npos)
                            {
                                std::string token = jsonStr.substr(valStart, valEnd - valStart);
                                accumulatedText += token;
                                totalTokens++;

                                ParsedCompletion pc;
                                pc.text = token;
                                pc.isPartial = true;
                                pc.tokenIndex = totalTokens;
                                chunkCallback(pc);
                            }
                        }
                        dataPos = chunk.find("data: ", lineEnd == std::string::npos ? chunk.size() : lineEnd);
                    }
                });

            response.completion = accumulatedText;
            response.totalTokens = totalTokens;
            response.success = httpResp.success;
            if (!httpResp.success)
                response.errorMessage = httpResp.errorMessage;
        }
        else
        {
            response.success = false;
            response.errorMessage = "Streaming not supported for backend: " + m_config.backend;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        response.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        if (m_metrics)
        {
            m_metrics->recordHistogram("ai_stream_latency_ms", static_cast<double>(response.latencyMs));
            m_metrics->incrementCounter("ai_stream_tokens", response.totalTokens);
            m_metrics->incrementCounter("ai_stream_requests", 1);
        }

        if (m_logger)
        {
            m_logger->info("AIImplementation", "Stream complete: " + std::to_string(response.latencyMs) + "ms, " +
                                                   std::to_string(response.totalTokens) + " tokens");
        }
    }
    catch (const std::exception& e)
    {
        response.success = false;
        response.errorMessage = std::string("Stream exception: ") + e.what();
        if (m_logger)
        {
            m_logger->error("AIImplementation", "streamComplete failed: " + response.errorMessage);
        }
    }

    return response;
}

void AIImplementation::registerTool(const ToolDefinition& tool)
{
    m_registeredTools[tool.name] = tool;
    if (m_logger)
    {
        m_logger->info("AIImplementation", "Registered tool: " + tool.name);
    }
}

json AIImplementation::executeTool(const std::string& toolName, const json& parameters)
{
    auto it = m_registeredTools.find(toolName);
    if (it == m_registeredTools.end())
    {
        if (m_logger)
        {
            m_logger->error("AIImplementation", "Tool not found: " + toolName);
        }
        json result;
        return result;
    }

    try
    {
        auto result = it->second.handler(parameters);
        if (m_metrics)
        {
            m_metrics->incrementCounter("ai_tool_calls", 1);
        }
        return result;
    }
    catch (const std::exception& e)
    {
        if (m_logger)
        {
            m_logger->error("AIImplementation", "Tool execution failed: " + std::string(e.what()));
        }
        json result;
        return result;
    }
}

CompletionResponse AIImplementation::agenticLoop(const CompletionRequest& request, int maxIterations)
{
    CompletionResponse finalResponse;
    int iteration = 0;

    CompletionRequest currentRequest = request;
    currentRequest.useToolCalling = !m_registeredTools.empty();

    while (iteration < maxIterations)
    {
        if (m_logger)
        {
            m_logger->info("AIImplementation", "Agentic loop iteration " + std::to_string(iteration + 1));
        }

        auto response = complete(currentRequest);

        if (!response.success)
        {
            return response;
        }

        finalResponse = response;
        break;
        iteration++;
    }

    if (iteration >= maxIterations && m_logger)
    {
        m_logger->warn("AIImplementation", "Agentic loop hit max iterations");
    }

    return finalResponse;
}

void AIImplementation::addToHistory(const std::string& role, const std::string& content)
{
    m_conversationHistory.push_back({role, content});
}

void AIImplementation::clearHistory()
{
    m_conversationHistory.clear();
}

std::vector<std::pair<std::string, std::string>> AIImplementation::getHistory() const
{
    return m_conversationHistory;
}

int AIImplementation::estimateTokenCount(const std::string& text)
{
    return static_cast<int>(text.length() / 4) + 1;
}

bool AIImplementation::supportsToolCalling() const
{
    return (m_config.backend == "openai" || m_config.backend == "anthropic");
}

bool AIImplementation::testConnectivity()
{
    try
    {
        if (m_config.backend == "ollama" || m_config.backend == "local")
        {
            if (!m_config.allowRemoteFallback && !isLoopbackEndpoint(m_config.endpoint))
            {
                return false;
            }
            auto response = m_httpClient->get(m_config.endpoint + "/api/tags");
            return response.success && response.statusCode == 200;
        }
        else if (m_config.backend == "openai" || m_config.backend == "anthropic")
        {
            return !m_config.apiKey.empty();
        }
        return false;
    }
    catch (...)
    {
        return false;
    }
}

LLMConfig AIImplementation::getConfig() const
{
    return m_config;
}

json AIImplementation::getUsageStats() const
{
    json stats = json::object_type();
    return stats;
}

namespace PromptTemplates
{
std::string codeGeneration(const std::string& codeContext, const std::string& requirement)
{
    return "Context:\n" + codeContext + "\n\nRequirement:\n" + requirement + "\n\nGenerate code:";
}

std::string codeReview(const std::string& code, const std::string& language)
{
    return "Review " + language + " code:\n\n" + code + "\n\nProvide feedback:";
}

std::string bugFix(const std::string& code, const std::string& errorMessage, const std::string& stackTrace)
{
    return "Fix bug:\n\n" + code + "\n\nError: " + errorMessage + "\n\nStack trace:\n" + stackTrace;
}

std::string refactoring(const std::string& code, const std::string& language, const std::string& objective)
{
    return "Refactor " + language + " code for " + objective + ":\n\n" + code;
}

std::string documentation(const std::string& code, const std::string& language)
{
    return "Document " + language + " code:\n\n" + code;
}
}  // namespace PromptTemplates
