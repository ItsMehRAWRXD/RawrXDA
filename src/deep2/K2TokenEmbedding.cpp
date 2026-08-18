// ============================================================================
// K2TokenEmbedding.cpp — Real Token Embedding Lookup Implementation (K2-009)
// ============================================================================

#include "K2TokenEmbedding.hpp"
#include "K2GlobalTensorIndex.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace Deep2 {

// ── FP16 → FP32 (standalone) ──
static inline float fp16ToFloat(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    uint32_t f;
    if (exp == 0) {
        if (mant == 0) { f = sign << 31; }
        else {
            int e = -1;
            do { e++; mant <<= 1; } while (!(mant & 0x400));
            mant &= 0x3FF;
            f = (sign << 31) | ((127 - 15 - e) << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        f = (sign << 31) | (0xFF << 23) | (mant << 13);
    } else {
        f = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
    }
    float result;
    memcpy(&result, &f, sizeof(float));
    return result;
}

// ── Q4_K Block structure (144 bytes) ──
#pragma pack(push, 1)
struct Q4_K_Block {
    uint16_t d;
    uint16_t dmin;
    uint8_t  scales[12];
    uint8_t  qs[128];
};
#pragma pack(pop)
static_assert(sizeof(Q4_K_Block) == 144, "Q4_K_Block must be 144 bytes");

static inline void unpackQ4KScaleMin(const uint8_t* scales, int j,
                                     uint8_t& sc, uint8_t& m) {
    if (j < 4) {
        sc = scales[j] & 63;
        m  = scales[j + 4] & 63;
    } else {
        sc = (scales[j + 4] & 0x0F) | ((scales[j - 4] >> 6) << 4);
        m  = (scales[j + 4] >> 4)      | ((scales[j]   >> 6) << 4);
    }
}

static void dequantizeQ4KBlock(const Q4_K_Block* block, float* out) {
    float d    = fp16ToFloat(block->d);
    float dmin = fp16ToFloat(block->dmin);
    for (int j = 0; j < 8; j++) {
        uint8_t sc, m;
        unpackQ4KScaleMin(block->scales, j, sc, m);
        float scale = d * sc;
        float min   = dmin * m;
        const uint8_t* quants = block->qs + j * 16;
        for (int k = 0; k < 16; k++) {
            uint8_t byte = quants[k];
            int lo = byte & 0xF;
            int hi = (byte >> 4) & 0xF;
            out[j * 32 + k]      = scale * lo - min;
            out[j * 32 + k + 16] = scale * hi - min;
        }
    }
}

// ── Q6_K Block structure (210 bytes) ──
#pragma pack(push, 1)
struct Q6_K_Block {
    uint8_t  ql[128];
    uint8_t  qh[64];
    int8_t   scales[16];
    uint16_t d;
};
#pragma pack(pop)
static_assert(sizeof(Q6_K_Block) == 210, "Q6_K_Block must be 210 bytes");

static void dequantizeQ6KBlock(const Q6_K_Block* block, float* out) {
    float d = fp16ToFloat(block->d);
    const uint8_t* ql = block->ql;
    const uint8_t* qh = block->qh;
    const int8_t*  sc = block->scales;

    for (int n = 0; n < 256; n += 128) {
        for (int l = 0; l < 32; ++l) {
            int is = l / 16;
            int8_t q1 = (int8_t)((ql[l + 0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
            int8_t q2 = (int8_t)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
            int8_t q3 = (int8_t)((ql[l + 0]  >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
            int8_t q4 = (int8_t)((ql[l + 32]  >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
            out[l + 0]  = d * sc[is + 0] * q1;
            out[l + 32] = d * sc[is + 2] * q2;
            out[l + 64] = d * sc[is + 4] * q3;
            out[l + 96] = d * sc[is + 6] * q4;
        }
        out += 128;
        ql  += 64;
        qh  += 32;
        sc  += 8;
    }
}

// ============================================================================
// K2TokenEmbedding
// ============================================================================

K2TokenEmbedding::K2TokenEmbedding(const Config& config)
    : config_(config)
{
}

bool K2TokenEmbedding::initialize(const GlobalTensorIndex* index) {
    if (!index) {
        return false;
    }
    index_ = index;
    return locateEmbeddingTensor();
}

bool K2TokenEmbedding::locateEmbeddingTensor() {
    tensorName_ = "token_embd.weight";
    auto refOpt = index_->Find(tensorName_);
    if (!refOpt) {
        return false;
    }
    ref_ = *refOpt;
    hasRef_ = true;
    return true;
}

K2TokenEmbedding::Result K2TokenEmbedding::lookup(std::uint32_t tokenId, float* output) {
    Result result;
    result.tokenId = tokenId;

    if (!index_ || !hasRef_) {
        result.error = "K2TokenEmbedding not initialized";
        return result;
    }

    if (tokenId >= config_.vocabSize) {
        result.error = "Token ID out of range";
        return result;
    }

    // Determine quantization parameters
    std::size_t kBlockElems = 256;
    std::size_t kBlockBytes = 0;

    if (ref_.ggmlType == 12) {
        kBlockBytes = 144; // Q4_K
    } else if (ref_.ggmlType == 14) {
        kBlockBytes = 210; // Q6_K
    } else {
        result.error = "Unsupported GGML type: " + std::to_string(ref_.ggmlType);
        return result;
    }

    std::size_t blocksPerRow = (config_.hiddenSize + kBlockElems - 1) / kBlockElems;
    std::size_t rowBytes = blocksPerRow * kBlockBytes;

    // Validate tensor dimensions
    if (ref_.nDims >= 2) {
        std::size_t dim0 = ref_.shape[0];
        std::size_t dim1 = ref_.shape[1];
        // token_embd.weight may be [vocabSize, hiddenSize] or [hiddenSize, vocabSize]
        bool shapeOk = (dim0 == config_.vocabSize && dim1 == config_.hiddenSize) ||
                       (dim0 == config_.hiddenSize && dim1 == config_.vocabSize);
        if (!shapeOk) {
            result.error = "Unexpected tensor shape";
            return result;
        }
    }

    // Seek to the row within the tensor payload
    std::size_t rowOffset = tokenId * rowBytes;
    if (rowOffset + rowBytes > ref_.byteSize) {
        result.error = "Row offset exceeds tensor size";
        return result;
    }

    const auto& shardPath = index_->ShardPath(ref_.shardId);
    std::ifstream f(shardPath.string(), std::ios::binary);
    if (!f) {
        result.error = "Cannot open shard";
        return result;
    }
    f.seekg(static_cast<std::streamoff>(ref_.fileOffset + rowOffset));
    if (!f.good()) {
        result.error = "Seek failed";
        return result;
    }

    // Allocate temporary row buffer
    std::vector<uint8_t> rowBuf(rowBytes);
    acquire(rowBytes);
    f.read(reinterpret_cast<char*>(rowBuf.data()), rowBytes);
    if (static_cast<std::size_t>(f.gcount()) != rowBytes) {
        release(rowBytes);
        result.error = "Read size mismatch for row";
        return result;
    }
    result.bytesRead = rowBytes;

    // Dequantize into output
    bool decodeOk = decodeRow(rowBuf.data(), rowBytes, ref_.ggmlType, output, config_.hiddenSize);
    release(rowBytes);

    if (!decodeOk) {
        result.error = "Dequantization failed";
        return result;
    }

    result.dimensions = config_.hiddenSize;
    result.checksum = checksum(output, config_.hiddenSize);
    result.peakResidency = peakResidency_;
    result.finalResidency = currentResidency_;
    result.ok = true;
    return result;
}

bool K2TokenEmbedding::decodeRow(const std::uint8_t* data, std::size_t bytes,
                                  std::uint32_t ggmlType, float* output, std::size_t elements) {
    constexpr std::size_t kBlockElems = 256;
    std::size_t kBlockBytes = 0;

    if (ggmlType == 12) kBlockBytes = 144;
    else if (ggmlType == 14) kBlockBytes = 210;
    else return false;

    std::size_t blocksPerRow = (elements + kBlockElems - 1) / kBlockElems;
    if (bytes < blocksPerRow * kBlockBytes) return false;

    float blockDequant[256];
    std::size_t col = 0;

    for (std::size_t b = 0; b < blocksPerRow && col < elements; ++b) {
        std::size_t elemsInBlock = std::min(kBlockElems, elements - col);
        if (ggmlType == 12) {
            const Q4_K_Block* block = reinterpret_cast<const Q4_K_Block*>(data + b * kBlockBytes);
            dequantizeQ4KBlock(block, blockDequant);
        } else if (ggmlType == 14) {
            const Q6_K_Block* block = reinterpret_cast<const Q6_K_Block*>(data + b * kBlockBytes);
            dequantizeQ6KBlock(block, blockDequant);
        }
        for (std::size_t i = 0; i < elemsInBlock; ++i) {
            output[col + i] = blockDequant[i];
        }
        col += elemsInBlock;
    }

    return true;
}

std::uint64_t K2TokenEmbedding::checksum(const float* data, std::size_t count) {
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t bits;
        memcpy(&bits, &data[i], sizeof(std::uint32_t));
        sum = (sum << 1) | (sum >> 63);
        sum ^= bits;
    }
    return sum;
}

void K2TokenEmbedding::acquire(std::size_t bytes) {
    currentResidency_ += bytes;
    if (currentResidency_ > peakResidency_) {
        peakResidency_ = currentResidency_;
    }
}

void K2TokenEmbedding::release(std::size_t bytes) {
    currentResidency_ = (bytes <= currentResidency_) ? currentResidency_ - bytes : 0;
}

} // namespace Deep2
