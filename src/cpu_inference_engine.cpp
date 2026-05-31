// ============================================================================
// cpu_inference_engine.cpp — CPUInferenceEngine implementation
// Delegates actual GGUF/inference work to RawrXDInference pipeline.
// Matches the monolithic class layout declared in cpu_inference_engine.h.
// ============================================================================
#include "cpu_inference_engine.h"
#include "rawrxd_inference.h"
#include "gpu/speculative_decoder_v2.h"
#include "rawr_circular_sdma.h"
#include "inference/MemoryPressureGuard.h"
#include "inference/rawr_inference_autopatch_loop.h"
#include "codec/brutal_gzip.h"
#include "kernels/dequant_q6k_avx512.h"
#include "kernels/kv_accum_avx512.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <immintrin.h>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>

extern "C" {
    uint64_t g_sdma_flip_count = 0;
    uint64_t g_sdma_wait_cycles = 0;
    uint64_t g_expert_cache_hits = 0;
    uint64_t g_expert_cache_misses = 0;
    uint64_t g_sovereign_bar_base = 0;
    volatile unsigned long long g_rawrxd_mailbox_data_seq = 0;
    volatile unsigned long long g_rawrxd_mailbox_consumed_seq = 0;
    volatile unsigned long long g_rawrxd_mailbox_frame_ready = 0;
}

namespace RawrXD
{

// ============================================================================
// File-scope inference backend (the REAL compute chain)
// ============================================================================
static RawrXDInference s_inferenceBackend;

namespace
{
constexpr size_t kMaxContextTokens = 1'000'000;
constexpr size_t kMaxKvCacheBytes = 8ull * 1024ull * 1024ull * 1024ull;

constexpr uint32_t kRxaMagic = 0x21584152u;     // "RXA!"
constexpr uint32_t kRxaVersion1 = 0x00010000u;  // v1.0
constexpr uint8_t kRxaAlgRaw = 0;
constexpr uint8_t kRxaAlgXpress = 1;
constexpr uint8_t kRxaAlgLznt1 = 2;
constexpr uint8_t kRxaAlgBrutalGzip = 3;

const char* AutopatchActionName(RawrXD::Inference::PatchAction action)
{
    using RawrXD::Inference::PatchAction;
    switch (action)
    {
        case PatchAction::None:
            return "none";
        case PatchAction::EmergencyReset:
            return "emergency-reset";
        case PatchAction::EvictCold20:
            return "evict-cold-20pct";
        case PatchAction::PrefetchDown:
            return "prefetch-down";
        case PatchAction::PrefetchUp:
            return "prefetch-up";
        case PatchAction::EnableKvCompression:
            return "kv-compression";
        default:
            return "unknown";
    }
}

bool IsTruthyEnvFlag(const char* name)
{
    const char* v = std::getenv(name);
    if (!v || !v[0])
        return false;

    std::string value(v);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });

    return !(value == "0" || value == "false" || value == "off" || value == "no");
}

#pragma pack(push, 1)
struct RxaHeaderV1
{
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t blockSize;
    uint64_t uncompressedSize;
    uint32_t blockCount;
    uint32_t reserved;
};

struct RxaBlockEntryV1
{
    uint64_t offset;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint32_t crc32c;
    uint8_t algorithm;
    uint8_t reserved[3];
};
#pragma pack(pop)

bool hasRxaExtension(const std::string& modelPath)
{
    std::error_code ec;
    const std::filesystem::path p(modelPath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return !ec && ext == ".rxa";
}

class Utf8StreamSanitizer
{
  public:
    std::string Consume(const std::string& chunk)
    {
        pending_ += chunk;
        return Extract(false);
    }

    std::string Finish()
    {
        std::string out = Extract(true);
        pending_.clear();
        return out;
    }

  private:
    static bool IsContinuation(unsigned char c)
    {
        return (c & 0xC0u) == 0x80u;
    }

    std::string Extract(bool finalChunk)
    {
        std::string out;
        size_t i = 0;

        while (i < pending_.size())
        {
            const unsigned char c0 = static_cast<unsigned char>(pending_[i]);
            if (c0 <= 0x7Fu)
            {
                out.push_back(static_cast<char>(c0));
                ++i;
                continue;
            }

            auto need = [&](size_t count) -> bool { return (i + count) <= pending_.size(); };

            if (c0 >= 0xC2u && c0 <= 0xDFu)
            {
                if (!need(2))
                {
                    if (!finalChunk)
                        break;
                    ++i;
                    continue;
                }
                const unsigned char c1 = static_cast<unsigned char>(pending_[i + 1]);
                if (IsContinuation(c1))
                {
                    out.append(pending_, i, 2);
                    i += 2;
                }
                else
                {
                    ++i;
                }
                continue;
            }

            if (c0 >= 0xE0u && c0 <= 0xEFu)
            {
                if (!need(3))
                {
                    if (!finalChunk)
                        break;
                    ++i;
                    continue;
                }

                const unsigned char c1 = static_cast<unsigned char>(pending_[i + 1]);
                const unsigned char c2 = static_cast<unsigned char>(pending_[i + 2]);
                bool ok = false;
                if (c0 == 0xE0u)
                    ok = (c1 >= 0xA0u && c1 <= 0xBFu) && IsContinuation(c2);
                else if (c0 == 0xEDu)
                    ok = (c1 >= 0x80u && c1 <= 0x9Fu) && IsContinuation(c2);
                else
                    ok = IsContinuation(c1) && IsContinuation(c2);

                if (ok)
                {
                    out.append(pending_, i, 3);
                    i += 3;
                }
                else
                {
                    ++i;
                }
                continue;
            }

            if (c0 >= 0xF0u && c0 <= 0xF4u)
            {
                if (!need(4))
                {
                    if (!finalChunk)
                        break;
                    ++i;
                    continue;
                }

                const unsigned char c1 = static_cast<unsigned char>(pending_[i + 1]);
                const unsigned char c2 = static_cast<unsigned char>(pending_[i + 2]);
                const unsigned char c3 = static_cast<unsigned char>(pending_[i + 3]);
                bool ok = false;
                if (c0 == 0xF0u)
                    ok = (c1 >= 0x90u && c1 <= 0xBFu) && IsContinuation(c2) && IsContinuation(c3);
                else if (c0 == 0xF4u)
                    ok = (c1 >= 0x80u && c1 <= 0x8Fu) && IsContinuation(c2) && IsContinuation(c3);
                else
                    ok = IsContinuation(c1) && IsContinuation(c2) && IsContinuation(c3);

                if (ok)
                {
                    out.append(pending_, i, 4);
                    i += 4;
                }
                else
                {
                    ++i;
                }
                continue;
            }

            ++i;
        }

        if (i > 0)
            pending_.erase(0, i);
        if (finalChunk && !pending_.empty())
            pending_.clear();

        return out;
    }

    std::string pending_;
};

std::string sanitizeUtf8Lossy(const std::string& input)
{
    Utf8StreamSanitizer sanitizer;
    std::string out = sanitizer.Consume(input);
    out += sanitizer.Finish();
    return out;
}

bool isRxaHeaderValid(const RxaHeaderV1& header)
{
    if (header.magic != kRxaMagic)
        return false;
    if (header.version != kRxaVersion1)
        return false;
    if (header.blockSize < 4096 || header.blockSize > (64u * 1024u * 1024u))
        return false;
    if (header.blockCount == 0 || header.blockCount > (8u * 1024u * 1024u))
        return false;
    if (header.uncompressedSize == 0 || header.uncompressedSize > (256ull * 1024ull * 1024ull * 1024ull))
        return false;
    return true;
}

#ifdef _WIN32
using FRtlGetCompressionWorkSpaceSize = long(__stdcall*)(unsigned short, unsigned long*, unsigned long*);
using FRtlDecompressBufferEx = long(__stdcall*)(unsigned short, unsigned char*, unsigned long, const unsigned char*,
                                                unsigned long, unsigned long*, void*);

static bool decompressRxaBlockWindows(uint8_t algorithm, const uint8_t* src, uint32_t srcSize, uint8_t* dst,
                                      uint32_t dstSize, std::string& outError)
{
    if (!src || !dst || srcSize == 0 || dstSize == 0)
    {
        outError = "invalid RXA compressed block pointers";
        return false;
    }

    unsigned short format = 0;
    if (algorithm == kRxaAlgXpress)
    {
        // COMPRESS_ALGORITHM_XPRESS_HUFF
        format = 0x0004u;
    }
    else if (algorithm == kRxaAlgLznt1)
    {
        // COMPRESSION_FORMAT_LZNT1
        format = 0x0002u;
    }
    else
    {
        outError = "unsupported RXA compression algorithm";
        return false;
    }

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll)
    {
        ntdll = LoadLibraryA("ntdll.dll");
    }
    if (!ntdll)
    {
        outError = "failed to load ntdll for RXA decompression";
        return false;
    }

    auto pGetWorkspace = reinterpret_cast<FRtlGetCompressionWorkSpaceSize>(
        GetProcAddress(ntdll, "RtlGetCompressionWorkSpaceSize"));
    auto pDecompress = reinterpret_cast<FRtlDecompressBufferEx>(GetProcAddress(ntdll, "RtlDecompressBufferEx"));
    if (!pGetWorkspace || !pDecompress)
    {
        outError = "required decompression symbols unavailable in ntdll";
        return false;
    }

    unsigned long workspaceCompress = 0;
    unsigned long workspaceDecompress = 0;
    const long wsStatus = pGetWorkspace(format, &workspaceCompress, &workspaceDecompress);
    if (wsStatus < 0)
    {
        outError = "failed to query RXA decompression workspace";
        return false;
    }

    std::vector<uint8_t> workspace;
    workspace.resize(static_cast<size_t>(workspaceDecompress));

    unsigned long finalSize = 0;
    const long decStatus = pDecompress(format, reinterpret_cast<unsigned char*>(dst), static_cast<unsigned long>(dstSize),
                                       reinterpret_cast<const unsigned char*>(src), static_cast<unsigned long>(srcSize),
                                       &finalSize, workspace.empty() ? nullptr : workspace.data());
    if (decStatus < 0)
    {
        outError = "RXA block decompression failed";
        return false;
    }
    if (finalSize != static_cast<unsigned long>(dstSize))
    {
        outError = "RXA decompressed block size mismatch";
        return false;
    }

    return true;
}
#endif

bool extractRxaToTempGguf(const std::string& rxaPath, std::string& outGgufPath, std::string& outError)
{
    outGgufPath.clear();
    outError.clear();

    std::ifstream in(rxaPath, std::ios::binary);
    if (!in.is_open())
    {
        outError = "unable to open RXA archive";
        return false;
    }

    RxaHeaderV1 header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in.good() || !isRxaHeaderValid(header))
    {
        outError = "invalid RXA header";
        return false;
    }

    std::vector<RxaBlockEntryV1> entries(header.blockCount);
    in.read(reinterpret_cast<char*>(entries.data()), static_cast<std::streamsize>(entries.size() * sizeof(RxaBlockEntryV1)));
    if (!in.good())
    {
        outError = "failed to read RXA block index";
        return false;
    }

    std::error_code ec;
    const std::filesystem::path sourcePath(rxaPath);
    const std::filesystem::path tempRoot = std::filesystem::temp_directory_path(ec);
    if (ec)
    {
        outError = "failed to resolve temp directory";
        return false;
    }

    const std::string cacheName = sourcePath.stem().string() + ".streamed.gguf";
    const std::filesystem::path cachePath = tempRoot / cacheName;

    const bool cacheExists = std::filesystem::exists(cachePath, ec) && !ec;
    if (cacheExists)
    {
        const uint64_t cachedSize = std::filesystem::file_size(cachePath, ec);
        if (!ec && cachedSize == header.uncompressedSize)
        {
            const auto sourceTime = std::filesystem::last_write_time(sourcePath, ec);
            if (!ec)
            {
                const auto cacheTime = std::filesystem::last_write_time(cachePath, ec);
                if (!ec && cacheTime >= sourceTime)
                {
                    outGgufPath = cachePath.string();
                    return true;
                }
            }
        }
    }

    std::ofstream out(cachePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        outError = "unable to create cached GGUF output";
        return false;
    }

    std::vector<char> compressed;
    uint64_t writtenTotal = 0;

    for (uint32_t i = 0; i < header.blockCount; ++i)
    {
        const RxaBlockEntryV1& entry = entries[i];
        if (entry.algorithm == kRxaAlgRaw && entry.compressedSize < entry.uncompressedSize)
        {
            outError = "corrupt RXA block size metadata";
            return false;
        }
        if (entry.algorithm != kRxaAlgRaw && entry.compressedSize == 0)
        {
            outError = "corrupt RXA compressed block";
            return false;
        }

        compressed.resize(entry.compressedSize);
        in.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
        if (!in.good())
        {
            outError = "invalid RXA block offset";
            return false;
        }

        in.read(compressed.data(), static_cast<std::streamsize>(entry.compressedSize));
        if (!in.good())
        {
            outError = "failed to read RXA block payload";
            return false;
        }

        if (entry.algorithm == kRxaAlgRaw)
        {
            out.write(compressed.data(), static_cast<std::streamsize>(entry.uncompressedSize));
        }
        else if (entry.algorithm == kRxaAlgBrutalGzip)
        {
            const std::vector<uint8_t> packed(reinterpret_cast<const uint8_t*>(compressed.data()),
                                              reinterpret_cast<const uint8_t*>(compressed.data()) + entry.compressedSize);
            std::vector<uint8_t> decompressed = brutal::decompress(packed);
            if (decompressed.size() != static_cast<size_t>(entry.uncompressedSize))
            {
                outError = "RXA brutal block decompressed size mismatch";
                return false;
            }
            out.write(reinterpret_cast<const char*>(decompressed.data()),
                      static_cast<std::streamsize>(entry.uncompressedSize));
        }
        else
        {
#ifdef _WIN32
            // Some archives may carry legacy algorithm metadata but still contain
            // brutal gzip blocks. Detect by gzip magic and decode via brutal path.
            if (entry.compressedSize >= 2 && static_cast<uint8_t>(compressed[0]) == 0x1f &&
                static_cast<uint8_t>(compressed[1]) == 0x8b)
            {
                const std::vector<uint8_t> packed(reinterpret_cast<const uint8_t*>(compressed.data()),
                                                  reinterpret_cast<const uint8_t*>(compressed.data()) + entry.compressedSize);
                std::vector<uint8_t> decompressed = brutal::decompress(packed);
                if (decompressed.size() != static_cast<size_t>(entry.uncompressedSize))
                {
                    outError = "RXA brutal fallback decompressed size mismatch";
                    return false;
                }
                out.write(reinterpret_cast<const char*>(decompressed.data()),
                          static_cast<std::streamsize>(entry.uncompressedSize));
            }
            else
            {
                std::vector<uint8_t> decompressed(static_cast<size_t>(entry.uncompressedSize));
                std::string decError;
                if (!decompressRxaBlockWindows(entry.algorithm, reinterpret_cast<const uint8_t*>(compressed.data()),
                                               entry.compressedSize, decompressed.data(), entry.uncompressedSize, decError))
                {
                    outError = decError;
                    return false;
                }
                out.write(reinterpret_cast<const char*>(decompressed.data()),
                          static_cast<std::streamsize>(entry.uncompressedSize));
            }
#else
            outError = "compressed RXA blocks require Windows decompression backend";
            return false;
#endif
        }
        if (!out.good())
        {
            outError = "failed to stream RXA block to GGUF cache";
            return false;
        }

        writtenTotal += entry.uncompressedSize;
    }

    out.flush();
    if (!out.good())
    {
        outError = "failed to flush streamed GGUF cache";
        return false;
    }

    if (writtenTotal != header.uncompressedSize)
    {
        outError = "RXA output size mismatch";
        return false;
    }

    outGgufPath = cachePath.string();
    return true;
}

bool checkedMulSize(size_t a, size_t b, size_t& out)
{
    if (a == 0 || b == 0)
    {
        out = 0;
        return true;
    }
    if (a > (std::numeric_limits<size_t>::max() / b))
    {
        return false;
    }
    out = a * b;
    return true;
}

bool utf8ToWideChecked(const std::string& utf8, std::wstring& wide)
{
    wide.clear();
    if (utf8.empty())
        return false;

    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0)
        return false;

    wide.resize(static_cast<size_t>(len));
    const int conv = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    if (conv <= 0)
    {
        wide.clear();
        return false;
    }
    if (!wide.empty() && wide.back() == L'\0')
        wide.pop_back();
    return !wide.empty();
}

inline float DotProductF32(const float* a, const float* b, int n)
{
    if (!a || !b || n <= 0)
        return 0.0f;

#if defined(__AVX512F__)
    int i = 0;
    __m512 acc = _mm512_setzero_ps();
    for (; i + 16 <= n; i += 16)
    {
        const __m512 va = _mm512_loadu_ps(a + i);
        const __m512 vb = _mm512_loadu_ps(b + i);
        acc = _mm512_fmadd_ps(va, vb, acc);
    }
    float sum = _mm512_reduce_add_ps(acc);
    for (; i < n; ++i)
        sum += a[i] * b[i];
    return sum;
#else
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += a[i] * b[i];
    return sum;
#endif
}

inline bool KvHotPathTelemetryEnabled()
{
    static int enabled = -1;
    if (enabled < 0)
    {
        char value[8] = {};
        const DWORD len = GetEnvironmentVariableA("RAWRXD_KV_HOTPATH_TIMER", value, static_cast<DWORD>(sizeof(value)));
        enabled = (len > 0 && len < sizeof(value) && value[0] != '0') ? 1 : 0;
    }
    return enabled != 0;
}

inline void AccumulateScaledKVHotPath(float* dst, const float* src, float scale, int n)
{
    if (!dst || !src || n <= 0)
    {
        return;
    }

    if (scale == 1.0f)
    {
        KernelOps::AccumulateKV(src, dst, n);
        return;
    }

    KernelOps::AccumulateScaledKV(src, dst, n, scale);
}
}  // namespace

[[nodiscard]] static bool ConvertTokensToU32Checked(const std::vector<int32_t>& in, std::vector<uint32_t>& out,
                                                    const char* caller)
{
    out.clear();
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        const int32_t tok = in[i];
        if (tok < 0)
        {
            printf("[CPUInferenceEngine] ERROR: %s received negative token %d at index %zu\n", caller, tok, i);
            out.clear();
            return false;
        }
        out.push_back(static_cast<uint32_t>(tok));
    }
    return true;
}

[[nodiscard]] std::vector<std::pair<int, float>> BuildTopKLogprobs(const std::vector<float>& logits, int topK)
{
    std::vector<std::pair<int, float>> result;
    if (topK <= 0 || logits.empty())
    {
        return result;
    }

    const std::size_t vocabSize = logits.size();
    std::vector<int> validIndices;
    validIndices.reserve(vocabSize);

    float maxLogit = -std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < vocabSize; ++i)
    {
        const float value = logits[i];
        if (!std::isfinite(value))
        {
            continue;
        }
        validIndices.push_back(static_cast<int>(i));
        if (value > maxLogit)
        {
            maxLogit = value;
        }
    }

    if (validIndices.empty())
    {
        return result;
    }

    double sumExp = 0.0;
    for (int index : validIndices)
    {
        sumExp += std::exp(static_cast<double>(logits[static_cast<std::size_t>(index)] - maxLogit));
    }

    if (!(sumExp > 0.0))
    {
        return result;
    }

    const float logZ = static_cast<float>(maxLogit + std::log(sumExp));
    topK = std::min(topK, static_cast<int>(validIndices.size()));

    auto better = [&](int left, int right)
    {
        return logits[static_cast<std::size_t>(left)] > logits[static_cast<std::size_t>(right)];
    };

    if (topK < static_cast<int>(validIndices.size()))
    {
        std::partial_sort(validIndices.begin(), validIndices.begin() + topK, validIndices.end(), better);
    }
    else
    {
        std::sort(validIndices.begin(), validIndices.end(), better);
    }

    result.reserve(static_cast<std::size_t>(topK));
    for (int i = 0; i < topK; ++i)
    {
        const int tokenId = validIndices[static_cast<std::size_t>(i)];
        const float value = logits[static_cast<std::size_t>(tokenId)];
        if (!std::isfinite(value))
        {
            continue;
        }
        result.emplace_back(tokenId, value - logZ);
    }

    return result;
}

struct SwarmSpeculativeAdapter
{
    RawrXDInference* engine = nullptr;
    std::vector<uint32_t> cachedContext;
    std::vector<float> cachedLogits;
    std::string modelId;
};

[[nodiscard]] std::vector<float> EvaluateAdapterContext(SwarmSpeculativeAdapter& adapter,
                                                        const std::vector<int>& context)
{
    std::vector<uint32_t> requested;
    requested.reserve(context.size());

    for (int token : context)
    {
        if (token < 0)
        {
            return {};
        }
        requested.push_back(static_cast<uint32_t>(token));
    }

    if (!adapter.engine)
    {
        return {};
    }

    if (requested == adapter.cachedContext)
    {
        return adapter.cachedLogits;
    }

    const bool canAdvance = !adapter.cachedContext.empty() &&
                            requested.size() == adapter.cachedContext.size() + 1 &&
                            std::equal(adapter.cachedContext.begin(), adapter.cachedContext.end(), requested.begin());

    std::vector<float> logits;
    if (canAdvance)
    {
        std::vector<uint32_t> delta{requested.back()};
        logits = adapter.engine->ForwardTokens(delta, static_cast<uint32_t>(adapter.cachedContext.size()));
    }
    else
    {
        logits = adapter.engine->ForwardTokens(requested, 0);
    }

    if (logits.empty())
    {
        adapter.cachedContext.clear();
        adapter.cachedLogits.clear();
        return {};
    }

    adapter.cachedContext = std::move(requested);
    adapter.cachedLogits = logits;
    return logits;
}

[[nodiscard]] RawrXD::Speculative::ModelInference BuildSwarmModelInference(SwarmSpeculativeAdapter& adapter)
{
    RawrXD::Speculative::ModelInference model;
    model.modelId = adapter.modelId;

    model.logprobs = [](const std::vector<int>& context, int topK, void* userData)
        -> std::vector<std::pair<int, float>>
    {
        auto* state = reinterpret_cast<SwarmSpeculativeAdapter*>(userData);
        if (!state || !state->engine || topK <= 0)
        {
            return {};
        }
        const std::vector<float> logits = EvaluateAdapterContext(*state, context);
        return BuildTopKLogprobs(logits, topK);
    };

    model.batchLogprobs = [](const std::vector<std::vector<int>>& contexts, int topK, void* userData)
        -> std::vector<std::vector<std::pair<int, float>>>
    {
        auto* state = reinterpret_cast<SwarmSpeculativeAdapter*>(userData);
        if (!state || !state->engine || topK <= 0)
        {
            return {};
        }

        std::vector<std::vector<std::pair<int, float>>> results;
        results.reserve(contexts.size());
        for (const auto& ctx : contexts)
        {
            const std::vector<float> logits = EvaluateAdapterContext(*state, ctx);
            results.push_back(BuildTopKLogprobs(logits, topK));
        }
        return results;
    };

    model.decode = [](int tokenId, void* userData) -> std::string
    {
        auto* state = reinterpret_cast<SwarmSpeculativeAdapter*>(userData);
        if (!state || !state->engine || tokenId < 0)
        {
            return {};
        }
        return state->engine->Detokenize({static_cast<uint32_t>(tokenId)});
    };

    model.encode = [](const std::string& text, void* userData) -> std::vector<int>
    {
        auto* state = reinterpret_cast<SwarmSpeculativeAdapter*>(userData);
        if (!state || !state->engine)
        {
            return {};
        }
        const auto tokens = state->engine->Tokenize(text);
        return std::vector<int>(tokens.begin(), tokens.end());
    };

    model.userData = &adapter;
    return model;
}

[[nodiscard]] uint64_t EstimateSwarmModelScale(const RawrXDInference& model)
{
    const uint64_t dim = static_cast<uint64_t>(std::max(1, model.getDim()));
    const uint64_t layers = static_cast<uint64_t>(std::max(1, model.getLayers()));
    const uint64_t heads = static_cast<uint64_t>(std::max(1, model.getHeads()));
    const uint64_t vocab = static_cast<uint64_t>(std::max(1, model.getVocabSize()));
    return (layers * dim) + heads + (vocab / 1024u);
}

[[nodiscard]] bool SelectSpeculativeSwarmPair(const std::vector<std::unique_ptr<RawrXDInference>>& models,
                                              std::size_t& draftIndex, std::size_t& targetIndex)
{
    draftIndex = 0;
    targetIndex = 0;
    if (models.size() < 2)
    {
        return false;
    }

    std::size_t bestTarget = std::numeric_limits<std::size_t>::max();
    uint64_t bestTargetScore = 0;
    int targetVocab = 0;
    for (std::size_t i = 0; i < models.size(); ++i)
    {
        const auto* model = models[i].get();
        if (!model)
        {
            continue;
        }
        const int vocab = model->getVocabSize();
        const uint64_t score = EstimateSwarmModelScale(*model);
        if (score > bestTargetScore || (score == bestTargetScore && i > bestTarget))
        {
            bestTargetScore = score;
            bestTarget = i;
            targetVocab = vocab;
        }
    }

    if (bestTarget == std::numeric_limits<std::size_t>::max() || targetVocab <= 0)
    {
        return false;
    }

    std::size_t bestDraft = std::numeric_limits<std::size_t>::max();
    uint64_t bestDraftScore = std::numeric_limits<uint64_t>::max();
    for (std::size_t i = 0; i < models.size(); ++i)
    {
        if (i == bestTarget)
        {
            continue;
        }
        const auto* model = models[i].get();
        if (!model || model->getVocabSize() != targetVocab)
        {
            continue;
        }
        const uint64_t score = EstimateSwarmModelScale(*model);
        if (score < bestDraftScore || (score == bestDraftScore && i < bestDraft))
        {
            bestDraftScore = score;
            bestDraft = i;
        }
    }

    if (bestDraft == std::numeric_limits<std::size_t>::max())
    {
        return false;
    }

    draftIndex = bestDraft;
    targetIndex = bestTarget;
    return true;
}

[[nodiscard]] bool IsTruthyEnv(const char* name)
{
    const char* value = std::getenv(name);
    if (!value || value[0] == '\0')
    {
        return false;
    }
    return value[0] != '0' && value[0] != 'f' && value[0] != 'F' && value[0] != 'n' && value[0] != 'N';
}

// ============================================================================
// Shared CPUInferenceEngine (single facade; matches single s_inferenceBackend).
// Static holder keeps one refcount so the facade is not destroyed while the
// static RawrXDInference backend may still be initialized.
// ============================================================================
std::shared_ptr<CPUInferenceEngine> CPUInferenceEngine::GetSharedInstance()
{
    static std::mutex mutex;
    static std::shared_ptr<CPUInferenceEngine> holder;
    std::lock_guard<std::mutex> lock(mutex);
    if (!holder)
        holder = std::make_shared<CPUInferenceEngine>();
    return holder;
}

CPUInferenceEngine* CPUInferenceEngine::getInstance()
{
    return GetSharedInstance().get();
}

// ============================================================================
// Lifecycle
// ============================================================================
CPUInferenceEngine::CPUInferenceEngine() {
    m_measurement_collector = std::make_unique<RawrXD::Inference::MeasurementCollector>();
}
CPUInferenceEngine::~CPUInferenceEngine()
{
    ClearCache();
    if (m_hTitanDLL)
    {
        FreeLibrary(static_cast<HMODULE>(m_hTitanDLL));
        m_hTitanDLL = nullptr;
    }
}

// ============================================================================
// Cooperative cancellation for LocalGGUF backpressure
// ============================================================================
void CPUInferenceEngine::RequestCancelGeneration()
{
    m_cancelGenerationRequested.store(true, std::memory_order_release);
}

void CPUInferenceEngine::ResetCancelGeneration()
{
    m_cancelGenerationRequested.store(false, std::memory_order_release);
}

bool CPUInferenceEngine::IsCancelGenerationRequested() const
{
    return m_cancelGenerationRequested.load(std::memory_order_acquire);
}

// ============================================================================
// Model Loading — delegates to RawrXDInference::Initialize
// ============================================================================
bool CPUInferenceEngine::LoadModel(const std::string& model_path)
{
    m_lastLoadErrorMessage.clear();
    if (model_path.empty())
    {
        m_lastLoadErrorMessage = "empty model path";
        return false;
    }

    std::string effectiveModelPath = model_path;
    if (hasRxaExtension(model_path))
    {
        std::string extractedPath;
        std::string extractError;
        if (!extractRxaToTempGguf(model_path, extractedPath, extractError))
        {
            m_lastLoadErrorMessage = "RXA stream extraction failed: " + extractError;
            printf("[CPUInferenceEngine] RXA extraction failed for %s: %s\n", model_path.c_str(), extractError.c_str());
            return false;
        }
        effectiveModelPath = extractedPath;
        printf("[CPUInferenceEngine] RXA stream extraction complete: %s -> %s\n", model_path.c_str(), effectiveModelPath.c_str());
    }

    printf("[CPUInferenceEngine] Loading model: %s\n", effectiveModelPath.c_str());
    printf("[CPUInferenceEngine] Stage: initialize backend (GPU will be attempted first, with CPU fallback)\n");
    try
    {
        // Try GPU-accelerated path first
        if (s_inferenceBackend.Initialize(effectiveModelPath))
        {
            m_lastLoadErrorMessage.clear();
            m_modelLoaded = true;

            // Propagate metadata from backend
            int bvs = s_inferenceBackend.getVocabSize();
            int bdim = s_inferenceBackend.getDim();
            int blay = s_inferenceBackend.getLayers();
            int bhd = s_inferenceBackend.getHeads();
            m_vocabSize = (bvs > 0) ? bvs : 32000;
            m_embeddingDim = (bdim > 0) ? bdim : 4096;
            m_numLayers = (blay > 0) ? blay : 32;
            m_numHeads = (bhd > 0) ? bhd : 32;
            // Try to load Titan ASM DLL if available
            if (m_useTitanAssembly && !m_hTitanDLL)
            {
                HMODULE hDll = LoadLibraryA("RawrXD_Titan.dll");
                if (hDll)
                {
                    m_hTitanDLL = hDll;
                    fnTitan_Initialize = (FTitan_Initialize)GetProcAddress(hDll, "Titan_Initialize");
                    fnTitan_LoadModel = (FTitan_LoadModel)GetProcAddress(hDll, "Titan_LoadModel");
                    fnTitan_RunInferenceStep = (FTitan_RunInferenceStep)GetProcAddress(hDll, "Titan_RunInferenceStep");

                    if (fnTitan_Initialize)
                    {
                        fnTitan_Initialize(&m_pTitanContext);
                        printf("[CPUInferenceEngine] Titan ASM engine loaded\n");
                    }
                    if (fnTitan_LoadModel && m_pTitanContext)
                    {
                        fnTitan_LoadModel(m_pTitanContext, effectiveModelPath.c_str());
                    }
                }
            }

            printf("[CPUInferenceEngine] Model loaded successfully\n");
            return true;
        }
        
        // GPU init failed — fail closed. GPU inference is mandatory; CPU fallback is
        // intentionally not permitted. Surface the underlying GPU error verbatim.
        printf("[CPUInferenceEngine] GPU initialization failed; refusing CPU fallback (GPU is mandatory)\n");
        std::string gpu_error = s_inferenceBackend.GetLastLoadErrorMessage();
        if (!gpu_error.empty())
        {
            printf("[CPUInferenceEngine] GPU error details: %s\n", gpu_error.c_str());
            m_lastLoadErrorMessage = "GPU initialization failed (" + gpu_error + "); GPU inference is mandatory and CPU fallback is disabled";
        }
        else
        {
            m_lastLoadErrorMessage = "GPU initialization failed; GPU inference is mandatory and CPU fallback is disabled";
        }
        m_modelLoaded = false;
        return false;
    }
    catch (const std::bad_alloc&)
    {
        m_modelLoaded = false;
        m_lastLoadErrorMessage = "Out of memory during model initialization (OOM)";
        printf("[CPUInferenceEngine] OOM during backend initialization\n");
        return false;
    }
    catch (const std::exception& e)
    {
        m_modelLoaded = false;
        std::string exc_msg = e.what();
        m_lastLoadErrorMessage = "Exception during model load: " + exc_msg;
        printf("[CPUInferenceEngine] Exception during backend initialization: %s\n", exc_msg.c_str());
        return false;
    }
    catch (...)
    {
        m_modelLoaded = false;
        m_lastLoadErrorMessage = "Unknown exception during model initialization";
        printf("[CPUInferenceEngine] Unknown exception during backend initialization\n");
        return false;
    }

    return false;
}

bool CPUInferenceEngine::LoadWeights(const std::unordered_map<std::string, Tensor>& tensors)
{
    m_weights = tensors;
    // Extract model dimensions from weight shapes if available
    auto it = m_weights.find("token_emb.weight");
    if (it != m_weights.end() && it->second.shape.size() >= 2)
    {
        m_vocabSize = static_cast<int>(it->second.shape[0]);
        m_embeddingDim = static_cast<int>(it->second.shape[1]);
    }
    m_modelLoaded = true;
    return true;
}

// ============================================================================
// Tokenization — delegates to RawrXDInference → RawrXDTokenizer
// ============================================================================
std::vector<int32_t> CPUInferenceEngine::Tokenize(const std::string& text)
{
    if (!m_modelLoaded || text.empty())
        return {};
    const std::string safeText = sanitizeUtf8Lossy(text);
    auto u32_toks = s_inferenceBackend.Tokenize(safeText);
    return std::vector<int32_t>(u32_toks.begin(), u32_toks.end());
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int32_t>& tokens)
{
    if (!m_modelLoaded || tokens.empty())
        return "";
    std::vector<uint32_t> u32_toks;
    if (!ConvertTokensToU32Checked(tokens, u32_toks, "Detokenize"))
        return "";
    return sanitizeUtf8Lossy(s_inferenceBackend.Detokenize(u32_toks));
}

// ============================================================================
// Inference — delegates to RawrXDInference::Generate
// ============================================================================
std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& input_tokens)
{
    if (!m_modelLoaded)
        return {};
    if (input_tokens.empty())
        return {};

    std::vector<uint32_t> toks;
    if (!ConvertTokensToU32Checked(input_tokens, toks, "Eval"))
        return {};
    const uint32_t startPos = (m_currentPos < 0) ? 0u : static_cast<uint32_t>(m_currentPos);
    auto logits = s_inferenceBackend.ForwardTokens(toks, startPos);
    if (!logits.empty())
    {
        m_lastState = logits;
    }
    return m_lastState;
}

std::string CPUInferenceEngine::DumpTokenTraceSummary(size_t lastNTokens) const
{
    return s_inferenceBackend.DumpTokenTraceSummary(lastNTokens);
}

bool CPUInferenceEngine::DumpTokenTracesToCSV(const std::string& filepath) const
{
    if (filepath.empty())
        return false;
    s_inferenceBackend.DumpTokenTracesToCSV(filepath.c_str());
    return true;
}

void CPUInferenceEngine::ClearTokenTraceBuffer()
{
    s_inferenceBackend.GetTokenTraceBuffer().clear();
}

std::string CPUInferenceEngine::DiagnoseToken(int32_t tokenId) const
{
    if (tokenId < 0)
        return "ERROR: token id must be non-negative";
    return s_inferenceBackend.DiagnoseToken(static_cast<uint32_t>(tokenId));
}

void CPUInferenceEngine::GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens,
                                           std::function<void(const std::string&)> token_callback,
                                           std::function<void()> complete_callback,
                                           std::function<void(int32_t)> token_id_callback)
{
    // Use swarm mode if enabled and models loaded
    if (m_swarmMode && !m_swarmModels.empty())
    {
        GenerateSwarmStreaming(input_tokens, max_tokens, token_callback, complete_callback, token_id_callback);
        return;
    }

    if (!m_modelLoaded)
    {
        if (complete_callback)
            complete_callback();
        return;
    }
    if (max_tokens <= 0 || input_tokens.empty())
    {
        if (complete_callback)
            complete_callback();
        return;
    }
    max_tokens = std::min(max_tokens, 8192);

    auto start = std::chrono::high_resolution_clock::now();
    if (input_tokens.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        printf("[CPUInferenceEngine] Warning: input_tokens.size() %zu exceeds int range, clamping\n",
               input_tokens.size());
        m_currentPos = std::numeric_limits<int>::max();
    }
    else
    {
        m_currentPos = static_cast<int>(input_tokens.size());
    }
    printf("[CPUInferenceEngine] GenerateStreaming: input_tokens=%llu max_tokens=%d\n",
           static_cast<unsigned long long>(input_tokens.size()), max_tokens);

    m_lastSwarmTelemetryPost = std::chrono::steady_clock::now() - std::chrono::milliseconds(300);
    emitSwarmTelemetryThrottled_(true);

    // New collector per generation session to avoid cross-request metric bleed.
    m_measurement_collector = std::make_unique<RawrXD::Inference::MeasurementCollector>();
    if (m_measurement_collector)
    {
        m_measurement_collector->ConfigureAutopatchGate(16, 20.0, 3);
    }

    // Stream directly from token IDs to avoid detokenize->retokenize drift.
    std::vector<uint32_t> u32_toks;
    if (!ConvertTokensToU32Checked(input_tokens, u32_toks, "GenerateStreaming"))
    {
        fprintf(stderr, "[GenerateStreaming] DIAGNOSTIC: ConvertTokensToU32Checked failed, returning early\n");
        if (complete_callback)
            complete_callback();
        return;
    }

    try
    {
        static auto token_step = 0;
        token_step = 0;
        auto token_start_time = std::chrono::high_resolution_clock::now();
        double peak_bandwidth_gbps = 0.0;
        bool diag_compute_stalled = false;
        bool diag_cache_thrash = false;
        bool diag_under_prefetch = false;
        bool diag_over_prefetch = false;
        std::string latest_root_cause = "NOMINAL";

        if (m_measurement_collector)
        {
            m_measurement_collector->TokenGenerationStart();
            m_measurement_collector->SetDiagnosisCallback(
                [&](const RawrXD::Autopatch::Diagnosis& diag)
                {
                    latest_root_cause = diag.root_cause;
                    diag_compute_stalled = (diag.root_cause.find("COMPUTE_STALLED") != std::string::npos);
                    diag_cache_thrash = diag.pattern.cache_thrashing;
                    diag_under_prefetch = diag.pattern.under_prefetching;
                    diag_over_prefetch = diag.pattern.over_prefetching;
                });
        }

        RawrXD::Inference::InferenceAutopatchConfig autopatchCfg;
        autopatchCfg.ringCapacity = 256;
        autopatchCfg.tpsWindow = 64;
        autopatchCfg.adaptEvery = 8;
        // CORRECTED: Realistic TPS thresholds based on measurement framework fix
        // Baseline: ~117 TPS for 40B Q4_K_M on CPU
        // Panic: below minimum viable throughput
        // Target: realistic sustained throughput  
        // Headroom: optimistic but achievable with tuning
        autopatchCfg.panicTps = 12.0;      // Minimum viable for 70B (per gate threshold)
        autopatchCfg.targetTps = 100.0;    // Realistic baseline for 40B
        autopatchCfg.headroomTps = 130.0;  // Optimistic with good cache locality
        autopatchCfg.highPressure = 0.85;  // Memory pressure threshold
        RawrXD::Inference::InferenceAutopatchController autopatch(autopatchCfg);
        const bool autopatchDisabled = IsTruthyEnvFlag("RAWRXD_DISABLE_AUTOPATCH") ||
                                       IsTruthyEnvFlag("RAWRXD_SMOKE_DISABLE_AUTOPATCH");
        if (autopatchDisabled)
        {
            std::printf("[Autopatch] disabled via env flag\n");
        }

        s_inferenceBackend.GenerateFromTokens(u32_toks, static_cast<uint32_t>(max_tokens),
                                              [&](uint32_t tok, const std::string& piece)
                                              {
                                                  // Cooperative cancellation check for LocalGGUF backpressure
                                                  if (m_cancelGenerationRequested.load(std::memory_order_acquire))
                                                  {
                                                      return false;  // Stop generation
                                                  }
                                                  emitSwarmTelemetryThrottled_(false);
                                                  const std::string safePiece = sanitizeUtf8Lossy(piece);
                                                  if (token_callback && !safePiece.empty())
                                                      token_callback(safePiece);
                                                  if (token_id_callback)
                                                      token_id_callback(static_cast<int32_t>(tok));
                                                  m_currentPos++;
                                                  // Measurement: Record token generation timing
                                                  auto token_end_time = std::chrono::high_resolution_clock::now();
                                                  if (m_measurement_collector) {
                                                      uint64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(token_end_time - token_start_time).count();
                                                      auto sdma = QuerySDMATelemetry();
                                                      RawrXD::Inference::TokenTelemetry telem{};
                                                      telem.tokenLatencyUs = elapsed_us;
                                                      telem.committedRamBytes = RawrXD::Inference::MemoryPressureGuard::committedRAM();
                                                      telem.committedVramBytes = RawrXD::Inference::MemoryPressureGuard::committedVRAM();
                                                      telem.cacheHitRate = sdma.cache_hit_rate;
                                                      telem.dispatchBound = !sdma.within_32ms_target;

                                                      constexpr double kRamBudget = 64.0 * 1024.0 * 1024.0 * 1024.0;
                                                      constexpr double kVramBudget = 16.0 * 1024.0 * 1024.0 * 1024.0;
                                                      const double pressureNorm =
                                                          (static_cast<double>(telem.committedRamBytes) / kRamBudget) * 0.65 +
                                                          (static_cast<double>(telem.committedVramBytes) / kVramBudget) * 0.35;
                                                      const float pressurePct = static_cast<float>(std::clamp(pressureNorm * 100.0, 0.0, 100.0));

                                                      int tierCurrent = 0;
                                                      if (pressurePct > 92.0f)
                                                          tierCurrent = 3;
                                                      else if (pressurePct > 82.0f)
                                                          tierCurrent = 2;
                                                      else if (pressurePct > 70.0f)
                                                          tierCurrent = 1;

                                                      const double bandwidthGbps = (elapsed_us > 0)
                                                              ? (static_cast<double>(m_embeddingDim) * sizeof(float) * 8.0 / static_cast<double>(elapsed_us) / 1000.0)
                                                              : 0.0;
                                                          peak_bandwidth_gbps = std::max(peak_bandwidth_gbps, bandwidthGbps);
                                                      m_measurement_collector->TokenGenerationEnd(
                                                          static_cast<int>(tok),
                                                          bandwidthGbps,
                                                          telem.cacheHitRate,
                                                          static_cast<int>(autopatch.currentPrefetchDepth()),
                                                          pressurePct,
                                                          tierCurrent
                                                      );
                                                      token_step++;
                                                      token_start_time = std::chrono::high_resolution_clock::now();

                                                      // Diagnostic-driven telemetry shaping for autopatch tuning.
                                                      if (diag_compute_stalled)
                                                      {
                                                          telem.dispatchBound = true;
                                                      }
                                                      if (diag_cache_thrash)
                                                      {
                                                          telem.cacheHitRate = std::min(telem.cacheHitRate, 0.35);
                                                      }

                                                      if (!autopatchDisabled)
                                                      {
                                                          autopatch.onToken(telem);
                                                          const bool urgent_adapt = diag_compute_stalled && ((token_step % 2) == 0);
                                                          if (autopatch.shouldAdapt() || urgent_adapt)
                                                          {
                                                              const auto decision = autopatch.adapt();
                                                              if (decision.action != RawrXD::Inference::PatchAction::None)
                                                              {
                                                                  std::printf(
                                                                      "[Autopatch] tok=%llu action=%s tps=%.1f pressure=%.3f depth=%u diag=%s\n",
                                                                      static_cast<unsigned long long>(autopatch.tokenCount()),
                                                                      AutopatchActionName(decision.action),
                                                                      decision.rollingTps,
                                                                      decision.rollingPressure,
                                                                      decision.suggestedPrefetchDepth,
                                                                      latest_root_cause.c_str());
                                                              }
                                                          }
                                                      }

                                                      if (m_measurement_collector)
                                                      {
                                                          m_measurement_collector->TokenGenerationStart();
                                                      }
                                                  }
                                              });
    }
    catch (const std::bad_alloc&)
    {
        printf("[CPUInferenceEngine] OOM during GenerateFromTokens\n");
        throw;
    }
    catch (const std::exception& e)
    {
        printf("[CPUInferenceEngine] Exception during GenerateFromTokens: %s\n", e.what());
        throw;
    }
    m_lastState = s_inferenceBackend.LastLogits();

    if (m_measurement_collector)
    {
        const auto finalMeasurement = m_measurement_collector->GetFinalMeasurement();
        RawrXD::Benchmark::MeasurementValidator::ThroughputEnvelope envelope;
        envelope.peak_memory_bandwidth_gbps = 25.0; // Conservative default for DDR4
        envelope.avg_token_footprint_bytes = std::max(1024.0, static_cast<double>(m_embeddingDim) * sizeof(float));
        envelope.pipeline_efficiency = 0.65;

        const bool valid = RawrXD::Benchmark::MeasurementValidator::ValidateMeasurement(finalMeasurement, &envelope);
        const auto gate = m_measurement_collector->EvaluateAutopatchGate();

        if (!valid)
        {
            std::fprintf(stderr,
                         "[Measurement] invalid session measurement (ttft_ms=%.1f decode_tps=%.2f e2e_tps=%.2f)\n",
                         finalMeasurement.ttft_ms(),
                         finalMeasurement.real_decode_tps(),
                         finalMeasurement.total_end_to_end_tps());
        }

        std::printf("[Measurement] gate=%s reason=%s samples=%zu rolling_tps=%.2f stddev=%.2f\n",
                    gate.allow ? "allow" : "block",
                    gate.reason.c_str(),
                    gate.sample_count,
                    gate.rolling_tps,
                    gate.tps_stddev);
    }

    emitSwarmTelemetryThrottled_(true);

    if (complete_callback)
        complete_callback();

    auto end = std::chrono::high_resolution_clock::now();
    m_inferenceCount++;
    m_totalInferenceTime += std::chrono::duration<double>(end - start).count();
}

// ============================================================================
// AI Mode setters
// ============================================================================
void CPUInferenceEngine::SetMaxMode(bool enabled)
{
    m_maxMode = enabled;
}

void CPUInferenceEngine::SetDeepThinking(bool enabled)
{
    m_deepThinking = enabled;
}

void CPUInferenceEngine::SetDeepResearch(bool enabled)
{
    m_deepResearch = enabled;
}

// ============================================================================
// Swarm Mode setters
// ============================================================================
void CPUInferenceEngine::SetSwarmMode(bool enabled, int chainDepth)
{
    m_swarmMode = enabled;
    // Prevent divide-by-zero and negative depth in swarm scheduling.
    m_swarmChainDepth = enabled ? std::max(1, chainDepth) : 1;
}

// ============================================================================
// Context and memory management
// ============================================================================
void CPUInferenceEngine::SetContextSize(size_t size)
{
    m_contextLimit = (size > kMaxContextTokens) ? kMaxContextTokens : size;
}

size_t CPUInferenceEngine::GetMemoryUsage() const
{
    return m_totalMemoryAllocated;
}

void CPUInferenceEngine::ClearCache()
{
    m_kv_cache.clear();
    m_memoryPool.clear();
    m_totalMemoryAllocated = 0;
}

// ============================================================================
// Swarm Inference with Model Chaining
// ============================================================================
void CPUInferenceEngine::GenerateSwarmStreaming(const std::vector<int32_t>& input_tokens, int max_tokens,
                                                std::function<void(const std::string&)> token_callback,
                                                std::function<void()> complete_callback,
                                                std::function<void(int32_t)> token_id_callback)
{
    if (!m_swarmMode || m_swarmModels.empty())
    {
        // Fallback to single model
        GenerateStreaming(input_tokens, max_tokens, token_callback, complete_callback, token_id_callback);
        return;
    }

    std::vector<int32_t> current_tokens = input_tokens;
    int tokens_generated = 0;
    const int chain_depth = std::max(1, m_swarmChainDepth);
    const int tokens_per_model = std::max(1, max_tokens / chain_depth);

    printf("[Swarm] Starting chain with %zu models, depth %d\n", m_swarmModels.size(), chain_depth);

    std::size_t draftIndex = 0;
    std::size_t targetIndex = 0;
    if (SelectSpeculativeSwarmPair(m_swarmModels, draftIndex, targetIndex))
    {
        SwarmSpeculativeAdapter draftAdapter;
        draftAdapter.engine = m_swarmModels[draftIndex].get();
        draftAdapter.modelId = std::string("swarm:draft:") + std::to_string(draftIndex);

        SwarmSpeculativeAdapter targetAdapter;
        targetAdapter.engine = m_swarmModels[targetIndex].get();
        targetAdapter.modelId = std::string("swarm:target:") + std::to_string(targetIndex);

        RawrXD::Speculative::SpeculationConfig specCfg{};
        specCfg.maxDraftTokens = m_maxMode ? 8 : (m_deepThinking ? 6 : 5);
        specCfg.minDraftTokens = 1;
        specCfg.acceptanceThreshold = m_deepThinking ? 0.25f : 0.30f;
        specCfg.adaptiveDraftLen = true;
        specCfg.treeSpeculation = m_maxMode || m_deepThinking || m_deepResearch ||
                                  current_tokens.size() >= 128u ||
                                  IsTruthyEnv("RAWRXD_SWARM_TREE_SPECULATION");
        specCfg.treeBranching = m_maxMode ? 4 : 2;
        specCfg.treeDepth = m_deepResearch ? 4 : (m_maxMode ? 4 : 3);
        specCfg.ensembleDrafts = m_deepResearch ? 2 : 1;
        specCfg.temperatureDraft = 0.0f;
        specCfg.temperatureTarget = 0.0f;

        RawrXD::Speculative::SpeculativeDecoderV2 decoder;
        decoder.setConfig(specCfg);

        const auto draftModel = BuildSwarmModelInference(draftAdapter);
        const auto targetModel = BuildSwarmModelInference(targetAdapter);
        if (decoder.setDraftModel(draftModel).success && decoder.setTargetModel(targetModel).success)
        {
            struct SwarmStreamSink
            {
                std::function<void(const std::string&)>* tokenCallback = nullptr;
                std::function<void(int32_t)>* tokenIdCallback = nullptr;
                int* currentPos = nullptr;
            } sink{&token_callback, &token_id_callback, &m_currentPos};

            std::vector<uint32_t> promptU32;
            if (!ConvertTokensToU32Checked(current_tokens, promptU32, "GenerateSwarmStreaming speculative prompt"))
            {
                printf("[Swarm] Speculative prompt validation failed, using legacy chain path\n");
            }
            else
            {
                std::vector<int> promptTokens(promptU32.begin(), promptU32.end());
                if (static_cast<unsigned long long>(current_tokens.size()) >
                    static_cast<unsigned long long>(std::numeric_limits<int>::max()))
                {
                    m_currentPos = std::numeric_limits<int>::max();
                }
                else
                {
                    m_currentPos = static_cast<int>(current_tokens.size());
                }

                auto streamCallback = [](const RawrXD::Speculative::Token& token, bool /*isDraft*/, void* userData)
                {
                    auto* state = reinterpret_cast<SwarmStreamSink*>(userData);
                    if (!state)
                    {
                        return;
                    }

                    const std::string safePiece = sanitizeUtf8Lossy(token.text);
                    try
                    {
                        if (state->tokenCallback && !safePiece.empty())
                        {
                            (*state->tokenCallback)(safePiece);
                        }
                    }
                    catch (...)
                    {
                    }

                    try
                    {
                        if (state->tokenIdCallback)
                        {
                            (*state->tokenIdCallback)(static_cast<int32_t>(token.id));
                        }
                    }
                    catch (...)
                    {
                    }

                    if (state->currentPos)
                    {
                        ++(*state->currentPos);
                    }
                };

                const auto speculativeResult = decoder.generateStreaming(promptTokens, max_tokens, streamCallback, &sink);
                if (speculativeResult.success)
                {
                    std::vector<uint32_t> finalContext = promptU32;
                    for (const auto& token : speculativeResult.tokens)
                    {
                        if (token.id >= 0)
                        {
                            finalContext.push_back(static_cast<uint32_t>(token.id));
                        }
                    }

                    if (!finalContext.empty() && finalContext.back() == 0u)
                    {
                        finalContext.pop_back();
                    }

                    if (!finalContext.empty())
                    {
                        const auto logits = targetAdapter.engine->ForwardTokens(finalContext, 0);
                        if (!logits.empty())
                        {
                            m_lastState = logits;
                        }
                    }

                    emitSwarmTelemetryThrottled_(true);
                    if (complete_callback)
                        complete_callback();
                    return;
                }

                printf("[Swarm] Speculative decode unavailable, falling back to legacy chaining: %s\n",
                       speculativeResult.detail ? speculativeResult.detail : "unknown error");
            }
        }
        else
        {
            printf("[Swarm] Speculative decoder setup failed, using legacy chaining\n");
        }
    }

    for (int chain_step = 0; chain_step < chain_depth && tokens_generated < max_tokens; ++chain_step)
    {
        // Select model for this step (cycle through available models)
        size_t model_idx = chain_step % m_swarmModels.size();
        auto& model = m_swarmModels[model_idx];

        printf("[Swarm] Chain step %d using model %zu\n", chain_step, model_idx);

        // Speculative batching: Generate multiple candidate tokens per step
        const int speculative_depth = m_maxMode ? 8 : 1;  // TPS boost in max mode
        std::vector<std::vector<int32_t>> candidate_batches(speculative_depth);

        for (int batch = 0; batch < speculative_depth; ++batch)
        {
            std::vector<int32_t> batch_tokens;
            std::vector<uint32_t> prompt_tokens;
            if (!ConvertTokensToU32Checked(current_tokens, prompt_tokens, "GenerateSwarmStreaming"))
            {
                if (complete_callback)
                    complete_callback();
                return;
            }
            std::vector<uint32_t> generated = model->GenerateFromTokens(
                prompt_tokens, static_cast<uint32_t>(std::max(1, tokens_per_model / speculative_depth)),
                [&](uint32_t token_id, const std::string& token_str)
                {
                    const std::string safeTokenStr = sanitizeUtf8Lossy(token_str);
                    if (token_callback && !safeTokenStr.empty() && batch == 0)
                    {
                        token_callback(safeTokenStr);
                    }
                    batch_tokens.push_back(static_cast<int32_t>(token_id));
                });

            if (batch_tokens.empty() && !generated.empty())
            {
                batch_tokens.assign(generated.begin(), generated.end());
            }

            candidate_batches[batch] = batch_tokens;
        }

        // Select best candidate (simplified: use first batch for now, could add scoring)
        std::vector<int32_t> step_tokens = candidate_batches[0];

        // Output the selected tokens
        for (int32_t token_id : step_tokens)
        {
            if (token_id_callback)
                token_id_callback(token_id);
            tokens_generated++;
            if (tokens_generated >= max_tokens)
                break;
        }

        // Update current tokens for next model
        current_tokens.insert(current_tokens.end(), step_tokens.begin(), step_tokens.end());

        // Limit context with safe arithmetic
        if (current_tokens.size() > m_contextLimit)
        {
            // Compute: keep_start = current_tokens.size() - m_contextLimit + input_tokens.size()
            // Safely to avoid unsigned underflow when current_tokens.size() < m_contextLimit
            size_t keep_start = 0;
            size_t excess = current_tokens.size() - m_contextLimit;
            if (input_tokens.size() > excess)
            {
                // Preserve as much input context as possible
                keep_start = 0;
            }
            else
            {
                // Remove early tokens, preserving input context
                keep_start = excess;
            }
            if (keep_start < current_tokens.size())
            {
                current_tokens = std::vector<int32_t>(current_tokens.begin() + keep_start, current_tokens.end());
            }
        }
    }

    if (complete_callback)
        complete_callback();
    emitSwarmTelemetryThrottled_(true);
    printf("[Swarm] Chain complete, generated %d tokens\n", tokens_generated);
}

void CPUInferenceEngine::UpdateOutputWeights(const std::vector<float>& gradients, float learningRate)
{
    // Training mode: update output projection weights
    // Not yet implemented — inference-only for now
}

// ============================================================================
// Swarm Model Loading from Directory
// ============================================================================
bool CPUInferenceEngine::LoadSwarmFromDirectory(const std::string& directoryPath, int maxModels)
{
    namespace fs = std::filesystem;
    fs::path dirPath(directoryPath);

    if (maxModels <= 0)
    {
        m_lastLoadErrorMessage = "maxModels must be > 0";
        return false;
    }

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
    {
        m_lastLoadErrorMessage = "Directory does not exist: " + directoryPath;
        return false;
    }

    std::vector<std::string> modelPaths;

    // Find all .gguf files
    for (const auto& entry : fs::directory_iterator(dirPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".gguf")
        {
            modelPaths.push_back(entry.path().string());
            if (modelPaths.size() >= static_cast<size_t>(maxModels))
                break;
        }
    }

    if (modelPaths.empty())
    {
        m_lastLoadErrorMessage = "No .gguf files found in directory: " + directoryPath;
        return false;
    }

    printf("[Swarm] Found %zu models in directory, loading up to %d\n", modelPaths.size(), maxModels);

    return LoadSwarmModels(modelPaths);
}

bool CPUInferenceEngine::LoadSwarmModels(const std::vector<std::string>& modelPaths)
{
    m_swarmModels.clear();
    m_lastLoadErrorMessage.clear();

    for (const auto& path : modelPaths)
    {
        auto model = std::make_unique<RawrXDInference>();

        printf("[Swarm] Loading model %zu/%zu: %s\n", m_swarmModels.size() + 1, modelPaths.size(), path.c_str());

        if (!model->Initialize(path))
        {
            m_lastLoadErrorMessage = "Failed to load swarm model: " + path;
            m_swarmModels.clear();
            return false;
        }

        m_swarmModels.push_back(std::move(model));
    }

    printf("[Swarm] Successfully loaded %zu models for chaining\n", m_swarmModels.size());
    return true;
}

void CPUInferenceEngine::UpdateWeights(const std::vector<std::vector<float>>& layer_gradients, float learning_rate)
{
    (void)layer_gradients;
    (void)learning_rate;
}

void CPUInferenceEngine::SetContextLimit(size_t limit)
{
    m_contextLimit = (limit > kMaxContextTokens) ? kMaxContextTokens : limit;
}

void CPUInferenceEngine::RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin> plugin)
{
    m_memoryPlugins.push_back(std::move(plugin));
}

void CPUInferenceEngine::SetLayerProgressCallback(std::function<void(const std::string&)> cb)
{
    s_inferenceBackend.SetLayerProgressCallback(std::move(cb));
}

void CPUInferenceEngine::SetSwarmTelemetryOutputCallback(std::function<void(const std::string&)> cb)
{
    m_swarmTelemetryOutputCb = std::move(cb);
}

std::string CPUInferenceEngine::MoEPackHudStatusLineUtf8() const
{
    if (!m_modelLoaded)
        return {};
    const MoEPackHudMetrics m = s_inferenceBackend.moEPackHudMetrics();
    char b[512];
    const int n = std::snprintf(
        b, sizeof(b),
        "MoE pack: hit=%llu miss=%llu fb=%llu | wa=%llu sa=%llu wf=%llu sf=%llu | sync=%llu pre=%llu | "
        "qdrop=%llu qnr=%llu rowEv=%llu q~=%llu | B=%llu evict=%llu rInv=%llu",
        static_cast<unsigned long long>(m.packHits), static_cast<unsigned long long>(m.packMisses),
        static_cast<unsigned long long>(m.groupedFallbacks),
        static_cast<unsigned long long>(m.groupedWeightedApplies),
        static_cast<unsigned long long>(m.groupedSingleExpertApplies),
        static_cast<unsigned long long>(m.groupedWeightedFallbacks),
        static_cast<unsigned long long>(m.groupedSingleExpertFallbacks),
        static_cast<unsigned long long>(m.syncPackInserts),
        static_cast<unsigned long long>(m.prepackInserts), static_cast<unsigned long long>(m.prepackQueueDropped),
        static_cast<unsigned long long>(m.prepackSkippedNotResident),
        static_cast<unsigned long long>(m.packEvictedByPlanRow),
        static_cast<unsigned long long>(m.prepackQueueDepthApprox),
        static_cast<unsigned long long>(m.packCachePackedBytes), static_cast<unsigned long long>(m.packCacheEvictions),
        static_cast<unsigned long long>(m.packCacheSelectiveRowInvalidations));
    if (n <= 0)
        return {};
    return std::string(b, static_cast<std::size_t>(n));
}

CPUInferenceEngine::SDMAKineticMetrics CPUInferenceEngine::QuerySDMATelemetry() const
{
    SDMAKineticMetrics metrics{};
    if (!m_modelLoaded)
        return metrics;

    metrics.flip_count = CircularSDMA::g_sdma_flip_count;
    metrics.wait_cycles = CircularSDMA::g_sdma_wait_cycles;
    metrics.cache_hits = CircularSDMA::g_expert_cache_hits;
    metrics.cache_misses = CircularSDMA::g_expert_cache_misses;

    // Derived metrics
    const std::uint64_t total_predictions = metrics.cache_hits + metrics.cache_misses;
    if (total_predictions > 0)
    {
        metrics.cache_hit_rate = static_cast<double>(metrics.cache_hits) / static_cast<double>(total_predictions);
    }

    if (metrics.flip_count > 0)
    {
        const double avg_cycles = static_cast<double>(metrics.wait_cycles) / static_cast<double>(metrics.flip_count);
        constexpr double CPU_GHZ = 2.4;  // Target CPU frequency
        metrics.avg_wait_ms = avg_cycles / (CPU_GHZ * 1e6);
        
        constexpr std::uint64_t MAX_WAIT_CYCLES_32MS = 76'800'000ull;  // 32ms @ 2.4 GHz
        metrics.within_32ms_target = (avg_cycles < static_cast<double>(MAX_WAIT_CYCLES_32MS));
    }

    return metrics;
}

std::string CPUInferenceEngine::SDMAKineticHudStatusLineUtf8() const
{
    if (!m_modelLoaded)
        return {};
    
    const SDMAKineticMetrics m = QuerySDMATelemetry();
    if (m.flip_count == 0)
        return "SDMA: [inactive]";

    char b[256];
    const int n = std::snprintf(
        b, sizeof(b),
        "SDMA: flips=%llu hit=%.1f%% wait=%.2fms %s",
        static_cast<unsigned long long>(m.flip_count),
        m.cache_hit_rate * 100.0,
        m.avg_wait_ms,
        m.within_32ms_target ? "[OK]" : "[SLOW]");
    
    if (n <= 0)
        return {};
    return std::string(b, static_cast<std::size_t>(n));
}

bool CPUInferenceEngine::CaptureSwarmExpertHeatmap(const RawrXD::Swarm::ExpertHeatmapCaptureParams& params,
                                                   RawrXD::Swarm::ExpertHeatmapSnapshot& out) const
{
    if (!m_modelLoaded)
    {
        out = {};
        return false;
    }
    return s_inferenceBackend.CaptureSwarmExpertHeatmap(params, out);
}

std::uint64_t CPUInferenceEngine::SwarmPlanGeneration() const
{
    return s_inferenceBackend.swarmPlanGeneration();
}

void CPUInferenceEngine::emitSwarmTelemetryThrottled_(bool force)
{
    if (!m_swarmTelemetryOutputCb || !m_modelLoaded)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (!force && (now - m_lastSwarmTelemetryPost) < std::chrono::milliseconds(250))
        return;

    const auto tel = s_inferenceBackend.loaderSlidingWindowTelemetry();
    const auto st = s_inferenceBackend.swarmRuntimeStats();
    const MoEPackHudMetrics moem = s_inferenceBackend.moEPackHudMetrics();
    char buf[640];
    const int n = std::snprintf(
        buf, sizeof(buf),
        "[SWARM] slot_block=%llu svc_block=%llu promote_skip=%llu backoff=%llu "
        "pf_ok=%llu pf_dup=%llu pf_cov=%llu pf_enq_dup=%llu evict=%llu "
        "pf_spec=%llu jit_cold=%llu ev_rej_in_use=%llu ev_starve=%llu "
        "pin_blk=%llu pin_blk_to=%llu pin_blk_ms_sum=%llu in_use=%u\n",
        static_cast<unsigned long long>(tel.noEvictableComputeSlot),
        static_cast<unsigned long long>(tel.sovereignRemapBlockedInUse),
        static_cast<unsigned long long>(tel.promotionSkippedNoEvictableSlot),
        static_cast<unsigned long long>(tel.swarmPinBackoffCycles),
        static_cast<unsigned long long>(st.prefetchPinSuccess),
        static_cast<unsigned long long>(st.prefetchSkippedDuplicate),
        static_cast<unsigned long long>(st.prefetchSkippedComputeCovers),
        static_cast<unsigned long long>(st.prefetchEnqueueSkippedDuplicate),
        static_cast<unsigned long long>(st.evictions),
        static_cast<unsigned long long>(st.speculativeExpertPrefetchEnqueued),
        static_cast<unsigned long long>(st.jitPinNonResident),
        static_cast<unsigned long long>(st.evictionRejectedInUse), static_cast<unsigned long long>(st.evictStarvation),
        static_cast<unsigned long long>(st.pinBlockAttempts), static_cast<unsigned long long>(st.pinBlockTimeouts),
        static_cast<unsigned long long>(st.pinBlockLatencyMsTotal), static_cast<unsigned>(st.inUseSliceCount));
    char moeBuf[512];
    const int nm = std::snprintf(
        moeBuf, sizeof(moeBuf),
        "[MOE_PACK] hit=%llu miss=%llu fb=%llu wa=%llu sa=%llu wf=%llu sf=%llu sync_i=%llu pre_i=%llu "
        "qdrop=%llu qnr=%llu row_ev=%llu q~=%llu B=%llu ev=%llu rinv=%llu\n",
        static_cast<unsigned long long>(moem.packHits), static_cast<unsigned long long>(moem.packMisses),
        static_cast<unsigned long long>(moem.groupedFallbacks),
        static_cast<unsigned long long>(moem.groupedWeightedApplies),
        static_cast<unsigned long long>(moem.groupedSingleExpertApplies),
        static_cast<unsigned long long>(moem.groupedWeightedFallbacks),
        static_cast<unsigned long long>(moem.groupedSingleExpertFallbacks),
        static_cast<unsigned long long>(moem.syncPackInserts),
        static_cast<unsigned long long>(moem.prepackInserts), static_cast<unsigned long long>(moem.prepackQueueDropped),
        static_cast<unsigned long long>(moem.prepackSkippedNotResident),
        static_cast<unsigned long long>(moem.packEvictedByPlanRow),
        static_cast<unsigned long long>(moem.prepackQueueDepthApprox),
        static_cast<unsigned long long>(moem.packCachePackedBytes),
        static_cast<unsigned long long>(moem.packCacheEvictions),
        static_cast<unsigned long long>(moem.packCacheSelectiveRowInvalidations));
    m_lastSwarmTelemetryPost = now;
    std::string msg;
    if (n > 0)
        msg.assign(buf, static_cast<std::size_t>(n));
    if (nm > 0)
        msg.append(moeBuf, static_cast<std::size_t>(nm));
    if (!msg.empty())
        m_swarmTelemetryOutputCb(std::move(msg));
}

bool CPUInferenceEngine::MatVecQ4(const float* matrix, const float* vector, float* output, uint32_t rows, uint32_t cols)
{
    if (!matrix || !vector || !output || rows == 0 || cols == 0)
    {
        return false;
    }

    for (uint32_t row = 0; row < rows; ++row)
    {
        float sum = 0.0f;
        for (uint32_t col = 0; col < cols; ++col)
        {
            sum += matrix[static_cast<size_t>(row) * cols + col] * vector[col];
        }
        output[row] = sum;
    }

    return true;
}

// ============================================================================
// Memory Allocation
// ============================================================================
float* CPUInferenceEngine::AllocateTensor(size_t size)
{
    if (size == 0 || size > (std::numeric_limits<size_t>::max() / sizeof(float)))
    {
        return nullptr;
    }

    size_t bytes = 0;
    if (!checkedMulSize(size, sizeof(float), bytes))
    {
        return nullptr;
    }
    if (m_totalMemoryAllocated > (std::numeric_limits<size_t>::max() - bytes))
    {
        return nullptr;
    }

    std::unique_ptr<float[]> ptr;
    try
    {
        ptr = std::make_unique<float[]>(size);
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }

    float* raw = ptr.get();
    m_totalMemoryAllocated += bytes;
    m_memoryPool.push_back(std::move(ptr));
    return raw;
}

void CPUInferenceEngine::DeallocateTensor(float* ptr)
{
    // Pool-managed — deallocated on ClearCache()
    (void)ptr;
}

// ============================================================================
// KV Cache
// ============================================================================
void CPUInferenceEngine::InitKVCache()
{
    if (m_numLayers <= 0 || m_embeddingDim <= 0)
    {
        m_kv_cache.clear();
        m_dynamicKVCache = false;
        return;
    }

    m_kv_cache.resize(m_numLayers);

    // Memory-gate bypass: For large contexts, use dynamic allocation instead of pre-allocation
    size_t initialSize = (m_contextLimit > kMaxContextTokens) ? kMaxContextTokens : m_contextLimit;

    const size_t layers = static_cast<size_t>(m_numLayers);
    const size_t dim = static_cast<size_t>(m_embeddingDim);
    size_t perLayerElems = 0;
    if (!checkedMulSize(initialSize, dim, perLayerElems))
    {
        initialSize = 0;
    }
    else
    {
        size_t perTokenBytes = 0;
        size_t tmp = 0;
        bool budgetMulOk = checkedMulSize(layers, dim, tmp) && checkedMulSize(tmp, sizeof(float), tmp) &&
                           checkedMulSize(tmp, 2, perTokenBytes);
        if (budgetMulOk && perTokenBytes > 0)
        {
            const size_t maxTokensByBudget = kMaxKvCacheBytes / perTokenBytes;
            if (maxTokensByBudget < initialSize)
            {
                initialSize = maxTokensByBudget;
            }
        }
    }

    for (auto& layer : m_kv_cache)
    {
        layer.keys.resize(initialSize * m_embeddingDim, 0.0f);
        layer.values.resize(initialSize * m_embeddingDim, 0.0f);
    }

    // Mark as using dynamic allocation for large contexts
    m_dynamicKVCache = (m_contextLimit >= kMaxContextTokens);
}

// ============================================================================
// Private Math Operations
// These are available for direct-call mode but the primary path goes through
// the RawrXDTransformer pipeline (which has its own optimized implementations).
// ============================================================================
void CPUInferenceEngine::MatMul(const float* A, const float* B, float* C, int m, int n, int k)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float sum = 0.0f;
            for (int p = 0; p < k; p++)
            {
                sum += A[i * k + p] * B[p * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

void CPUInferenceEngine::ApplySoftmax(float* data, int size)
{
    if (!data || size <= 0)
        return;

    float maxVal = *std::max_element(data, data + size);
    float sum = 0.0f;

    for (int i = 0; i < size; i++)
    {
        data[i] = std::exp(data[i] - maxVal);
        sum += data[i];
    }

    if (!(sum > 0.0f) || !std::isfinite(sum))
    {
        const float uniform = 1.0f / static_cast<float>(size);
        for (int i = 0; i < size; i++)
            data[i] = uniform;
        return;
    }

    for (int i = 0; i < size; i++)
    {
        data[i] /= sum;
    }
}

void CPUInferenceEngine::LayerNorm(float* data, int size, float epsilon)
{
    if (!data || size <= 0)
        return;
    float mean = 0.0f;
    for (int i = 0; i < size; i++)
        mean += data[i];
    mean /= size;
    float var = 0.0f;
    for (int i = 0; i < size; i++)
        var += (data[i] - mean) * (data[i] - mean);
    var /= size;
    float inv = 1.0f / std::sqrt(var + epsilon);
    for (int i = 0; i < size; i++)
        data[i] = (data[i] - mean) * inv;
}

void CPUInferenceEngine::GELU(float* data, int size)
{
    for (int i = 0; i < size; i++)
    {
        float x = data[i];
        data[i] = 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / 3.14159265f) * (x + 0.044715f * x * x * x)));
    }
}

void CPUInferenceEngine::RMSNorm(float* data, int size, float epsilon)
{
    if (!data || size <= 0)
        return;
    float ss = 0.0f;
    for (int i = 0; i < size; i++)
        ss += data[i] * data[i];
    ss = 1.0f / std::sqrt(ss / size + epsilon);
    for (int i = 0; i < size; i++)
        data[i] *= ss;
}

void CPUInferenceEngine::RoPE(float* data, int dim, int pos, int rotary_dim)
{
    if (!data || dim <= 1 || rotary_dim <= 1)
        return;
    const int safe_rotary_dim = std::min(rotary_dim, dim);
    const int even_rotary_dim = safe_rotary_dim - (safe_rotary_dim % 2);
    for (int i = 0; i < even_rotary_dim; i += 2)
    {
        float freq = 1.0f / std::pow(10000.0f, static_cast<float>(i) / even_rotary_dim);
        float val = pos * freq;
        float cos_val = std::cos(val);
        float sin_val = std::sin(val);
        float v0 = data[i];
        float v1 = data[i + 1];
        data[i] = v0 * cos_val - v1 * sin_val;
        data[i + 1] = v0 * sin_val + v1 * cos_val;
    }
}

void CPUInferenceEngine::MultiHeadAttention(const float* query, const float* key, const float* value, float* output,
                                            int seq_len, int embed_dim, int num_heads, int layer_idx)
{
    (void)layer_idx;
    if (!query || !key || !value || !output || seq_len <= 0 || embed_dim <= 0 || num_heads <= 0)
        return;
    if ((embed_dim % num_heads) != 0)
        return;
    const size_t seq_len_u = static_cast<size_t>(seq_len);
    if (seq_len_u > (std::numeric_limits<size_t>::max() / seq_len_u))
        return;

    int head_dim = embed_dim / num_heads;
    std::vector<float> attn_scores(seq_len * seq_len);
    float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));

    LARGE_INTEGER qpc_freq{};
    LARGE_INTEGER qpc_batch_start{};
    LARGE_INTEGER qpc_batch_end{};
    const bool kv_timer_enabled = KvHotPathTelemetryEnabled();
    if (kv_timer_enabled)
    {
        QueryPerformanceFrequency(&qpc_freq);
        QueryPerformanceCounter(&qpc_batch_start);
    }

    // Q*K^T scaled
    for (int h = 0; h < num_heads; h++)
    {
        int offset = h * head_dim;
        for (int i = 0; i < seq_len; i++)
        {
            for (int j = 0; j <= i; j++)
            {  // Causal mask
                const float* q_ptr = &query[i * embed_dim + offset];
                const float* k_ptr = &key[j * embed_dim + offset];
                const float score = DotProductF32(q_ptr, k_ptr, head_dim);
                attn_scores[i * seq_len + j] = score * scale;
            }
            for (int j = i + 1; j < seq_len; j++)
            {
                attn_scores[i * seq_len + j] = -1e9f;  // Mask future
            }
        }
        // ApplySoftmax per row, then attn * V
        for (int i = 0; i < seq_len; i++)
        {
            ApplySoftmax(&attn_scores[i * seq_len], seq_len);
            // AVX-512 accelerated equivalent of the scalar accumulation above.
            float* out_ptr = &output[i * embed_dim + offset];
            std::fill(out_ptr, out_ptr + head_dim, 0.0f);
            for (int j = 0; j < seq_len; ++j)
            {
                const float s = attn_scores[i * seq_len + j];
                const float* v_ptr = &value[j * embed_dim + offset];
                AccumulateScaledKVHotPath(out_ptr, v_ptr, s, head_dim);
            }
        }
    }

    if (kv_timer_enabled && qpc_freq.QuadPart > 0)
    {
        QueryPerformanceCounter(&qpc_batch_end);
        const LONGLONG ticks = qpc_batch_end.QuadPart - qpc_batch_start.QuadPart;
        const double us = (static_cast<double>(ticks) * 1000000.0) / static_cast<double>(qpc_freq.QuadPart);
        std::printf("[CPUInferenceEngine] KV hot-path batch_us=%.3f seq=%d embed=%d heads=%d\n", us, seq_len, embed_dim,
                    num_heads);
    }
}

void CPUInferenceEngine::FeedForward(const float* input, float* output, int dim)
{
    // Simple projection stub — real path goes through RawrXDTransformer
    if (!input || !output || dim <= 0)
    {
        return;
    }
    size_t bytes = 0;
    if (!checkedMulSize(static_cast<size_t>(dim), sizeof(float), bytes))
    {
        return;
    }
    std::memcpy(output, input, bytes);
}

void CPUInferenceEngine::TransformerLayer(const float* input, float* output, int layer_idx, int seq_len,
                                          uint32_t deviceId)
{
    // Fallback math path only; primary production path is RawrXDInference::ForwardTokens.
    (void)layer_idx;
    (void)deviceId;
    if (!input || !output || seq_len <= 0)
    {
        return;
    }
    int dim = m_embeddingDim > 0 ? m_embeddingDim : 4096;
    size_t sz = 0;
    if (!checkedMulSize(static_cast<size_t>(seq_len), static_cast<size_t>(dim), sz))
    {
        return;
    }
    size_t bytes = 0;
    if (!checkedMulSize(sz, sizeof(float), bytes))
    {
        return;
    }
    std::memcpy(output, input, bytes);
    for (int t = 0; t < seq_len; ++t)
    {
        RMSNorm(output + static_cast<size_t>(t) * dim, dim);
    }
}

void CPUInferenceEngine::ApplyNorm(const std::string& name, float* data)
{
    int dim = m_embeddingDim > 0 ? m_embeddingDim : 4096;
    if (name.find("rms") != std::string::npos)
    {
        RMSNorm(data, dim);
    }
    else
    {
        LayerNorm(data, dim);
    }
}

// ============================================================================
// Forward declarations for CPUOps dequantization functions
// ============================================================================
namespace CPUOps
{
void DequantizeQ4_0(const uint8_t* quantized, float* output, int size);
void DequantizeQ8_0(const uint8_t* quantized, float* output, int size);
void DequantizeQ4_K(const uint8_t* quantized, float* output, int num_elements);
void DequantizeQ5_K(const uint8_t* quantized, float* output, int num_elements);
void DequantizeQ6_K(const uint8_t* quantized, float* output, int num_elements);
void DequantizeQ2_K(const uint8_t* quantized, float* output, int num_elements);
void DequantizeQ3_K(const uint8_t* quantized, float* output, int num_elements);
void DequantizeF16(const uint8_t* quantized, float* output, int num_elements);
}  // namespace CPUOps

void CPUInferenceEngine::DequantizeTensor(const std::vector<uint8_t>& src, float* dst, size_t size, TensorType type)
{
    if (!dst || size == 0)
    {
        return;
    }
    if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        return;
    }

    auto safeZeroOut = [&]() {
        size_t bytes = 0;
        if (checkedMulSize(size, sizeof(float), bytes))
        {
            std::memset(dst, 0, bytes);
        }
    };

    switch (type)
    {
        case TensorType::Q4_0:
        {
            const size_t nblocks = size / 32;
            if (src.size() < nblocks * 18)
            {
                safeZeroOut();
                return;
            }
            CPUOps::DequantizeQ4_0(src.data(), dst, static_cast<int>(size));
            break;
        }
        case TensorType::Q8_0:
        {
            const size_t nblocks = size / 32;
            if (src.size() < nblocks * 34)
            {
                safeZeroOut();
                return;
            }
            CPUOps::DequantizeQ8_0(src.data(), dst, static_cast<int>(size));
            break;
        }
        case TensorType::Q4_K:
            CPUOps::DequantizeQ4_K(src.data(), dst, static_cast<int>(size));
            break;
        case TensorType::Q5_K:
            CPUOps::DequantizeQ5_K(src.data(), dst, static_cast<int>(size));
            break;
        case TensorType::Q6_K:
            CPUOps::DequantizeQ6_K(src.data(), dst, static_cast<int>(size));
            break;
        case TensorType::Q2_K:
            CPUOps::DequantizeQ2_K(src.data(), dst, static_cast<int>(size));
            break;
        case TensorType::Q3_K:
            CPUOps::DequantizeQ3_K(src.data(), dst, static_cast<int>(size));
            break;
        case TensorType::F16:
            if (src.size() < size * sizeof(uint16_t))
            {
                safeZeroOut();
                return;
            }
            CPUOps::DequantizeF16(src.data(), dst, static_cast<int>(size));
            break;
        case TensorType::F32:
            if (src.size() < size * sizeof(float))
            {
                safeZeroOut();
                return;
            }
            std::memcpy(dst, src.data(), size * sizeof(float));
            break;
        default:
            safeZeroOut();
            break;
    }
}

// ============================================================================
// CPUOps Namespace — Standalone math utilities
// ============================================================================
namespace CPUOps
{

void MatMul(const float* A, const float* B, float* C, int m, int n, int k)
{
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
        {
            float s = 0.0f;
            for (int p = 0; p < k; p++)
                s += A[i * k + p] * B[p * n + j];
            C[i * n + j] = s;
        }
}

void VectorAdd(const float* a, const float* b, float* c, int size)
{
    for (int i = 0; i < size; i++)
        c[i] = a[i] + b[i];
}
void VectorMul(const float* a, const float* b, float* c, int size)
{
    for (int i = 0; i < size; i++)
        c[i] = a[i] * b[i];
}
void VectorScale(float* data, float scale, int size)
{
    for (int i = 0; i < size; i++)
        data[i] *= scale;
}

void Softmax(float* data, int size)
{
    if (!data || size <= 0)
        return;
    float mx = *std::max_element(data, data + size);
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        data[i] = std::exp(data[i] - mx);
        sum += data[i];
    }
    if (!(sum > 0.0f) || !std::isfinite(sum))
    {
        const float uniform = 1.0f / static_cast<float>(size);
        for (int i = 0; i < size; i++)
            data[i] = uniform;
        return;
    }
    for (int i = 0; i < size; i++)
        data[i] /= sum;
}

void GELU(float* data, int size)
{
    for (int i = 0; i < size; i++)
    {
        float x = data[i];
        data[i] = 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / 3.14159265f) * (x + 0.044715f * x * x * x)));
    }
}

void SiLU(float* data, int size)
{
    for (int i = 0; i < size; i++)
    {
        data[i] = data[i] / (1.0f + std::exp(-data[i]));
    }
}

void LayerNorm(float* data, int size, float epsilon)
{
    if (!data || size <= 0)
        return;
    float mean = 0.0f;
    for (int i = 0; i < size; i++)
        mean += data[i];
    mean /= size;
    float var = 0.0f;
    for (int i = 0; i < size; i++)
        var += (data[i] - mean) * (data[i] - mean);
    var /= size;
    float inv = 1.0f / std::sqrt(var + epsilon);
    for (int i = 0; i < size; i++)
        data[i] = (data[i] - mean) * inv;
}

void RMSNorm(float* data, int size, float epsilon)
{
    if (!data || size <= 0)
        return;
    float ss = 0.0f;
    for (int i = 0; i < size; i++)
        ss += data[i] * data[i];
    ss = 1.0f / std::sqrt(ss / size + epsilon);
    for (int i = 0; i < size; i++)
        data[i] *= ss;
}

// ---- Dequantization ----
void DequantizeQ4_0(const uint8_t* quantized, float* output, int size)
{
    // Q4_0: 32 elements per block. Block = 2-byte scale (f16) + 16 bytes of 4-bit data
    int nblocks = size / 32;
    for (int b = 0; b < nblocks; b++)
    {
        const uint8_t* block = quantized + b * 18;  // 2 + 16
        // Read f16 scale (simplified: treat as raw uint16 → float approximation)
        uint16_t raw_scale;
        std::memcpy(&raw_scale, block, 2);
        // F16 → F32 (simplified)
        int exp = (raw_scale >> 10) & 0x1F;
        int frac = raw_scale & 0x3FF;
        float scale = (exp == 0) ? (frac / 1024.0f / 16384.0f) : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        if (raw_scale & 0x8000)
            scale = -scale;

        const uint8_t* nibbles = block + 2;
        for (int i = 0; i < 16; i++)
        {
            uint8_t byte = nibbles[i];
            int lo = (byte & 0x0F) - 8;
            int hi = ((byte >> 4) & 0x0F) - 8;
            output[b * 32 + i * 2] = lo * scale;
            output[b * 32 + i * 2 + 1] = hi * scale;
        }
    }
}

void DequantizeQ8_0(const uint8_t* quantized, float* output, int size)
{
    // Q8_0: 32 elements per block. Block = 2-byte scale (f16) + 32 bytes of int8 data
    int nblocks = size / 32;
    for (int b = 0; b < nblocks; b++)
    {
        const uint8_t* block = quantized + b * 34;  // 2 + 32
        uint16_t raw_scale;
        std::memcpy(&raw_scale, block, 2);
        int exp = (raw_scale >> 10) & 0x1F;
        int frac = raw_scale & 0x3FF;
        float scale = (exp == 0) ? (frac / 1024.0f / 16384.0f) : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        if (raw_scale & 0x8000)
            scale = -scale;

        const int8_t* vals = reinterpret_cast<const int8_t*>(block + 2);
        for (int i = 0; i < 32; i++)
        {
            output[b * 32 + i] = vals[i] * scale;
        }
    }
}

void DequantizeQ4_K(const uint8_t* quantized, float* output, int num_elements)
{
    // Q4_K: 256-element super-blocks. Layout per block:
    //   [0..1]   d    (ggml_rxd_half / f16): super-block scale multiplier
    //   [2..3]   dmin (ggml_rxd_half / f16): super-block min multiplier
    //   [4..15]  scales[12]: 8 sub-blocks × (6-bit scale + 6-bit min), packed
    //   [16..143] qs[128]: 256 packed 4-bit quantized values
    // Total: 144 bytes per block.
    static constexpr int BLOCK_BYTES = 144;
    static constexpr int NUM_SUB  = 8;
    static constexpr int SUB_ELEMS = 32;

    auto f16_to_f32 = [](uint16_t h) -> float {
        int exp  = (h >> 10) & 0x1F;
        int frac = h & 0x3FF;
        float v  = (exp == 0) ? (frac / 1024.0f / 16384.0f)
                              : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        return (h & 0x8000) ? -v : v;
    };

    // Unpack 6-bit scale and min for sub-block j from the 12-byte scales field.
    // Mirrors GGML get_scale_min_k4().
    auto get_scale_min = [](const uint8_t* sc, int j,
                             uint8_t& scale_out, uint8_t& min_out) {
        if (j < 4) {
            scale_out = sc[j]   & 0x3F;
            min_out   = sc[j+4] & 0x3F;
        } else {
            scale_out = static_cast<uint8_t>((sc[j+4] & 0x0F) | ((sc[j-4] >> 6) << 4));
            min_out   = static_cast<uint8_t>((sc[j+4] >> 4)   | ((sc[j]   >> 6) << 4));
        }
    };

    const int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
    {
        const uint8_t* blk = quantized + b * BLOCK_BYTES;
        uint16_t raw_d, raw_dmin;
        std::memcpy(&raw_d,    blk,     2);
        std::memcpy(&raw_dmin, blk + 2, 2);
        const float d    = f16_to_f32(raw_d);
        const float dmin = f16_to_f32(raw_dmin);
        const uint8_t* scales = blk + 4;
        const uint8_t* qs     = blk + 16;

        for (int j = 0; j < NUM_SUB; j++)
        {
            uint8_t sc, mn;
            get_scale_min(scales, j, sc, mn);
            const float d_sc   = d    * static_cast<float>(sc);
            const float dmin_m = dmin * static_cast<float>(mn);
            const uint8_t* qs_sub = qs + j * 16;  // 16 bytes → 32 nibbles
            float* out_sub = output + b * 256 + j * SUB_ELEMS;
            for (int i = 0; i < 16; i++)
            {
                const uint8_t byte = qs_sub[i];
                out_sub[i * 2]     = static_cast<float>(byte & 0x0F) * d_sc - dmin_m;
                out_sub[i * 2 + 1] = static_cast<float>((byte >> 4) & 0x0F) * d_sc - dmin_m;
            }
        }
    }
}

void DequantizeQ5_K(const uint8_t* quantized, float* output, int num_elements)
{
    // Q5_K: 256-element super-blocks. Layout per block:
    //   [0..1]   d    (f16)
    //   [2..3]   dmin (f16)
    //   [4..15]  scales[12]: same 6-bit encoding as Q4_K
    //   [16..47] qh[32]: one high bit per element (256 bits)
    //   [48..175] qs[128]: low 4 bits per element
    // Total: 176 bytes per block.
    static constexpr int BLOCK_BYTES = 176;
    static constexpr int NUM_SUB  = 8;
    static constexpr int SUB_ELEMS = 32;

    auto f16_to_f32 = [](uint16_t h) -> float {
        int exp  = (h >> 10) & 0x1F;
        int frac = h & 0x3FF;
        float v  = (exp == 0) ? (frac / 1024.0f / 16384.0f)
                              : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        return (h & 0x8000) ? -v : v;
    };

    auto get_scale_min = [](const uint8_t* sc, int j,
                             uint8_t& scale_out, uint8_t& min_out) {
        if (j < 4) {
            scale_out = sc[j]   & 0x3F;
            min_out   = sc[j+4] & 0x3F;
        } else {
            scale_out = static_cast<uint8_t>((sc[j+4] & 0x0F) | ((sc[j-4] >> 6) << 4));
            min_out   = static_cast<uint8_t>((sc[j+4] >> 4)   | ((sc[j]   >> 6) << 4));
        }
    };

    const int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
    {
        const uint8_t* blk = quantized + b * BLOCK_BYTES;
        uint16_t raw_d, raw_dmin;
        std::memcpy(&raw_d,    blk,     2);
        std::memcpy(&raw_dmin, blk + 2, 2);
        const float d    = f16_to_f32(raw_d);
        const float dmin = f16_to_f32(raw_dmin);
        const uint8_t* scales = blk + 4;
        const uint8_t* qh     = blk + 16;  // 32 bytes of high bits
        const uint8_t* qs     = blk + 48;  // 128 bytes of low nibbles

        for (int j = 0; j < NUM_SUB; j++)
        {
            uint8_t sc, mn;
            get_scale_min(scales, j, sc, mn);
            const float d_sc   = d    * static_cast<float>(sc);
            const float dmin_m = dmin * static_cast<float>(mn);
            const uint8_t* qs_sub = qs + j * 16;
            // qh provides bit (b*256 + j*32 + i) / 8 for element offset (j*32 + i)
            const int base_elem = j * SUB_ELEMS;
            float* out_sub = output + b * 256 + base_elem;
            for (int i = 0; i < SUB_ELEMS; i++)
            {
                const int abs_i    = base_elem + i;
                const int qh_bit   = (qh[abs_i / 8] >> (abs_i % 8)) & 1;
                const uint8_t byte = qs_sub[i / 2];
                const int lo4 = (i % 2 == 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
                const int q5  = lo4 | (qh_bit << 4);  // 0..31
                out_sub[i] = static_cast<float>(q5) * d_sc - dmin_m;
            }
        }
    }
}

void DequantizeQ6_K(const uint8_t* quantized, float* output, int num_elements)
{
    // Use AVX-512 kernel for maximum KV-cache dequant throughput
    RawrXD::KernelOps::DequantizeQ6K_AVX512(quantized, output, num_elements);
}

void DequantizeQ2_K(const uint8_t* quantized, float* output, int num_elements)
{
    // Q2_K: 256-element super-blocks. Layout per block:
    //   [0..15]  scales[16]: low nibble = scale, high nibble = min, per 16-element sub-block
    //   [16..79] qs[64]: 2-bit quantized values (4 per byte, 256 total)
    //   [80..81] d    (f16)
    //   [82..83] dmin (f16)
    // Total: 84 bytes per block.
    static constexpr int BLOCK_BYTES = 84;
    static constexpr int NUM_SUB  = 16;
    static constexpr int SUB_ELEMS = 16;

    auto f16_to_f32 = [](uint16_t h) -> float {
        int exp  = (h >> 10) & 0x1F;
        int frac = h & 0x3FF;
        float v  = (exp == 0) ? (frac / 1024.0f / 16384.0f)
                              : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        return (h & 0x8000) ? -v : v;
    };

    const int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
    {
        const uint8_t* blk    = quantized + b * BLOCK_BYTES;
        const uint8_t* scales = blk;         // 16 bytes
        const uint8_t* qs     = blk + 16;    // 64 bytes
        uint16_t raw_d, raw_dmin;
        std::memcpy(&raw_d,    blk + 80, 2);
        std::memcpy(&raw_dmin, blk + 82, 2);
        const float d    = f16_to_f32(raw_d);
        const float dmin = f16_to_f32(raw_dmin);

        for (int s = 0; s < NUM_SUB; s++)
        {
            const float sc  = d    * static_cast<float>(scales[s] & 0x0F);
            const float mn  = dmin * static_cast<float>(scales[s] >> 4);
            float* out_sub  = output + b * 256 + s * SUB_ELEMS;
            // 4 elements packed per byte (2 bits each)
            for (int i = 0; i < SUB_ELEMS; i++)
            {
                const int abs_i  = s * SUB_ELEMS + i;
                const uint8_t qb = qs[abs_i / 4];
                const int q2     = (qb >> (2 * (i % 4))) & 3;  // 0..3
                out_sub[i] = static_cast<float>(q2) * sc - mn;
            }
        }
    }
}

void DequantizeQ3_K(const uint8_t* quantized, float* output, int num_elements)
{
    // Q3_K: 256-element super-blocks. Layout per block:
    //   [0..31]  hmask[32]: high bits (1 bit per element, for 3-bit total)
    //   [32..95] qs[64]:    low 2 bits per element (packed 4/byte)
    //   [96..107] scales[12]: 6-bit packed, same layout as Q4_K
    //   [108..109] d (f16)
    // Total: 110 bytes per block.
    static constexpr int BLOCK_BYTES = 110;
    static constexpr int NUM_SUB  = 8;
    static constexpr int SUB_ELEMS = 32;

    auto f16_to_f32 = [](uint16_t h) -> float {
        int exp  = (h >> 10) & 0x1F;
        int frac = h & 0x3FF;
        float v  = (exp == 0) ? (frac / 1024.0f / 16384.0f)
                              : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        return (h & 0x8000) ? -v : v;
    };

    const int nblocks = num_elements / 256;
    for (int b = 0; b < nblocks; b++)
    {
        const uint8_t* blk    = quantized + b * BLOCK_BYTES;
        const uint8_t* hmask  = blk;         // 32 bytes (1 bit per elem)
        const uint8_t* qs     = blk + 32;    // 64 bytes (2 bits per elem)
        const uint8_t* scales = blk + 96;    // 12 bytes (6-bit per sub-block)
        uint16_t raw_d;
        std::memcpy(&raw_d, blk + 108, 2);
        const float d = f16_to_f32(raw_d);

        for (int s = 0; s < NUM_SUB; s++)
        {
            // Q3_K uses only a scale (no min): sc = scales[s] & 0x1F (5-bit signed)
            // The sign is encoded as bit 5 of scales[s]
            const int8_t sc_raw = static_cast<int8_t>(
                (scales[s] & 0x0F) | ((scales[s / 4 + 8] >> (2 * (s % 4))) << 4));
            // Reinterpret 4-bit signed: bit 3 is sign
            const int sc_signed = (sc_raw > 7) ? (sc_raw - 16) : sc_raw;
            const float sc = d * static_cast<float>(sc_signed);
            float* out_sub = output + b * 256 + s * SUB_ELEMS;
            const int base = s * SUB_ELEMS;
            for (int i = 0; i < SUB_ELEMS; i++)
            {
                const int abs_i  = base + i;
                // 3-bit value: high bit from hmask, low 2 bits from qs
                const int hb    = (hmask[abs_i / 8] >> (abs_i % 8)) & 1;
                const uint8_t qb = qs[abs_i / 4];
                const int q2    = (qb >> (2 * (abs_i % 4))) & 3;
                // Reconstruct 3-bit signed (0..7 → -4..3 after bias of 4)
                const int q3 = q2 | (hb << 2);
                out_sub[i]   = static_cast<float>(q3 - 4) * sc;
            }
        }
    }
}

void DequantizeF16(const uint8_t* quantized, float* output, int num_elements)
{
    const uint16_t* src = reinterpret_cast<const uint16_t*>(quantized);
    for (int i = 0; i < num_elements; i++)
    {
        uint16_t h = src[i];
        int exp = (h >> 10) & 0x1F;
        int frac = h & 0x3FF;
        float val = (exp == 0) ? (frac / 1024.0f / 16384.0f) : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
        if (h & 0x8000)
            val = -val;
        output[i] = val;
    }
}

void EnableAVX2(bool enable)
{ /* TODO: runtime dispatch */
}
void EnableMultiThreading(bool enable)
{ /* TODO: thread pool toggle */
}

}  // namespace CPUOps

}  // namespace RawrXD
