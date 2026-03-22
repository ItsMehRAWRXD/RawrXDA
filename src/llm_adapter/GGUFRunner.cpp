#include "GGUFRunner.h"
#include "../core/model_runtime_gate.h"
#include "../logging/Logger.h"
#include "QuantBackend.h"
#include "brutal_gzip.h"
#include "gguf_k_quants.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// AVX2 intrinsics for SIMD matrix multiplication
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#include <immintrin.h>
#define GGUF_USE_AVX2 1
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#define USE_MMAP 1
#else
#define USE_MMAP 0
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// winreg.h / related headers can define Logging as a macro name — breaks RawrXD::Logging::Logger below.
#ifdef Logging
#undef Logging
#endif
#endif

// Keep linkage visible to the ASM translation unit.
extern "C" void matmul_kernel_avx2(float* A, float* B, float* C, int N, int M, int K, bool accumulate = false);
extern "C" void ggml_gemm_q4_0(int M, int N, int K, const float* A, const uint8_t* Bq4, float scale, float* C);

namespace
{
constexpr const char* kDefaultModelPath = "model/llama-7b-q4_0.gguf";
struct GGUFHeader
{
    uint32_t magic{0};
    uint32_t version{0};
    uint64_t tensorCount{0};
    uint64_t kvCount{0};
};

void skipGgufValue(RawrXD::BinaryStream& ds, uint32_t type)
{
    switch (type)
    {
        case 0:
        {
            uint8_t v;
            ds >> v;
            break;
        }
        case 1:
        {
            int8_t v;
            ds >> v;
            break;
        }
        case 2:
        {
            uint16_t v;
            ds >> v;
            break;
        }
        case 3:
        {
            int16_t v;
            ds >> v;
            break;
        }
        case 4:
        {
            uint32_t v;
            ds >> v;
            break;
        }
        case 5:
        {
            int32_t v;
            ds >> v;
            break;
        }
        case 6:
        {
            float v;
            ds >> v;
            break;
        }
        case 7:
        {
            bool v;
            ds >> v;
            break;
        }
        case 8:
        {
            uint64_t len;
            ds >> len;
            ds.skipRawData(static_cast<int>(len));
            break;
        }
        case 9:
        {
            uint32_t elemType;
            ds >> elemType;
            uint64_t len;
            ds >> len;
            for (uint64_t i = 0; i < len; ++i)
                skipGgufValue(ds, elemType);
            break;
        }
        default:
            ds.setStatus(RawrXD::StreamStatus::ReadCorruptData);
            break;
    }
}

std::string readGgufStr(RawrXD::BinaryStream& ds)
{
    uint64_t len;
    ds >> len;
    std::vector<uint8_t> ba(static_cast<size_t>(len), '\0');
    if (len > 0)
        ds.readRawData(reinterpret_cast<char*>(ba.data()), static_cast<int>(len));
    return std::string(reinterpret_cast<const char*>(ba.data()), ba.size());
}

float f16ToF32(uint16_t h)
{
    uint32_t sign = (h >> 15) & 1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;

    if (exp == 0)
    {
        if (mant == 0)
            return sign ? -0.0f : 0.0f;
        // Denormal
        while ((mant & 0x400) == 0)
        {
            mant <<= 1;
            exp--;
        }
        exp++;
        mant &= 0x3FF;
    }
    else if (exp == 31)
    {
        return mant ? NAN : (sign ? -INFINITY : INFINITY);
    }

    exp = exp - 15 + 127;
    uint32_t f32 = (sign << 31) | (exp << 23) | (mant << 13);
    float result;
    std::memcpy(&result, &f32, sizeof(float));
    return result;
}

// Scalar Q4_0 dequantization: 32 weights per block (16 bytes + 2 bytes delta)
void dequantizeRowQ4_0_scalar(const void* src, float* dst, size_t n)
{
    const BlockQ4_0* b = reinterpret_cast<const BlockQ4_0*>(src);
    size_t nb = n / 32;  // 32 weights per block
    for (size_t i = 0; i < nb; ++i)
    {
        float d = f16ToF32(b[i].d);
        for (size_t j = 0; j < 16; ++j)
        {
            int vi = (b[i].qs[j] & 0xF) - 8;
            dst[i * 32 + j] = vi * d;
        }
        for (size_t j = 0; j < 16; ++j)
        {
            int vi = (b[i].qs[j] >> 4) - 8;
            dst[i * 32 + j + 16] = vi * d;
        }
    }
}

// Scalar Q8_0 dequantization: 32 weights per block (32 bytes + 2 bytes delta)
void dequantizeRowQ8_0_scalar(const void* src, float* dst, size_t n)
{
    const BlockQ8_0* b = reinterpret_cast<const BlockQ8_0*>(src);
    size_t nb = n / 32;
    for (size_t i = 0; i < nb; ++i)
    {
        float d = f16ToF32(b[i].d);
        for (size_t j = 0; j < 32; ++j)
        {
            dst[i * 32 + j] = static_cast<float>(b[i].qs[j]) * d;
        }
    }
}

}  // namespace

bool GGUFRunner::parseGgufTensorTable(RawrXD::NativeFile& file)
{
    file.seek(0);
    RawrXD::BinaryStream ds(file.getStream());
    ds.setByteOrderLittleEndian();

    uint32_t magic, version;
    uint64_t tensorCount, kvCount;
    ds >> magic >> version >> tensorCount >> kvCount;
    if (magic != 0x46554747)
        return false;
    if (version < 2)
        return false;

    for (uint64_t i = 0; i < kvCount; ++i)
    {
        uint64_t keyLen;
        ds >> keyLen;
        if (ds.status() != RawrXD::StreamStatus::Ok)
            return false;
        if (keyLen > 0)
        {
            ds.skipRawData(static_cast<int>(keyLen));
        }
        uint32_t valueType;
        ds >> valueType;
        if (ds.status() != RawrXD::StreamStatus::Ok)
            return false;
        skipGgufValue(ds, valueType);
        if (ds.status() != RawrXD::StreamStatus::Ok)
            return false;
    }

    context_.tensorTable.clear();
    context_.tensorDataBaseOffset = 0;

    for (uint64_t i = 0; i < tensorCount; ++i)
    {
        ModelContext::TensorDesc desc;
        uint64_t nameLen;
        ds >> nameLen;
        constexpr uint64_t kMaxTensorNameLen = 1u << 20;
        if (nameLen > kMaxTensorNameLen || nameLen > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            return false;
        }
        std::vector<uint8_t> nameBa = file.read(static_cast<int>(nameLen));
        desc.name = std::string(reinterpret_cast<const char*>(nameBa.data()), nameBa.size());
        uint32_t nDims;
        ds >> nDims;
        desc.dims.resize(nDims);
        for (uint32_t d = 0; d < nDims; ++d)
        {
            uint64_t dim;
            ds >> dim;
            desc.dims[d] = static_cast<uint32_t>(dim);
        }
        uint32_t typeRaw;
        ds >> typeRaw;
        desc.type = static_cast<GgmlType>(typeRaw);
        uint64_t relOffset = 0;
        ds >> relOffset;
        // GGUF: stored offset is relative to aligned tensor-data base (not file[0]). See ggml/llama.cpp writers.
        desc.offset = relOffset;
        context_.tensorTable.emplace(desc.name, desc);
    }
    if (ds.status() != RawrXD::StreamStatus::Ok)
    {
        return false;
    }

    const std::streampos endMetaPos = file.getStream().tellg();
    if (endMetaPos == std::streampos(-1))
    {
        context_.tensorTable.clear();
        context_.tensorDataBaseOffset = 0;
        return false;
    }
    const uint64_t endMeta = static_cast<uint64_t>(endMetaPos);
    uint64_t align = static_cast<uint64_t>(context_.ggufTensorAlignment);
    if (align == 0)
    {
        align = 32;
    }
    // GGUF: tensor.offset in the file is relative to the aligned start of the tensor *blob*, not to byte 0.
    // Equivalent to: abs = mmap_base + tensor_data_offset + rel, where tensor_data_offset is the aligned
    // end of the tensor info table (we compute it as tensorDataBase; KV general.alignment drives padding).
    const uint64_t tensorDataBase = (endMeta + align - 1u) / align * align;
    context_.tensorDataBaseOffset = tensorDataBase;

    const int64_t sz64 = file.size();
    const uint64_t fileSize = sz64 > 0 ? static_cast<uint64_t>(sz64) : 0u;
    for (auto& kv : context_.tensorTable)
    {
        const uint64_t rel = kv.second.offset;
        if (rel > UINT64_MAX - tensorDataBase)
        {
            context_.tensorTable.clear();
            context_.tensorDataBaseOffset = 0;
            return false;
        }
        const uint64_t absOff = tensorDataBase + rel;
        if (fileSize != 0 && absOff > fileSize)
        {
            context_.tensorTable.clear();
            context_.tensorDataBaseOffset = 0;
            return false;
        }
        kv.second.offset = absOff;
    }

    return true;
}

bool GGUFRunner::hasModelFileBacking() const
{
    return context_.mappedData != nullptr || context_.usesUnifiedFileBacking;
}

void GGUFRunner::releaseWeightFileBacking()
{
#if defined(_WIN32)
    if (context_.usesUnifiedFileBacking && context_.unifiedModelBuffer.ptr != nullptr)
    {
        (void)RawrXD::UnifiedMemory::UnifiedMemoryExecutor::instance().free(context_.unifiedModelBuffer);
    }
    context_.unifiedModelBuffer = {};
    context_.usesUnifiedFileBacking = false;
    context_.unifiedFileBase = nullptr;
    context_.unifiedFileBytes = 0;
#endif
#if USE_MMAP
    if (context_.mappedData != nullptr && context_.usesMmap)
    {
        (void)::munmap(static_cast<void*>(context_.mappedData), static_cast<size_t>(context_.modelFileSize));
        context_.mappedData = nullptr;
        context_.usesMmap = false;
    }
#endif
#if defined(_WIN32)
    if (context_.win32MapView != nullptr)
    {
        (void)UnmapViewOfFile(context_.win32MapView);
        context_.win32MapView = nullptr;
        context_.mappedData = nullptr;
        context_.usesMmap = false;
    }
    if (context_.win32MapHandle != nullptr)
    {
        (void)CloseHandle(static_cast<HANDLE>(context_.win32MapHandle));
        context_.win32MapHandle = nullptr;
    }
    if (context_.win32FileHandle != nullptr)
    {
        (void)CloseHandle(static_cast<HANDLE>(context_.win32FileHandle));
        context_.win32FileHandle = nullptr;
    }
#endif
    if (context_.mappedData != nullptr)
    {
        delete[] context_.mappedData;
        context_.mappedData = nullptr;
    }
}

GGUFRunner::GGUFRunner(void* parent)
{
    (void)parent;
    checkCpuFeatures();
    loadGGUFModel(std::string(kDefaultModelPath));

    if (context_.vocabSize > 0)
    {
        context_.logits.resize(context_.vocabSize);
    }

    if (hasModelFileBacking() && !context_.modelPath.empty())
    {
        RawrXD::Runtime::ModelRuntimeGate::instance().notifyModelResident(
            context_.modelPath, static_cast<std::uint64_t>(context_.modelFileSize));
    }
}

GGUFRunner::~GGUFRunner()
{
    if (!context_.modelPath.empty())
    {
        RawrXD::Runtime::ModelRuntimeGate::instance().notifyModelUnloaded(context_.modelPath);
    }
    releaseWeightFileBacking();

    context_.logits.clear();
    context_.vocabulary.clear();
}

bool GGUFRunner::inferenceWeightsReady() const
{
    if (!hasModelFileBacking())
    {
        return false;
    }
    // n_layers==0 is malformed for this runner; do not treat as "ready" (would skip the layer loop then
    // hit QuantBackend / logits paths with inconsistent state).
    if (context_.nLayers == 0)
    {
        return false;
    }
    if (context_.layers.size() < context_.nLayers)
    {
        return false;
    }
    const auto& L0 = context_.layers[0];
    const size_t d = context_.embedDim;
    if (d == 0)
    {
        return false;
    }
    const size_t kvDim = context_.nKVHeads * context_.headDim;
    if (kvDim == 0)
    {
        return false;
    }
    const size_t expectedSq = d * d;
    if (L0.attn_q_w.size() < expectedSq || L0.attn_o_w.size() < expectedSq)
    {
        return false;
    }
    if (L0.attn_k_w.size() < d * kvDim || L0.attn_v_w.size() < d * kvDim)
    {
        return false;
    }
    const size_t f = context_.ffnDim;
    if (f == 0 || L0.mlp_up_w.size() < d * f || L0.mlp_gate_w.size() < d * f || L0.mlp_down_w.size() < f * d)
    {
        return false;
    }
    return true;
}

void GGUFRunner::ingestGgufKvMetadata(RawrXD::BinaryStream& ds, uint64_t kvCount)
{
    using RawrXD::StreamStatus;
    for (uint64_t i = 0; i < kvCount; ++i)
    {
        uint64_t keyLen = 0;
        ds >> keyLen;
        if (ds.status() != StreamStatus::Ok)
        {
            return;
        }
        constexpr uint64_t kMaxKeyBytes = 4096;
        if (keyLen > kMaxKeyBytes)
        {
            ds.setStatus(StreamStatus::ReadCorruptData);
            return;
        }
        std::string key(static_cast<size_t>(keyLen), '\0');
        if (keyLen > 0)
        {
            const int n = static_cast<int>(keyLen);
            if (ds.readRawData(key.data(), n) != n)
            {
                return;
            }
        }
        uint32_t valueType = 0;
        ds >> valueType;
        if (ds.status() != StreamStatus::Ok)
        {
            return;
        }

        const auto skipValue = [&]() { skipGgufValue(ds, valueType); };

        if (key == "llama.block_count")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                context_.nLayers = static_cast<size_t>(v);
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                context_.nLayers = (v > 0) ? static_cast<size_t>(v) : 0;
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "llama.embedding_length")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                context_.embedDim = static_cast<size_t>(v);
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                context_.embedDim = (v > 0) ? static_cast<size_t>(v) : 0;
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "llama.vocab_size")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                context_.vocabSize = static_cast<size_t>(v);
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                context_.vocabSize = (v > 0) ? static_cast<size_t>(v) : 0;
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "llama.attention.head_count")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                context_.nHeads = static_cast<size_t>(v);
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                context_.nHeads = (v > 0) ? static_cast<size_t>(v) : 0;
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "llama.attention.head_count_kv")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                context_.nKVHeads = static_cast<size_t>(v);
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                context_.nKVHeads = (v > 0) ? static_cast<size_t>(v) : 0;
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "general.alignment")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                if (v > 0 && (v & (v - 1u)) == 0)
                {
                    context_.ggufTensorAlignment = v;
                }
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                const uint32_t u = static_cast<uint32_t>(v);
                if (v > 0 && (u & (u - 1u)) == 0)
                {
                    context_.ggufTensorAlignment = u;
                }
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "llama.feed_forward_length")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                context_.ffnDim = static_cast<size_t>(v);
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                if (v > 0)
                {
                    context_.ffnDim = static_cast<size_t>(v);
                }
            }
            else
            {
                skipValue();
            }
        }
        else if (key == "general.architecture" && valueType == 8)
        {
            context_.architecture = readGgufStr(ds);
        }
        else if (key == "general.name" && valueType == 8)
        {
            context_.modelName = readGgufStr(ds);
        }
        else
        {
            skipValue();
        }

        if (ds.status() != StreamStatus::Ok)
        {
            return;
        }
    }
}

void GGUFRunner::sanitizeArchMetadata()
{
    constexpr size_t kMaxLayers = 1024;
    constexpr size_t kMaxEmbed = 262144;
    constexpr size_t kMaxVocab = size_t{1} << 28;
    constexpr size_t kMaxHeads = 4096;

    if (context_.nLayers > kMaxLayers)
    {
        RawrXD::Logging::Logger::instance().warning(
            std::string("[GGUFRunner] llama.block_count=") + std::to_string(context_.nLayers) +
                " exceeds sanity cap; treating as unloadable to avoid KV cache overflow / UB.",
            "GGUFRunner");
        context_.nLayers = 0;
    }
    if (context_.embedDim > kMaxEmbed)
    {
        RawrXD::Logging::Logger::instance().warning(std::string("[GGUFRunner] embedding_length=") +
                                                        std::to_string(context_.embedDim) +
                                                        " exceeds sanity cap; clamping metadata.",
                                                    "GGUFRunner");
        context_.embedDim = 0;
    }
    if (context_.vocabSize > kMaxVocab)
    {
        context_.vocabSize = 0;
    }
    if (context_.nHeads > kMaxHeads)
    {
        context_.nHeads = 0;
    }
    if (context_.nKVHeads > kMaxHeads)
    {
        context_.nKVHeads = 0;
    }
}

bool GGUFRunner::runInference(const std::string& prompt, float* outputBuffer)
{
    if (!hasModelFileBacking())
    {
        inferenceComplete(false);
        return false;
    }

    if (!inferenceWeightsReady())
    {
        RawrXD::Logging::Logger::instance().error(
            std::string("[GGUFRunner] runInference refused: per-layer weights not loaded (layers in RAM=") +
                std::to_string(context_.layers.size()) + " metadata n_layers=" + std::to_string(context_.nLayers) +
                "). Per-layer `blk.*` tensors missing, RAM cap hit (RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB), or "
                "unsupported GGML type (e.g. IQ*); see parseGgufLayerWeights + gguf_k_quants.cpp.",
            "GGUFRunner");
        inferenceComplete(false);
        return false;
    }

    if (!outputBuffer)
    {
        inferenceComplete(false);
        return false;
    }

    std::vector<float> embeddings;
    if (!prepareLLMInput(prompt, embeddings))
    {
        inferenceComplete(false);
        return false;
    }

    const int N = 1;
    const int M = static_cast<int>(context_.embedDim);
    const int K = static_cast<int>(context_.vocabSize);

    float* layerWeightMatrix = getLayerWeights();
    if (!layerWeightMatrix)
    {
        inferenceComplete(false);
        return false;
    }

    RawrXD::Runtime::LaneGuard execLane(RawrXD::Runtime::SubsystemLane::Execute);
    if (!execLane.allowed())
    {
        inferenceComplete(false);
        return false;
    }
    RawrXD::Runtime::GenerationScope genScope("GGUFRunner", context_.modelPath);

    auto totalTimerStart = std::chrono::steady_clock::now();
    (void)totalTimerStart;

    const int maxTokens = std::max(1, context_.maxTokens > 0 ? context_.maxTokens : 64);
    size_t lastTokenId = 0;
    context_.lastDecodeSteps = 0;

    const bool logDecodeProgress = []
    {
        const char* e = std::getenv("RAWRXD_GGUF_DECODE_PROGRESS");
        return e && (e[0] == '1' || e[0] == 'y' || e[0] == 'Y');
    }();

    RawrXD::Logging::Logger::instance().info(
        std::string("[GGUFRunner] decode loop starting: maxTokens=") + std::to_string(maxTokens) +
            " nLayers=" + std::to_string(context_.nLayers) + " embedDim=" + std::to_string(context_.embedDim) +
            " vocabSize=" + std::to_string(context_.vocabSize) +
            " — first token may take a long time on large CPU-only models (this is not a hang). "
            "Set RAWRXD_GGUF_DECODE_PROGRESS=1 for per-step logs.",
        "GGUFRunner");

    for (int t = 0; t < maxTokens; ++t)
    {
        const auto stepWall0 = std::chrono::steady_clock::now();

        // Transformer forward (scalar) to produce logits
        if (context_.logits.size() != context_.vocabSize)
        {
            context_.logits.resize(context_.vocabSize);
        }

        std::vector<float> x(context_.embedDim);
        std::memcpy(x.data(), embeddings.data(), context_.embedDim * sizeof(float));
        for (size_t l = 0; l < context_.nLayers; ++l)
        {
            std::vector<float> attn(context_.embedDim);
            attentionForward(static_cast<int>(l), x.data(), attn.data());
            for (size_t i = 0; i < context_.embedDim; ++i)
                x[i] += attn[i];  // residual
            std::vector<float> ff(context_.embedDim);
            mlpForward(static_cast<int>(l), x.data(), ff.data());
            for (size_t i = 0; i < context_.embedDim; ++i)
                x[i] += ff[i];  // residual
        }
        // Final layernorm then logits projection
        std::vector<float> xnorm(context_.embedDim);
        layerNorm(x.data(), xnorm.data(), context_.ln_f_g, context_.ln_f_b, context_.embedDim);
        // Prefer raw Q4_0 output.weight if available (runtime-dispatched GEMM)
        if (!context_.raw_q4_output.empty())
        {
            if (context_.logits.size() != context_.vocabSize)
                context_.logits.resize(context_.vocabSize);
            ggml_gemm_q4_0(1, static_cast<int>(context_.vocabSize), static_cast<int>(context_.embedDim), xnorm.data(),
                           context_.raw_q4_output.data(), 1.0f, context_.logits.data());
        }
        else if (context_.tok_embeddings.size() == static_cast<size_t>(context_.vocabSize * context_.embedDim))
        {
            for (size_t v = 0; v < context_.vocabSize; ++v)
            {
                const float* Ev = context_.tok_embeddings.data() + v * context_.embedDim;
                float dot = 0.0f;
                for (size_t d = 0; d < context_.embedDim; ++d)
                    dot += xnorm[d] * Ev[d];
                context_.logits[v] = dot;
            }
        }
        else
        {
            // Use quantization-aware backend (ggml Q4_0/Q8_0 or fallback)
            QuantBackend::instance().matmul(xnorm.data(), layerWeightMatrix, context_.logits.data(), 1, M, K);
        }
        std::copy(context_.logits.cbegin(), context_.logits.cend(), outputBuffer);

        // Apply temperature before softmax for better distribution
        if (context_.temperature > 0.0f && std::abs(context_.temperature - 1.0f) > 0.001f)
        {
            applyTemperature(context_.logits.data(), context_.temperature);
        }

        applySoftmax(context_.logits.data());
        std::copy(context_.logits.cbegin(), context_.logits.cend(), outputBuffer);

        // Sample based on temperature setting
        size_t tokenId;
        if (context_.temperature < 0.01f)
        {
            tokenId = sampleGreedy(context_.logits.data());  // Greedy for temp ≈ 0
        }
        else if (context_.topP < 1.0f && context_.topP > 0.0f)
        {
            tokenId = sampleTopP(context_.logits.data(), context_.topP);  // Nucleus sampling
        }
        else
        {
            tokenId = sampleNextToken(context_.logits.data());  // Standard sampling
        }
        lastTokenId = tokenId;
        tokenChunkGenerated(decodeToken(tokenId));

        if (!embeddings.empty())
        {
            embeddings.back() = static_cast<float>(tokenId % 1024) / 1024.0f;
        }

        context_.lastDecodeSteps = t + 1;

        if (logDecodeProgress)
        {
            const auto stepWall1 = std::chrono::steady_clock::now();
            const double stepSec = std::chrono::duration<double>(stepWall1 - stepWall0).count();
            RawrXD::Logging::Logger::instance().info(std::string("[GGUFRunner] decode step ") + std::to_string(t + 1) +
                                                         "/" + std::to_string(maxTokens) +
                                                         " done wall_s=" + std::to_string(stepSec),
                                                     "GGUFRunner");
        }

        if (context_.eosTokenId >= 0 && static_cast<size_t>(context_.eosTokenId) == tokenId)
        {
            break;
        }

        // advance KV position
        context_.kvLen = std::min<size_t>(context_.kvLen + 1, static_cast<size_t>(context_.maxTokens - 1));
    }

    RawrXD::Logging::Logger::instance().info(std::string("[GGUFRunner] decode loop finished lastDecodeSteps=") +
                                                 std::to_string(context_.lastDecodeSteps),
                                             "GGUFRunner");

    inferenceComplete(true);
    return true;
}

void GGUFRunner::checkCpuFeatures()
{
    context_.hasAVX2 = false;
    context_.hasAVX512 = false;
    context_.hasFMA = false;

#if defined(__AVX2__)
    context_.hasAVX2 = true;
#endif

#if defined(__AVX512F__)
    context_.hasAVX512 = true;
#endif

#if defined(__FMA__)
    context_.hasFMA = true;
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0x7);
    if ((cpuInfo[1] & (1 << 5)) != 0)
    {
        context_.hasAVX2 = true;
    }
    if ((cpuInfo[1] & (1 << 16)) != 0)
    {
        context_.hasAVX512 = true;
    }
    if ((cpuInfo[1] & (1 << 12)) != 0)
    {
        context_.hasFMA = true;
    }
#endif

    // CPU features detected (AVX512, FMA)
}

void GGUFRunner::loadGGUFModel(const std::string& filePath)
{
    const std::string previousResident = context_.modelPath;
    if (!previousResident.empty() && previousResident != filePath)
    {
        RawrXD::Runtime::ModelRuntimeGate::instance().notifyModelUnloaded(previousResident);
    }
    // Tear down prior mmap/heap mirror and drop parsed weights so a new file cannot inherit stale layers
    // (would otherwise risk 0xC0000005 in attentionForward).
    releaseWeightFileBacking();
    context_.layers.clear();
    context_.tok_embeddings.clear();
    context_.output_norm_w.clear();
    context_.output_w.clear();
    context_.raw_q4_output.clear();
    context_.raw_q8_output.clear();
    context_.ln_f_g.clear();
    context_.ln_f_b.clear();
    context_.keyCache.clear();
    context_.valueCache.clear();
    context_.tensorTable.clear();
    context_.tensorDataBaseOffset = 0;
    context_.ggufTensorAlignment = 32;
    context_.ffnDim = 0;
    context_.kvLen = 0;

    context_.modelPath = filePath;
    context_.embedDim = 0;
    context_.vocabSize = 0;
    context_.nLayers = 0;
    context_.nHeads = 0;
    context_.nKVHeads = 0;
    context_.architecture.clear();
    context_.modelName.clear();
    context_.usesMmap = false;

    RawrXD::NativeFile file(filePath);
    // Missing file: do not allocate new float[0] (was non-null on MSVC and looked "loaded").
    if (!file.exists())
    {
        return;
    }
    if (!file.open())
    {
        return;
    }

    context_.modelFileSize = file.size();
    if (context_.modelFileSize < 16)
    {
        return;
    }

#if defined(_WIN32)
    // Optional: copy the full GGUF into UnifiedMemoryExecutor's host arena so tensor reads use memcpy from
    // unifiedFileBase (see readTensorData). Large files exceed the ~448 MiB default arena — we fall back to mmap.
    const char* unifiedGgufEnv = std::getenv("RAWRXD_GGUF_USE_UNIFIED_MEMORY");
    const bool wantGgufUnified = unifiedGgufEnv != nullptr && unifiedGgufEnv[0] != '\0' &&
                                 (unifiedGgufEnv[0] == '1' || unifiedGgufEnv[0] == 'y' || unifiedGgufEnv[0] == 'Y' ||
                                  unifiedGgufEnv[0] == 't' || unifiedGgufEnv[0] == 'T');
    if (wantGgufUnified)
    {
        const uint64_t ggufBytesU64 = static_cast<uint64_t>(context_.modelFileSize);
        auto& um = RawrXD::UnifiedMemory::UnifiedMemoryExecutor::instance();
        (void)um.initialize();
        // loadModelUnified uses allocate(..., 64 KiB alignment); reserve slop for align-up waste.
        constexpr uint64_t kUnifiedGgufSlop = 65536u * 2u;
        if (um.isInitialized() && ggufBytesU64 > 0 &&
            um.getUnifiedHeapRemainingBytes() >= ggufBytesU64 + kUnifiedGgufSlop)
        {
            auto ub = um.loadModelUnified(filePath, ggufBytesU64);
            if (ub)
            {
                context_.unifiedModelBuffer = ub.value();
                context_.unifiedFileBase = context_.unifiedModelBuffer.ptr;
                context_.unifiedFileBytes = ggufBytesU64;
                context_.usesUnifiedFileBacking = true;
                context_.mappedData = nullptr;
                context_.usesMmap = false;
                RawrXD::Logging::Logger::instance().info(
                    std::string("[GGUFRunner] GGUF bytes resident in UnifiedMemoryExecutor "
                                "(RAWRXD_GGUF_USE_UNIFIED_MEMORY): ") +
                        std::to_string(ggufBytesU64) + " bytes",
                    "GGUFRunner");
            }
            else
            {
                RawrXD::Logging::Logger::instance().warning(
                    std::string("[GGUFRunner] UnifiedMemoryExecutor::loadModelUnified failed for ") + filePath +
                        " — falling back to map / heap.",
                    "GGUFRunner");
            }
        }
        else
        {
            RawrXD::Logging::Logger::instance().warning(
                std::string("[GGUFRunner] GGUF (") + std::to_string(ggufBytesU64) +
                    " B) does not fit unified heap remaining (" +
                    std::to_string(um.isInitialized() ? um.getUnifiedHeapRemainingBytes() : 0) +
                    " B); using standard file backing. Unset RAWRXD_GGUF_USE_UNIFIED_MEMORY or use a smaller GGUF.",
                "GGUFRunner");
        }
    }
#endif

    {
        RawrXD::BinaryStream ds(file.getStream());
        ds.setByteOrderLittleEndian();
        GGUFHeader h{};
        ds >> h.magic >> h.version >> h.tensorCount >> h.kvCount;

        if (h.magic != 0x46554747)
        {  // 'GGUF' little-endian
            return;
        }
        context_.ggufVersion = h.version;

        ingestGgufKvMetadata(ds, h.kvCount);
        if (ds.status() != RawrXD::StreamStatus::Ok)
        {
            RawrXD::Logging::Logger::instance().warning(
                std::string("[GGUFRunner] GGUF KV metadata read ended with non-Ok stream status (truncated or "
                            "corrupt header?): ") +
                    filePath,
                "GGUFRunner");
        }

        file.seek(0);
        size_t headLen = (std::min)(static_cast<size_t>(context_.modelFileSize), static_cast<size_t>(8 * 1024 * 1024));
        std::vector<uint8_t> headBuf = file.read(static_cast<int>(headLen));
        std::string head(reinterpret_cast<const char*>(headBuf.data()), headBuf.size());

        auto findInt = [&](const char* key, int defVal)
        {
            size_t idx = head.find(key);
            if (idx == std::string::npos)
                return defVal;
            size_t nl = head.find('\n', idx);
            size_t lineLen = (nl != std::string::npos && nl > idx) ? (nl - idx) : 128;
            std::string line = head.substr(idx, lineLen);
            size_t eq = line.find('=');
            if (eq == std::string::npos)
                return defVal;
            std::string val = line.substr(eq + 1);
            size_t start = val.find_first_not_of(" \t\r\n");
            if (start != std::string::npos)
                val = val.substr(start);
            try
            {
                return std::stoi(val);
            }
            catch (...)
            {
                return defVal;
            }
        };

        auto findString = [&](const char* key) -> std::string
        {
            size_t idx = head.find(key);
            if (idx == std::string::npos)
                return std::string();
            size_t nl = head.find('\n', idx);
            size_t lineLen = (nl != std::string::npos && nl > idx) ? (nl - idx) : 128;
            std::string line = head.substr(idx, lineLen);
            size_t eq = line.find('=');
            if (eq == std::string::npos)
                return std::string();
            std::string val = line.substr(eq + 1);
            size_t start = val.find_first_not_of(" \t\r\n");
            if (start != std::string::npos)
                val = val.substr(start);
            return val;
        };

        if (context_.embedDim == 0)
        {
            context_.embedDim = static_cast<size_t>(std::max(0, findInt("ggml.embedding_length", 4096)));
        }
        if (context_.vocabSize == 0)
        {
            context_.vocabSize = static_cast<size_t>(std::max(0, findInt("ggml.vocab_size", 32000)));
        }
        if (context_.nLayers == 0)
        {
            context_.nLayers = static_cast<size_t>(std::max(0, findInt("llama.block_count", 32)));
        }
        if (context_.nHeads == 0)
        {
            context_.nHeads = static_cast<size_t>(std::max(0, findInt("llama.attention.head_count", 32)));
        }
        if (context_.nKVHeads == 0)
        {
            context_.nKVHeads = context_.nHeads;
        }

        if (context_.modelName.empty())
        {
            context_.modelName = findString("general.name");
        }
        if (context_.architecture.empty())
        {
            context_.architecture = findString("general.architecture");
        }
        std::string quantStr = findString("general.file_type");

        // Detect quantization type (case-insensitive)
        std::string q(quantStr);
        for (auto& c : q)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string fp(filePath);
        for (auto& c : fp)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (q.find("q4_0") != std::string::npos || fp.find("q4_0") != std::string::npos)
        {
            context_.quantType = QuantType::Q4_0;
        }
        else if (q.find("q4_1") != std::string::npos)
        {
            context_.quantType = QuantType::Q4_1;
        }
        else if (q.find("q8_0") != std::string::npos)
        {
            context_.quantType = QuantType::Q8_0;
        }
        else if (q.find("f16") != std::string::npos)
        {
            context_.quantType = QuantType::F16;
        }

        if (context_.embedDim <= 0)
        {
            context_.embedDim = 4096;
        }
        if (context_.vocabSize <= 0)
        {
            context_.vocabSize = 32000;
        }

        sanitizeArchMetadata();

        if (context_.embedDim <= 0)
        {
            context_.embedDim = 4096;
        }
        if (context_.vocabSize <= 0)
        {
            context_.vocabSize = 32000;
        }
        if (context_.nHeads == 0 && context_.embedDim > 0)
        {
            context_.nHeads = 32;
            if (context_.nKVHeads == 0)
            {
                context_.nKVHeads = context_.nHeads;
            }
        }

        // Multi-head attention parameters
        context_.headDim = (context_.nHeads > 0) ? (context_.embedDim / context_.nHeads) : 128;
        context_.ropeBase = 10000.0f;  // RoPE frequency base (standard LLaMA default)

        // Precompute inverse frequencies for RoPE (once per model)
        if (context_.headDim > 0)
        {
            context_.invFreq.resize(context_.headDim / 2);
            for (int i = 0; i < context_.headDim / 2; ++i)
            {
                context_.invFreq[i] =
                    1.0f / std::pow(context_.ropeBase, 2.0f * i / static_cast<float>(context_.headDim));
            }
        }

        file.seek(0);
    }

#if USE_MMAP
    if (!context_.usesUnifiedFileBacking)
    {
        void* mapped =
            ::mmap(nullptr, static_cast<size_t>(context_.modelFileSize), PROT_READ, MAP_PRIVATE, file.handle(), 0);
        if (mapped == MAP_FAILED)
        {
        }
        else
        {
            context_.mappedData = static_cast<float*>(mapped);
            context_.usesMmap = true;
        }
    }
#endif

#if defined(_WIN32)
    // Windows: map the GGUF read-only (same role as mmap on Unix). Without this, the code below
    // heap-mirrors the entire file as float[] — tens of GB for large checkpoints → OOM/abort right
    // after generation starts (TpsSmoke shows beginGeneration then silent exit).
    if (!context_.mappedData && !context_.usesUnifiedFileBacking)
    {
        const std::filesystem::path winPath(filePath);
        const std::wstring& wpath = winPath.native();
        HANDLE hf = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE)
        {
            HANDLE hm = CreateFileMappingW(hf, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (hm != nullptr)
            {
                void* view = MapViewOfFile(hm, FILE_MAP_READ, 0, 0, 0);
                if (view != nullptr)
                {
                    context_.win32FileHandle = hf;
                    context_.win32MapHandle = hm;
                    context_.win32MapView = view;
                    context_.mappedData = static_cast<float*>(view);
                    context_.usesMmap = true;
                }
                else
                {
                    CloseHandle(hm);
                    CloseHandle(hf);
                }
            }
            else
            {
                CloseHandle(hf);
            }
        }
    }
#endif

    if (!context_.mappedData && !context_.usesUnifiedFileBacking)
    {
        constexpr size_t kMaxHeapMirrorBytes = 512u * 1024u * 1024u;
        if (static_cast<size_t>(context_.modelFileSize) > kMaxHeapMirrorBytes)
        {
            RawrXD::Logging::Logger::instance().error(
                std::string("[GGUFRunner] Model is ") + std::to_string(context_.modelFileSize) +
                    " bytes: refusing heap mirror (Win32 map/unix mmap unavailable). "
                    "Large GGUFs require a successful memory map.",
                "GGUFRunner");
            context_.modelPath.clear();
            releaseWeightFileBacking();
            return;
        }
        const size_t floatCount = static_cast<size_t>(context_.modelFileSize / sizeof(float));
        context_.mappedData = new float[floatCount];
        size_t totalRead = 0;
        const size_t chunk = 64 * 1024 * 1024;  // 64MB
        while (totalRead < static_cast<size_t>(context_.modelFileSize))
        {
            size_t toRead = (std::min)(static_cast<size_t>(context_.modelFileSize) - totalRead, chunk);
            int n = file.read(reinterpret_cast<char*>(context_.mappedData) + totalRead, static_cast<int>(toRead));
            if (n <= 0)
                break;
            totalRead += static_cast<size_t>(n);
        }
    }

    // Build tensor directory and read embeddings + per-layer llama `blk.*` tensors (when present / supported).
    if (!parseGgufTensorTable(file) || !parseGgufTensors(file))
    {
    }
    else
    {
        if (parseGgufLayerWeights(file))
        {
            RawrXD::Logging::Logger::instance().info(
                std::string("[GGUFRunner] blk.* weights loaded: n_layers=") + std::to_string(context_.layers.size()) +
                    " embed_dim=" + std::to_string(context_.embedDim) + " ffn_dim=" + std::to_string(context_.ffnDim) +
                    " inference_ready=" + (inferenceWeightsReady() ? "yes" : "no"),
                "GGUFRunner");
        }
        else
        {
            RawrXD::Logging::Logger::instance().warning(
                std::string("[GGUFRunner] parseGgufLayerWeights failed for ") + filePath +
                    " — missing blk.* tensors, unsupported quant in GgufTensorBytes, or GQA/FFN shape mismatch. "
                    "runInference will refuse (weights_incomplete) until loads succeed.",
                "GGUFRunner");
        }
    }

    // Allocate KV-cache for multi-head GQA: [nLayers, nKVHeads, maxTokens, headDim]
    if (context_.nLayers > 0 && context_.nKVHeads > 0 && context_.headDim > 0)
    {
        size_t cacheSize = static_cast<size_t>(context_.nLayers) * static_cast<size_t>(context_.nKVHeads) *
                           static_cast<size_t>(context_.maxTokens) * static_cast<size_t>(context_.headDim);
        context_.keyCache.resize(cacheSize, 0.0f);
        context_.valueCache.resize(cacheSize, 0.0f);
        context_.kvLen = 0;
    }

    // NativeFile closes on destruction
    loadVocabulary(filePath + ".vocab");
    if (context_.vocabulary.empty())
    {
        context_.vocabulary.reserve(static_cast<size_t>(context_.vocabSize));
        for (size_t i = 0; i < static_cast<size_t>(context_.vocabSize); ++i)
        {
            context_.vocabulary.push_back("<%1>");
        }
    }
}

void GGUFRunner::loadVocabulary(const std::string& vocabPath)
{
    context_.vocabulary.clear();

    RawrXD::NativeFile vocabFile(vocabPath);
    if (!vocabFile.exists())
    {
        return;
    }

    if (!vocabFile.open())
    {
        return;
    }

    std::string all = vocabFile.readAll();
    size_t pos = 0;
    for (;;)
    {
        size_t nl = all.find('\n', pos);
        if (nl == std::string::npos)
        {
            if (pos < all.size())
                context_.vocabulary.push_back(all.substr(pos));
            break;
        }
        context_.vocabulary.push_back(all.substr(pos, nl - pos));
        pos = nl + 1;
    }
}

float* GGUFRunner::getLayerWeights()
{
    if (context_.mappedData != nullptr)
    {
        return context_.mappedData;
    }
    if (context_.usesUnifiedFileBacking && context_.unifiedFileBase != nullptr)
    {
        return reinterpret_cast<float*>(context_.unifiedFileBase);
    }
    return nullptr;
}

bool GGUFRunner::prepareLLMInput(const std::string& prompt, std::vector<float>& embeddings)
{
    if (context_.embedDim <= 0)
    {
        return false;
    }

    embeddings.assign(static_cast<size_t>(context_.embedDim), 0.0f);

    const std::vector<uint8_t> utf8(prompt.begin(), prompt.end());
    const int limit = std::min<int>(utf8.size(), static_cast<int>(context_.embedDim));
    for (int i = 0; i < limit; ++i)
    {
        const float v = static_cast<unsigned char>(utf8.at(i)) / 255.0f;
        const float pos = static_cast<float>(i) / static_cast<float>(context_.embedDim);
        embeddings[static_cast<size_t>(i)] = v + 0.01f * pos;
    }

    return true;
}

void GGUFRunner::applySoftmax(float* buffer)
{
    if (!buffer || context_.vocabSize == 0)
    {
        return;
    }

    float maxVal = buffer[0];
    for (size_t i = 1; i < context_.vocabSize; ++i)
    {
        if (buffer[i] > maxVal)
        {
            maxVal = buffer[i];
        }
    }

    float sumExp = 0.0f;
    for (size_t i = 0; i < context_.vocabSize; ++i)
    {
        buffer[i] = std::exp(buffer[i] - maxVal);
        sumExp += buffer[i];
    }

    if (sumExp <= 0.0f)
    {
        return;
    }

    const float invSum = 1.0f / sumExp;
    for (size_t i = 0; i < context_.vocabSize; ++i)
    {
        buffer[i] *= invSum;
    }
}

size_t GGUFRunner::sampleNextToken(float* buffer)
{
    if (!buffer || context_.vocabSize == 0)
    {
        return 0;
    }

    float maxProb = buffer[0];
    size_t bestIdx = 0;
    for (size_t i = 1; i < context_.vocabSize; ++i)
    {
        if (buffer[i] > maxProb)
        {
            maxProb = buffer[i];
            bestIdx = static_cast<size_t>(i);
        }
    }

    return bestIdx;
}

std::string GGUFRunner::decodeToken(size_t tokenId) const
{
    if (!context_.vocabulary.empty() && tokenId < static_cast<size_t>(context_.vocabulary.size()))
    {
        return context_.vocabulary[static_cast<size_t>(tokenId)];
    }

    return "<token_" + std::to_string(tokenId) + ">";
}

void GGUFRunner::applyTemperature(float* buffer, float temperature)
{
    if (!buffer || context_.vocabSize == 0 || temperature <= 0.0f)
    {
        return;
    }

    if (std::abs(temperature - 1.0f) < 0.001f)
    {
        return;  // No-op for temperature = 1.0
    }

    for (size_t i = 0; i < context_.vocabSize; ++i)
    {
        buffer[i] /= temperature;
    }
}

size_t GGUFRunner::sampleTopP(float* buffer, float topP)
{
    if (!buffer || context_.vocabSize == 0)
    {
        return 0;
    }

    // Create index-probability pairs
    std::vector<std::pair<size_t, float>> sorted;
    sorted.reserve(context_.vocabSize);
    for (size_t i = 0; i < context_.vocabSize; ++i)
    {
        sorted.emplace_back(i, buffer[i]);
    }

    // Sort descending by probability
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    // Accumulate until we reach topP
    float cumSum = 0.0f;
    size_t cutoff = 0;
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        cumSum += sorted[i].second;
        cutoff = i + 1;
        if (cumSum >= topP)
        {
            break;
        }
    }

    // Sample from top candidates (STL RNG — no framework dependency)
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(rng) * cumSum;
    float acc = 0.0f;
    for (size_t i = 0; i < cutoff; ++i)
    {
        acc += sorted[i].second;
        if (acc >= r)
        {
            return sorted[i].first;
        }
    }

    return sorted[0].first;  // Fallback to top token
}

size_t GGUFRunner::sampleGreedy(float* buffer)
{
    if (!buffer || context_.vocabSize == 0)
    {
        return 0;
    }

    float maxProb = buffer[0];
    size_t bestIdx = 0;
    for (size_t i = 1; i < context_.vocabSize; ++i)
    {
        if (buffer[i] > maxProb)
        {
            maxProb = buffer[i];
            bestIdx = static_cast<size_t>(i);
        }
    }
    return bestIdx;
}

// ---- Scalar transformer helpers ----
void GGUFRunner::layerNorm(const float* x, float* y, const std::vector<float>& gamma, const std::vector<float>& beta,
                           size_t dim)
{
    float mean = 0.0f;
    for (size_t i = 0; i < dim; ++i)
        mean += x[i];
    mean /= static_cast<float>(dim);
    float var = 0.0f;
    for (size_t i = 0; i < dim; ++i)
    {
        float d = x[i] - mean;
        var += d * d;
    }
    var /= static_cast<float>(dim);
    float invStd = 1.0f / std::sqrt(var + 1e-5f);
    for (size_t i = 0; i < dim; ++i)
    {
        float n = (x[i] - mean) * invStd;
        float g = gamma.empty() ? 1.0f : gamma[static_cast<size_t>(i)];
        float b = beta.empty() ? 0.0f : beta[static_cast<size_t>(i)];
        y[i] = n * g + b;
    }
}

void GGUFRunner::matmul(const float* A, const float* B, float* C, int N, int M, int K)
{
#ifdef GGUF_USE_AVX2
    // Runtime dispatch: use AVX2 if available, otherwise fall back to scalar
    if (context_.hasAVX2)
    {
        // Use the optimized AVX2 micro-kernel when available
        // Signature: matmul_kernel_avx2(A[NxM], B[MxK], C[NxK], N, M, K, accumulate)
        matmul_kernel_avx2(const_cast<float*>(A), const_cast<float*>(B), C, N, M, K, false);
        return;
    }
#endif

    // Scalar fallback path (when AVX2 not available or not enabled)
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < K; ++j)
        {
            float s = 0.0f;
            for (int k = 0; k < M; ++k)
                s += A[i * M + k] * B[k * K + j];
            C[i * K + j] = s;
        }
    }
}

void GGUFRunner::attentionForward(int layerIdx, const float* x, float* y)
{
    const size_t D = context_.embedDim;
    const size_t nHead = context_.nHeads;
    const size_t nKVHead = context_.nKVHeads;
    const size_t headDim = context_.headDim;

    const size_t kvDim = nKVHead * headDim;

    if (nHead == 0 || headDim == 0 || nKVHead == 0)
    {
        std::fill_n(y, D, 0.0f);
        return;
    }

    // Multi-head attention with GQA and RoPE
    std::vector<float> n(D), q(D), k(kvDim), v(kvDim);
    const auto& L = context_.layers[static_cast<size_t>(layerIdx)];
    layerNorm(x, n.data(), L.ln_1_g, L.ln_1_b, D);
    matmul(n.data(), L.attn_q_w.data(), q.data(), 1, static_cast<int>(D), static_cast<int>(D));
    matmul(n.data(), L.attn_k_w.data(), k.data(), 1, static_cast<int>(D), static_cast<int>(kvDim));
    matmul(n.data(), L.attn_v_w.data(), v.data(), 1, static_cast<int>(D), static_cast<int>(kvDim));

    // Apply RoPE to Q and K (in-place rotation per head)
    size_t pos = context_.kvLen;
    auto rotate = [&](float* vec, int head, size_t position)
    {
        float* h = vec + head * headDim;
        for (int i = 0; i < headDim; i += 2)
        {
            float fcr = std::cos(static_cast<float>(position) * context_.invFreq[i / 2]);
            float fsi = std::sin(static_cast<float>(position) * context_.invFreq[i / 2]);
            float v0 = h[i];
            float v1 = h[i + 1];
            h[i] = v0 * fcr - v1 * fsi;
            h[i + 1] = v0 * fsi + v1 * fcr;
        }
    };

    for (int h = 0; h < nHead; ++h)
        rotate(q.data(), h, pos);
    for (int h = 0; h < nKVHead; ++h)
        rotate(k.data(), h, pos);

    // Store K/V in cache: [nLayers, nKVHeads, maxTokens, headDim]
    size_t cacheHeadStride = static_cast<size_t>(context_.maxTokens) * static_cast<size_t>(headDim);
    size_t cacheLayerStride = static_cast<size_t>(nKVHead) * cacheHeadStride;
    for (int kvh = 0; kvh < nKVHead; ++kvh)
    {
        float* Kc = context_.keyCache.data() + layerIdx * cacheLayerStride + kvh * cacheHeadStride + pos * headDim;
        float* Vc = context_.valueCache.data() + layerIdx * cacheLayerStride + kvh * cacheHeadStride + pos * headDim;
        std::memcpy(Kc, k.data() + kvh * headDim, static_cast<size_t>(headDim) * sizeof(float));
        std::memcpy(Vc, v.data() + kvh * headDim, static_cast<size_t>(headDim) * sizeof(float));
    }

    // Multi-head attention loop with GQA mapping
    std::vector<float> attnOut(D, 0.0f);
    std::vector<float> logits(pos + 1);
    float scale = 1.0f / std::sqrt(static_cast<float>(headDim));

    for (int h = 0; h < nHead; ++h)
    {
        int kvH = h * nKVHead / nHead;  // GQA mapping: multiple query heads share one KV head
        float* qHead = q.data() + h * headDim;

        // Compute attention scores for this head
        for (size_t t = 0; t <= pos; ++t)
        {
            const float* Kt =
                context_.keyCache.data() + layerIdx * cacheLayerStride + kvH * cacheHeadStride + t * headDim;
            float score = 0.0f;
            for (int d = 0; d < headDim; ++d)
                score += qHead[d] * Kt[d];
            logits[t] = score * scale;
        }

        // Softmax over logits
        float maxScore = logits[0];
        for (size_t t = 1; t <= pos; ++t)
            if (logits[t] > maxScore)
                maxScore = logits[t];
        float sumExp = 0.0f;
        for (size_t t = 0; t <= pos; ++t)
        {
            logits[t] = std::exp(logits[t] - maxScore);
            sumExp += logits[t];
        }
        for (size_t t = 0; t <= pos; ++t)
            logits[t] /= (sumExp + 1e-9f);

        // Accumulate weighted values into output
        for (int d = 0; d < headDim; ++d)
        {
            float acc = 0.0f;
            for (size_t t = 0; t <= pos; ++t)
            {
                const float* Vt =
                    context_.valueCache.data() + layerIdx * cacheLayerStride + kvH * cacheHeadStride + t * headDim;
                acc += logits[t] * Vt[d];
            }
            attnOut[h * headDim + d] = acc;
        }
    }

    // Output projection
    matmul(attnOut.data(), L.attn_o_w.data(), y, 1, static_cast<int>(D), static_cast<int>(D));
}

void GGUFRunner::mlpForward(int layerIdx, const float* x, float* y)
{
    const size_t D = context_.embedDim;
    const size_t F = context_.ffnDim;
    if (F == 0)
    {
        return;
    }
    const auto& L = context_.layers[static_cast<size_t>(layerIdx)];
    std::vector<float> n(D);
    layerNorm(x, n.data(), L.ln_2_g, L.ln_2_b, D);
    std::vector<float> up(F), gate(F), act(F);
    matmul(n.data(), L.mlp_up_w.data(), up.data(), 1, static_cast<int>(D), static_cast<int>(F));
    matmul(n.data(), L.mlp_gate_w.data(), gate.data(), 1, static_cast<int>(D), static_cast<int>(F));
    for (size_t i = 0; i < F; ++i)
    {
        const float s = 1.0f / (1.0f + std::exp(-gate[i]));
        act[i] = up[i] * (gate[i] * s);
    }
    matmul(act.data(), L.mlp_down_w.data(), y, 1, static_cast<int>(F), static_cast<int>(D));
}

void GGUFRunner::fallback_matrix_multiply(float* A, float* B, float* C, int N, int M, int K)
{
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < K; ++j)
        {
            float sum = 0.0f;
            for (int k = 0; k < M; ++k)
            {
                sum += A[i * M + k] * B[k * K + j];
            }
            C[i * K + j] = sum;
        }
    }
}

bool GGUFRunner::loadModel(const std::string& filePath)
{
    loadGGUFModel(filePath);
    if (hasModelFileBacking())
    {
        RawrXD::Runtime::ModelRuntimeGate::instance().notifyModelResident(
            filePath, static_cast<std::uint64_t>(context_.modelFileSize));
        modelLoaded(filePath, context_.modelFileSize);
        return true;
    }
    return false;
}

size_t GGUFRunner::ggmlTypeSize(GgmlType type)
{
    switch (type)
    {
        case GgmlType::F32:
            return 4;  // 4 bytes per element
        case GgmlType::F16:
            return 2;  // 2 bytes per element
        case GgmlType::Q4_0:
            return 18;  // 18 bytes per 32-element block
        case GgmlType::Q8_0:
            return 34;  // 34 bytes per 32-element block
        case GgmlType::Q8_K:
            return sizeof(float) + 256 + 16 * sizeof(int16_t);  // 292 bytes per 256-element block
        default:
            return 4;
    }
}

std::vector<uint8_t> GGUFRunner::readTensorData(RawrXD::NativeFile& file, uint64_t offset, uint64_t numBytes)
{
#if defined(_WIN32)
    if (context_.usesUnifiedFileBacking && context_.unifiedFileBase != nullptr && context_.unifiedFileBytes > 0)
    {
        if (numBytes > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            return std::vector<uint8_t>();
        }
        const uint64_t end = offset + numBytes;
        if (end < offset || end > context_.unifiedFileBytes)
        {
            return std::vector<uint8_t>();
        }
        std::vector<uint8_t> out(static_cast<size_t>(numBytes));
        std::memcpy(out.data(), static_cast<const std::uint8_t*>(context_.unifiedFileBase) + offset,
                    static_cast<size_t>(numBytes));
        return out;
    }
#endif
    // Full file is contiguous: POSIX mmap, Win32 MapViewOfFile, or small-file heap mirror. Avoid seek+read I/O.
    if (context_.mappedData != nullptr && !context_.usesUnifiedFileBacking && context_.modelFileSize > 0)
    {
        if (numBytes > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            return std::vector<uint8_t>();
        }
        const uint64_t fileSz = static_cast<uint64_t>(context_.modelFileSize);
        const uint64_t end = offset + numBytes;
        if (end < offset || end > fileSz)
        {
            return std::vector<uint8_t>();
        }
        std::vector<uint8_t> out(static_cast<size_t>(numBytes));
        const auto* base = reinterpret_cast<const std::uint8_t*>(context_.mappedData);
        std::memcpy(out.data(), base + offset, static_cast<size_t>(numBytes));
        return out;
    }

    if (!file.seek(static_cast<size_t>(offset)))
        return std::vector<uint8_t>();
    return file.read(static_cast<int>(numBytes));
}

bool GGUFRunner::loadTensor(RawrXD::NativeFile& file, const std::string& name, std::vector<float>& weights)
{
    if (context_.tensorTable.find(name) == context_.tensorTable.end())
    {
        return false;
    }
    const auto& desc = context_.tensorTable[name];
    size_t totalElements = 1;
    for (auto dim : desc.dims)
    {
        totalElements *= static_cast<size_t>(dim);
    }

    size_t numBytes = 0;
    const uint32_t typeU = static_cast<uint32_t>(desc.type);
    if (!RawrXD::GgufTensorBytes::payloadBytes(typeU, totalElements, numBytes))
    {
        return false;
    }

    std::vector<uint8_t> rawData = readTensorData(file, desc.offset, numBytes);
    if (rawData.size() != numBytes)
    {
        return false;
    }

    weights.resize(totalElements);
    if (!RawrXD::GgufTensorBytes::dequantizeToFloat(typeU, rawData.data(), totalElements, weights.data()))
    {
        return false;
    }
    if (name == "output.weight" && desc.type == GgmlType::Q4_0)
    {
        context_.raw_q4_output.assign(rawData.data(), rawData.data() + rawData.size());
    }
    return true;
}

bool GGUFRunner::parseGgufTensors(RawrXD::NativeFile& file)
{
    // Load essential tensors using table-driven approach
    if (!loadTensor(file, "token_embd.weight", context_.tok_embeddings))
    {
        return false;
    }

    // Load output norm and output weights
    if (!loadTensor(file, "output_norm.weight", context_.output_norm_w))
    {
    }
    if (!loadTensor(file, "output.weight", context_.output_w))
    {
    }

    return true;
}

namespace
{

/// Upper-bound float32 RAM for full per-layer dequant (env caps huge models before `std::bad_alloc`).
bool ggufEstimatedLayerFloatMirrorWithinRamBudget(size_t nLayers, size_t d, size_t kvDim, size_t ffnDim)
{
    const char* e = std::getenv("RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB");
    if (!e || !e[0])
    {
        return true;
    }
    char* end = nullptr;
    const double maxGb = std::strtod(e, &end);
    if (end == e || !(maxGb > 0.0))
    {
        return true;
    }
    const double qo = 2.0 * static_cast<double>(d) * static_cast<double>(d);
    const double kv = 2.0 * static_cast<double>(d) * static_cast<double>(kvDim);
    const double mlp = 3.0 * static_cast<double>(d) * static_cast<double>(ffnDim);
    const double norms = 2.0 * static_cast<double>(d);
    const double perLayer = (qo + kv + mlp + norms) * static_cast<double>(sizeof(float));
    const double totalBytes = perLayer * static_cast<double>(nLayers);
    const double cap = maxGb * 1024.0 * 1024.0 * 1024.0;
    return totalBytes <= cap;
}

}  // namespace

bool GGUFRunner::parseGgufLayerWeights(RawrXD::NativeFile& file)
{
    if (context_.nLayers == 0 || context_.embedDim == 0 || context_.tensorTable.empty())
    {
        return false;
    }

    auto inferFfnDim = [&]()
    {
        if (context_.ffnDim != 0)
        {
            return;
        }
        const auto it = context_.tensorTable.find("blk.0.ffn_gate.weight");
        if (it == context_.tensorTable.end())
        {
            return;
        }
        const auto& d = it->second.dims;
        if (d.size() >= 2)
        {
            const size_t a = static_cast<size_t>(d[0]);
            const size_t b = static_cast<size_t>(d[1]);
            const size_t em = context_.embedDim;
            if (a == em)
            {
                context_.ffnDim = b;
            }
            else if (b == em)
            {
                context_.ffnDim = a;
            }
        }
    };
    inferFfnDim();

    {
        const size_t d = context_.embedDim;
        const size_t kvD = context_.nKVHeads * context_.headDim;
        size_t ffnEst = context_.ffnDim;
        if (ffnEst == 0)
        {
            const auto itg = context_.tensorTable.find("blk.0.ffn_gate.weight");
            if (itg != context_.tensorTable.end() && itg->second.dims.size() >= 2)
            {
                const size_t a = static_cast<size_t>(itg->second.dims[0]);
                const size_t b = static_cast<size_t>(itg->second.dims[1]);
                if (a == d)
                {
                    ffnEst = b;
                }
                else if (b == d)
                {
                    ffnEst = a;
                }
            }
        }
        if (ffnEst == 0)
        {
            ffnEst = d * 4;
        }
        if (!ggufEstimatedLayerFloatMirrorWithinRamBudget(context_.nLayers, d, kvD, ffnEst))
        {
            RawrXD::Logging::Logger::instance().warning(
                std::string("[GGUFRunner] parseGgufLayerWeights skipped: estimated float32 layer mirror exceeds "
                            "RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB; unset env to allow (may OOM). Path: ") +
                    context_.modelPath,
                "GGUFRunner");
            return false;
        }
    }

    context_.layers.clear();
    context_.layers.resize(context_.nLayers);

    for (size_t li = 0; li < context_.nLayers; ++li)
    {
        const std::string pre = "blk." + std::to_string(li) + ".";
        ModelContext::Layer& L = context_.layers[li];

        (void)loadTensor(file, pre + "attn_norm.weight", L.ln_1_g);
        (void)loadTensor(file, pre + "ffn_norm.weight", L.ln_2_g);

        if (!loadTensor(file, pre + "attn_q.weight", L.attn_q_w))
        {
            context_.layers.clear();
            return false;
        }
        if (!loadTensor(file, pre + "attn_k.weight", L.attn_k_w))
        {
            context_.layers.clear();
            return false;
        }
        if (!loadTensor(file, pre + "attn_v.weight", L.attn_v_w))
        {
            context_.layers.clear();
            return false;
        }
        if (!loadTensor(file, pre + "attn_output.weight", L.attn_o_w))
        {
            context_.layers.clear();
            return false;
        }
        if (!loadTensor(file, pre + "ffn_gate.weight", L.mlp_gate_w))
        {
            context_.layers.clear();
            return false;
        }
        if (!loadTensor(file, pre + "ffn_up.weight", L.mlp_up_w))
        {
            context_.layers.clear();
            return false;
        }
        if (!loadTensor(file, pre + "ffn_down.weight", L.mlp_down_w))
        {
            context_.layers.clear();
            return false;
        }

        if (li == 0 && context_.ffnDim == 0 && context_.embedDim > 0 && !L.mlp_gate_w.empty() &&
            L.mlp_gate_w.size() % context_.embedDim == 0)
        {
            context_.ffnDim = L.mlp_gate_w.size() / context_.embedDim;
        }
    }

    return true;
}

bool GGUFRunner::readTensorFloat32(RawrXD::NativeFile& file, int64_t offset, int64_t count, std::vector<float>& out)
{
    // 1. Look up the tensor that owns this byte range (exact offset match)
    const ModelContext::TensorDesc* desc = nullptr;
    for (const auto& kv : context_.tensorTable)
    {
        if (kv.second.offset == static_cast<uint64_t>(offset))
        {
            desc = &kv.second;
            break;
        }
    }
    if (!desc)
    {
        return false;
    }

    // 2. Compute element count from shape
    uint64_t expect = 1;
    for (uint64_t dim : desc->dims)
        expect *= dim;
    if (expect != static_cast<uint64_t>(count))
    {
        return false;
    }

    size_t byteSize = 0;
    if (!RawrXD::GgufTensorBytes::payloadBytes(static_cast<uint32_t>(desc->type), static_cast<size_t>(expect),
                                               byteSize))
    {
        return false;
    }

    std::vector<uint8_t> raw = readTensorData(file, static_cast<uint64_t>(offset), byteSize);
    if (raw.size() != byteSize)
    {
        return false;
    }

    out.resize(static_cast<size_t>(count));
    if (!RawrXD::GgufTensorBytes::dequantizeToFloat(static_cast<uint32_t>(desc->type), raw.data(),
                                                    static_cast<size_t>(expect), out.data()))
    {
        return false;
    }
    return true;
}

std::vector<uint8_t> GGUFRunner::compressBrutal(const void* data, size_t len)
{
    size_t out_len = 0;
    void* out_ptr = nullptr;

    // Selects the brutal_gzip kernel that matched the current architecture (MASM for x64, NEON for ARM64).
#if defined(HAS_BRUTAL_GZIP_MASM)
    out_ptr = deflate_brutal_masm(data, len, &out_len);
#elif defined(HAS_BRUTAL_GZIP_NEON)
    out_ptr = deflate_brutal_neon(data, len, &out_len);
#else
    // Fallback or no-op if architecture not supported
    // For now, return empty or implement a slow C++ fallback if needed
    return std::vector<uint8_t>();
#endif

    if (!out_ptr)
        return std::vector<uint8_t>();

    // Take ownership of the malloc-ed buffer into std::vector<uint8_t>
    // std::vector<uint8_t> makes a copy by default, so we copy and free.
    // To avoid copy, we would need to wrap it, but std::vector<uint8_t> doesn't easily take malloc ownership without
    // custom deleter. Given the speed, a memcpy here is negligible compared to qCompress.
    std::vector<uint8_t> result(reinterpret_cast<const uint8_t*>(out_ptr),
                                reinterpret_cast<const uint8_t*>(out_ptr) + out_len);
    free(out_ptr);
    return result;
}

// ============================================================
// QUANTIZATION CONTROL
// ============================================================

bool GGUFRunner::setQuantizationMode(QuantMode mode)
{
    bool success = QuantBackend::instance().setMode(mode);
    if (success)
    {
        // Quantization mode set successfully
    }
    else
    {
    }
    return success;
}

QuantMode GGUFRunner::currentQuantMode() const
{
    return QuantBackend::instance().currentMode();
}

float GGUFRunner::getCompressionRatio() const
{
    return QuantBackend::instance().getCompressionRatio();
}
