#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#
#include "gguf_loader.h"
#include "streaming_gguf_loader.h"
#

namespace
{

void appendU32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

void appendU64(std::vector<uint8_t>& out, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
    {
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

void appendBytes(std::vector<uint8_t>& out, const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    out.insert(out.end(), p, p + size);
}

void appendString(std::vector<uint8_t>& out, const std::string& s)
{
    appendU64(out, static_cast<uint64_t>(s.size()));
    appendBytes(out, s.data(), s.size());
}

uint64_t align32(uint64_t x)
{
    return (x + 31ULL) & ~31ULL;
}

uint64_t alignUp(uint64_t x, uint64_t alignment)
{
    if (alignment == 0)
        return x;
    const uint64_t r = x % alignment;
    return r ? (x + (alignment - r)) : x;
}

static uint32_t lcgNext(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

template <typename T> void shuffleLcg(std::vector<T>& v, uint32_t seed)
{
    if (v.size() <= 1)
        return;
    uint32_t s = seed ? seed : 0xC0FFEEu;
    for (size_t i = v.size() - 1; i > 0; --i)
    {
        const uint32_t r = lcgNext(s);
        const size_t j = static_cast<size_t>(r % (i + 1));
        std::swap(v[i], v[j]);
    }
}

std::filesystem::path writeMinimalGgufV3(const std::filesystem::path& path)
{
    // Minimal GGUF v3 with:
    // - required magic/version/counts
    // - metadata: architecture, tokenizer model, file_type, tokenizer tokens
    // - one tensor: token_embd.weight (F32) with 4 float elements

    constexpr uint32_t kMagic = 0x46554747u;  // "GGUF" LE
    constexpr uint32_t kVersion = 3u;
    constexpr uint64_t kTensorCount = 1u;
    constexpr uint64_t kKvCount = 4u;

    const std::string kArchKey = "general.architecture";
    const std::string kArchVal = "llama";
    const std::string kTokModelKey = "tokenizer.ggml.model";
    const std::string kTokModelVal = "gpt2";
    const std::string kFileTypeKey = "general.file_type";
    const uint32_t kFileTypeVal = 0u;  // F32
    const std::string kTokTokensKey = "tokenizer.ggml.tokens";
    const std::vector<std::string> kTokens = {"<unk>", "hello"};

    const std::string kTensorName = "token_embd.weight";
    const uint32_t kTensorNDims = 1u;
    const uint64_t kTensorDim0 = 4u;
    const uint32_t kTensorTypeF32 = 0u;  // GGMLType::F32
    const float kTensorData[4] = {1.0f, 2.0f, 3.5f, -4.0f};

    std::vector<uint8_t> buf;
    buf.reserve(4096);

    // Header
    appendU32(buf, kMagic);
    appendU32(buf, kVersion);
    appendU64(buf, kTensorCount);
    appendU64(buf, kKvCount);

    // KV: general.architecture (string)
    appendString(buf, kArchKey);
    appendU32(buf, 8u);  // string
    appendString(buf, kArchVal);

    // KV: tokenizer.ggml.model (string)
    appendString(buf, kTokModelKey);
    appendU32(buf, 8u);  // string
    appendString(buf, kTokModelVal);

    // KV: general.file_type (uint32)
    appendString(buf, kFileTypeKey);
    appendU32(buf, 4u);  // uint32
    appendU32(buf, kFileTypeVal);

    // KV: tokenizer.ggml.tokens (array of strings)
    appendString(buf, kTokTokensKey);
    appendU32(buf, 9u);  // array
    appendU32(buf, 8u);  // element type = string
    appendU64(buf, static_cast<uint64_t>(kTokens.size()));
    for (const auto& t : kTokens)
    {
        appendString(buf, t);
    }

    // Tensor info
    appendString(buf, kTensorName);
    appendU32(buf, kTensorNDims);
    appendU64(buf, kTensorDim0);
    appendU32(buf, kTensorTypeF32);

    const uint64_t dataStart = align32(static_cast<uint64_t>(buf.size() + 8 /* offset field */));
    const uint64_t tensorOffsetAbs = dataStart;
    appendU64(buf, tensorOffsetAbs);

    // Padding to reach dataStart
    if (buf.size() < dataStart)
    {
        buf.resize(static_cast<size_t>(dataStart), 0);
    }

    // Tensor bytes
    appendBytes(buf, kTensorData, sizeof(kTensorData));

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    out.flush();
    return path;
}

std::filesystem::path writeFragmentedGgufV3(const std::filesystem::path& path, uint32_t numTensors,
                                            uint64_t strideBytes, bool alignToStride, bool shuffleLayout,
                                            uint32_t shuffleSeed)
{
    // Fragmented GGUF v3:
    // - multiple small F32 tensors
    // - payloads spaced out across the file to stress offset math + seek behavior
    // - includes "embedding" + "layers_N" naming to exercise zone assignment

    constexpr uint32_t kMagic = 0x46554747u;  // "GGUF" LE
    constexpr uint32_t kVersion = 3u;
    constexpr uint32_t kTensorTypeF32 = 0u;  // GGMLType::F32

    if (numTensors < 4u)
        numTensors = 4u;
    if (strideBytes == 0)
        strideBytes = 1u * 1024u * 1024u;

    struct TensorSpec
    {
        std::string name;
        std::vector<uint64_t> dims;
        float values[4];
    };

    std::vector<TensorSpec> specs;
    specs.reserve(numTensors);

    // Canonical tensors: guarantee cross-zone and cross-layer-boundary coverage.
    specs.push_back({"token_embd.weight", {4u}, {11.0f, 12.0f, 13.0f, 14.0f}});
    specs.push_back({"blk.0.attn.weight", {4u}, {21.0f, 22.0f, 23.0f, 24.0f}});
    specs.push_back({"blk.7.ffn.weight", {4u}, {31.0f, 32.0f, 33.0f, 34.0f}});
    specs.push_back({"blk.8.attn.weight", {4u}, {41.0f, 42.0f, 43.0f, 44.0f}});

    // Fill the rest with per-layer tensors so zone assignment scales (8 layers per zone).
    // Note: the four canonical tensors above already consume some blk.N names
    // (blk.0.*, blk.7.*, blk.8.*). Ensure the generated set does not collide,
    // otherwise the streamer index (name -> tensor) will overwrite entries.
    constexpr uint32_t kGeneratedLayerBase = 16u;
    for (uint32_t i = 4u; i < numTensors; ++i)
    {
        const uint32_t layer = kGeneratedLayerBase + i;
        const bool useAttn = (i % 2u) == 0u;
        TensorSpec s;
        s.name = "blk." + std::to_string(layer) + (useAttn ? ".attn.weight" : ".ffn.weight");
        s.dims = {4u};
        const float base = static_cast<float>(1000.0 + layer * 10.0);
        s.values[0] = base + 1.0f;
        s.values[1] = base + 2.0f;
        s.values[2] = base + 3.0f;
        s.values[3] = base + 4.0f;
        specs.push_back(std::move(s));
    }

    const uint64_t kTensorCount = static_cast<uint64_t>(specs.size());
    constexpr uint64_t kKvCount = 4u;

    const std::string kArchKey = "general.architecture";
    const std::string kArchVal = "llama";
    const std::string kTokModelKey = "tokenizer.ggml.model";
    const std::string kTokModelVal = "gpt2";
    const std::string kFileTypeKey = "general.file_type";
    const uint32_t kFileTypeVal = 0u;  // F32
    const std::string kTokTokensKey = "tokenizer.ggml.tokens";
    const std::vector<std::string> kTokens = {"<unk>", "hello"};

    std::vector<uint8_t> buf;
    buf.reserve(8 * 1024 * 1024);

    // Header
    appendU32(buf, kMagic);
    appendU32(buf, kVersion);
    appendU64(buf, kTensorCount);
    appendU64(buf, kKvCount);

    // KV: general.architecture (string)
    appendString(buf, kArchKey);
    appendU32(buf, 8u);  // string
    appendString(buf, kArchVal);

    // KV: tokenizer.ggml.model (string)
    appendString(buf, kTokModelKey);
    appendU32(buf, 8u);  // string
    appendString(buf, kTokModelVal);

    // KV: general.file_type (uint32)
    appendString(buf, kFileTypeKey);
    appendU32(buf, 4u);  // uint32
    appendU32(buf, kFileTypeVal);

    // KV: tokenizer.ggml.tokens (array of strings)
    appendString(buf, kTokTokensKey);
    appendU32(buf, 9u);  // array
    appendU32(buf, 8u);  // element type = string
    appendU64(buf, static_cast<uint64_t>(kTokens.size()));
    for (const auto& t : kTokens)
    {
        appendString(buf, t);
    }

    // Tensor info array (we will fill offsets with absolute file offsets)
    // Offsets in this codebase are treated as absolute by StreamingGGUFLoader.
    struct PendingOffset
    {
        size_t offsetFieldPos;
        uint64_t payloadBytes;
        float values[4];
        std::string name;
    };

    std::vector<PendingOffset> pend;
    pend.reserve(specs.size());

    for (const auto& s : specs)
    {
        appendString(buf, s.name);
        appendU32(buf, static_cast<uint32_t>(s.dims.size()));
        for (auto d : s.dims)
            appendU64(buf, d);
        appendU32(buf, kTensorTypeF32);

        const size_t pos = buf.size();
        appendU64(buf, 0ULL);  // patch later

        PendingOffset p;
        p.offsetFieldPos = pos;
        p.payloadBytes = sizeof(float) * 4;
        std::memcpy(p.values, s.values, sizeof(p.values));
        p.name = s.name;
        pend.push_back(p);
    }

    // Payloads: align, then space + write each tensor.
    uint64_t cursor = align32(static_cast<uint64_t>(buf.size()));
    if (buf.size() < cursor)
        buf.resize(static_cast<size_t>(cursor), 0);

    std::vector<size_t> writeOrder(pend.size());
    for (size_t i = 0; i < writeOrder.size(); ++i)
        writeOrder[i] = i;
    if (shuffleLayout)
        shuffleLcg(writeOrder, shuffleSeed);

    for (size_t wi = 0; wi < writeOrder.size(); ++wi)
    {
        auto& p = pend[writeOrder[wi]];
        cursor = align32(cursor);
        cursor = alignToStride ? alignUp(cursor + strideBytes, strideBytes) : align32(cursor + strideBytes);
        if (buf.size() < cursor)
            buf.resize(static_cast<size_t>(cursor), 0);

        const uint64_t tensorOffsetAbs = cursor;
        // Patch the offset field.
        for (int i = 0; i < 8; ++i)
        {
            buf[p.offsetFieldPos + static_cast<size_t>(i)] = static_cast<uint8_t>((tensorOffsetAbs >> (8 * i)) & 0xFFu);
        }

        // Ensure buffer is large enough then write payload.
        if (buf.size() < tensorOffsetAbs)
            buf.resize(static_cast<size_t>(tensorOffsetAbs), 0);
        const size_t writePos = static_cast<size_t>(tensorOffsetAbs);
        if (buf.size() < writePos + p.payloadBytes)
            buf.resize(writePos + p.payloadBytes, 0);
        std::memcpy(buf.data() + writePos, p.values, p.payloadBytes);

        cursor = tensorOffsetAbs + p.payloadBytes;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    out.flush();
    return path;
}

}  // namespace

int main()
{
    const auto tmpDir = std::filesystem::temp_directory_path();
    const auto ggufPath = tmpDir / "rawrxd_minimal_model_loader_smoke.gguf";
    const auto ggufFragPath = tmpDir / "rawrxd_fragmented_model_loader_smoke.gguf";

    writeMinimalGgufV3(ggufPath);
    assert(std::filesystem::exists(ggufPath));
    uint32_t numTensors = 4;
    uint64_t strideBytes = 1u * 1024u * 1024u;
    bool alignToStride = true;
    bool randomOrder = true;
    uint32_t seed = 0xC0FFEEu;

    if (const char* e = std::getenv("RAWRXD_SMOKE_NUM_TENSORS"))
        numTensors = static_cast<uint32_t>(std::strtoul(e, nullptr, 10));
    if (const char* e = std::getenv("RAWRXD_SMOKE_STRIDE_BYTES"))
        strideBytes = static_cast<uint64_t>(std::strtoull(e, nullptr, 10));
    if (const char* e = std::getenv("RAWRXD_SMOKE_ALIGN_TO_STRIDE"))
        alignToStride = std::strtoul(e, nullptr, 10) != 0;
    if (const char* e = std::getenv("RAWRXD_SMOKE_RANDOM_ORDER"))
        randomOrder = std::strtoul(e, nullptr, 10) != 0;
    if (const char* e = std::getenv("RAWRXD_SMOKE_SEED"))
        seed = static_cast<uint32_t>(std::strtoul(e, nullptr, 10));

    bool shuffleLayout = true;
    if (const char* e = std::getenv("RAWRXD_SMOKE_SHUFFLE_LAYOUT"))
        shuffleLayout = std::strtoul(e, nullptr, 10) != 0;

    writeFragmentedGgufV3(ggufFragPath, numTensors, strideBytes, alignToStride, shuffleLayout, seed);
    assert(std::filesystem::exists(ggufFragPath));

    // 1) Baseline GGUFLoader (used across core/legacy code paths)
    {
        GGUFLoader loader;
        if (!loader.Open(ggufPath.string()))
        {
            return 10;
        }
        if (!loader.ParseMetadata())
        {
            return 11;
        }

        const auto hdr = loader.GetHeader();
        if (hdr.magic != 0x46554747u || hdr.version != 3u || hdr.tensor_count != 1u)
        {
            return 12;
        }

        const auto meta = loader.GetMetadata();
        if (meta.kv_pairs.find("general.architecture") == meta.kv_pairs.end())
        {
            return 13;
        }
        if (meta.kv_pairs.find("tokenizer.ggml.model") == meta.kv_pairs.end())
        {
            return 14;
        }

        const auto tensors = loader.GetTensorInfo();
        if (tensors.size() != 1u)
        {
            return 15;
        }
    }

    // 2) StreamingGGUFLoader (engine used by the real inference lane)
    {
        RawrXD::StreamingGGUFLoader streaming;
        if (!streaming.Open(ggufPath.string()))
        {
            return 20;
        }

        const auto hdr = streaming.GetHeader();
        if (hdr.magic != 0x46554747u || hdr.version != 3u || hdr.tensor_count != 1u)
        {
            return 21;
        }

        const auto meta = streaming.GetMetadata();
        if (meta.kv_pairs.find("general.architecture") == meta.kv_pairs.end())
        {
            return 22;
        }
        if (meta.kv_pairs.find("tokenizer.ggml.model") == meta.kv_pairs.end())
        {
            return 23;
        }

        const auto vocab = streaming.GetVocabulary();
        if (vocab.size() != 2u || vocab[0] != "<unk>" || vocab[1] != "hello")
        {
            return 24;
        }

        const auto tensors = streaming.GetTensorInfo();
        if (tensors.size() != 1u || tensors[0].name != "token_embd.weight")
        {
            return 25;
        }

        std::vector<uint8_t> tensorBytes;
        if (!streaming.LoadTensorZone("token_embd.weight", tensorBytes))
        {
            return 26;
        }
        if (tensorBytes.size() != sizeof(float) * 4)
        {
            return 27;
        }

        float got[4] = {};
        std::memcpy(got, tensorBytes.data(), sizeof(got));
        if (!(got[0] == 1.0f && got[1] == 2.0f && got[2] == 3.5f && got[3] == -4.0f))
        {
            return 28;
        }
    }

    // 3) Fragmented streaming load: multiple tensors spread across file
    {
        RawrXD::StreamingGGUFLoader streaming;
        if (!streaming.Open(ggufFragPath.string()))
        {
            return 30;
        }

        const auto hdr = streaming.GetHeader();
        if (hdr.magic != 0x46554747u || hdr.version != 3u || hdr.tensor_count != static_cast<uint64_t>(numTensors))
        {
            return 31;
        }

        const auto vocab = streaming.GetVocabulary();
        if (vocab.size() != 2u)
        {
            return 32;
        }

        struct Case
        {
            std::string name;
            float expected[4];
        };
        std::vector<Case> cases;
        cases.reserve(numTensors);

        cases.push_back({"token_embd.weight", {11.0f, 12.0f, 13.0f, 14.0f}});
        cases.push_back({"blk.0.attn.weight", {21.0f, 22.0f, 23.0f, 24.0f}});
        cases.push_back({"blk.7.ffn.weight", {31.0f, 32.0f, 33.0f, 34.0f}});
        cases.push_back({"blk.8.attn.weight", {41.0f, 42.0f, 43.0f, 44.0f}});
        constexpr uint32_t kGeneratedLayerBase = 16u;
        for (uint32_t i = 4u; i < numTensors; ++i)
        {
            const uint32_t layer = kGeneratedLayerBase + i;
            const bool useAttn = (i % 2u) == 0u;
            Case c;
            c.name = "blk." + std::to_string(layer) + (useAttn ? ".attn.weight" : ".ffn.weight");
            const float base = static_cast<float>(1000.0 + layer * 10.0);
            c.expected[0] = base + 1.0f;
            c.expected[1] = base + 2.0f;
            c.expected[2] = base + 3.0f;
            c.expected[3] = base + 4.0f;
            cases.push_back(std::move(c));
        }

        if (randomOrder)
            shuffleLcg(cases, seed);

        for (const auto& c : cases)
        {
            std::vector<uint8_t> tensorBytes;
            if (!streaming.LoadTensorZone(c.name, tensorBytes))
            {
                return 33;
            }
            if (tensorBytes.size() != sizeof(float) * 4)
            {
                return 34;
            }
            float got[4] = {};
            std::memcpy(got, tensorBytes.data(), sizeof(got));
            if (!(got[0] == c.expected[0] && got[1] == c.expected[1] && got[2] == c.expected[2] &&
                  got[3] == c.expected[3]))
            {
                return 35;
            }
        }
    }

    std::error_code ec;
    std::filesystem::remove(ggufPath, ec);
    std::filesystem::remove(ggufFragPath, ec);
    return 0;
}
