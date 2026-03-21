#include "analyzer_distiller.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ADFileContext {
    std::ifstream stream;
    AD_GGUFHeader header{};
    bool headerValidated = false;
    std::vector<std::unique_ptr<char[]>> nameStorage;
    std::vector<AD_TensorInfo> tensors;
};

inline ADFileContext* toCtx(intptr_t handle) {
    if (handle == -1 || handle == 0) {
        return nullptr;
    }
    return reinterpret_cast<ADFileContext*>(handle);
}

template <typename T>
bool readPod(std::ifstream& in, T& out) {
    in.read(reinterpret_cast<char*>(&out), static_cast<std::streamsize>(sizeof(T)));
    return in.good();
}

bool skipBytes(std::ifstream& in, uint64_t count) {
    in.seekg(static_cast<std::streamoff>(count), std::ios::cur);
    return in.good();
}

uint64_t scalarValueSize(uint32_t ggufType) {
    switch (ggufType) {
    case 0:  return 1; // u8
    case 1:  return 1; // i8
    case 2:  return 2; // u16
    case 3:  return 2; // i16
    case 4:  return 4; // u32
    case 5:  return 4; // i32
    case 6:  return 4; // f32
    case 7:  return 1; // bool
    case 10: return 8; // u64
    case 11: return 8; // i64
    case 12: return 8; // f64
    default: return 0;
    }
}

bool skipGgufValue(std::ifstream& in, uint32_t valueType) {
    if (valueType == AD_GGUF_TYPE_STRING) {
        uint64_t strLen = 0;
        return readPod(in, strLen) && skipBytes(in, strLen);
    }

    if (valueType == 9) { // array
        uint32_t elemType = 0;
        uint64_t count = 0;
        if (!readPod(in, elemType) || !readPod(in, count)) {
            return false;
        }

        if (elemType == AD_GGUF_TYPE_STRING) {
            for (uint64_t i = 0; i < count; ++i) {
                uint64_t strLen = 0;
                if (!readPod(in, strLen) || !skipBytes(in, strLen)) {
                    return false;
                }
            }
            return true;
        }

        const uint64_t elemSize = scalarValueSize(elemType);
        if (elemSize == 0) {
            return false;
        }
        return skipBytes(in, elemSize * count);
    }

    const uint64_t size = scalarValueSize(valueType);
    if (size == 0) {
        return false;
    }
    return skipBytes(in, size);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void destroyContext(ADFileContext* ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stream.is_open()) {
        ctx->stream.close();
    }
    delete ctx;
}

} // namespace

extern "C" intptr_t AD_OpenGGUFFile(const char* path) {
    if (!path || !*path) {
        return -1;
    }

    auto* ctx = new ADFileContext();
    ctx->stream.open(path, std::ios::binary);
    if (!ctx->stream.is_open()) {
        delete ctx;
        return -1;
    }
    return reinterpret_cast<intptr_t>(ctx);
}

extern "C" int AD_ValidateGGUFHeader(AD_GGUFHeader* header, intptr_t fileHandle) {
    ADFileContext* ctx = toCtx(fileHandle);
    if (!ctx || !header || !ctx->stream.is_open()) {
        return 0;
    }

    ctx->stream.clear();
    ctx->stream.seekg(0, std::ios::beg);

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t tensorCount = 0;
    uint64_t metadataKv = 0;
    if (!readPod(ctx->stream, magic) ||
        !readPod(ctx->stream, version) ||
        !readPod(ctx->stream, tensorCount) ||
        !readPod(ctx->stream, metadataKv)) {
        return 0;
    }

    const uint32_t expectedMagic = 0x46554747U; // "GGUF"
    if (magic != expectedMagic || version != 3U) {
        return 0;
    }

    header->magic = static_cast<uint64_t>(magic);
    header->version = version;
    header->tensor_count = tensorCount;
    header->metadata_kv = metadataKv;

    ctx->header = *header;
    ctx->headerValidated = true;
    return 1;
}

extern "C" int AD_SkipMetadataKV(uint64_t kvCount, intptr_t fileHandle) {
    ADFileContext* ctx = toCtx(fileHandle);
    if (!ctx || !ctx->stream.is_open()) {
        return 0;
    }

    for (uint64_t i = 0; i < kvCount; ++i) {
        uint64_t keyLen = 0;
        if (!readPod(ctx->stream, keyLen) || !skipBytes(ctx->stream, keyLen)) {
            return 0;
        }

        uint32_t valueType = 0;
        if (!readPod(ctx->stream, valueType)) {
            return 0;
        }

        if (!skipGgufValue(ctx->stream, valueType)) {
            return 0;
        }
    }

    return 1;
}

extern "C" uint64_t AD_CountParameters(AD_TensorInfo* tensorInfo) {
    if (!tensorInfo) {
        return 0;
    }
    uint64_t total = 1;
    const uint64_t dims = std::min<uint64_t>(tensorInfo->shape_rank, 4);
    for (uint64_t i = 0; i < dims; ++i) {
        total *= std::max<uint64_t>(1, tensorInfo->shape[i]);
    }
    return total;
}

extern "C" void AD_IdentifyPattern(AD_TensorInfo* tensorInfo, AD_AnalysisResult* analysis) {
    if (!tensorInfo) {
        return;
    }

    const char* namePtr = reinterpret_cast<const char*>(tensorInfo->name_ptr);
    const std::string lower = toLower(namePtr ? std::string(namePtr) : std::string());

    uint32_t pattern = AD_PATTERN_UNKNOWN;
    if (lower.find("ffn") != std::string::npos) {
        pattern = AD_PATTERN_FFN;
    } else if (lower.find("attn") != std::string::npos || lower.find("attention") != std::string::npos) {
        pattern = AD_PATTERN_ATTENTION;
    } else if (lower.find("embed") != std::string::npos || lower.find("token") != std::string::npos) {
        pattern = AD_PATTERN_EMBED;
    } else if (lower.find("norm") != std::string::npos) {
        pattern = AD_PATTERN_NORM;
    }

    tensorInfo->pattern_type = pattern;
    if (!analysis) {
        return;
    }
    switch (pattern) {
    case AD_PATTERN_FFN: ++analysis->ffn_blocks; break;
    case AD_PATTERN_ATTENTION: ++analysis->attn_heads; break;
    case AD_PATTERN_EMBED: ++analysis->embed_tokens; break;
    case AD_PATTERN_NORM: ++analysis->norm_layers; break;
    default: ++analysis->unknown_layers; break;
    }
}

extern "C" int AD_ParseTensorMetadata(AD_TensorInfo* tensorTable, AD_AnalysisResult* analysis, intptr_t fileHandle) {
    ADFileContext* ctx = toCtx(fileHandle);
    if (!ctx || !ctx->stream.is_open() || !ctx->headerValidated || !analysis) {
        return 0;
    }

    *analysis = {};
    ctx->nameStorage.clear();
    ctx->tensors.clear();
    ctx->tensors.reserve(static_cast<size_t>(ctx->header.tensor_count));

    for (uint64_t i = 0; i < ctx->header.tensor_count; ++i) {
        uint64_t nameLen = 0;
        uint32_t nDims = 0;
        if (!readPod(ctx->stream, nameLen) || !readPod(ctx->stream, nDims)) {
            return 0;
        }

        auto nameOwned = std::make_unique<char[]>(static_cast<size_t>(nameLen) + 1U);
        if (nameLen > 0) {
            ctx->stream.read(nameOwned.get(), static_cast<std::streamsize>(nameLen));
            if (!ctx->stream.good()) {
                return 0;
            }
        }
        nameOwned[nameLen] = '\0';

        AD_TensorInfo info{};
        info.name_len = nameLen;
        info.name_ptr = reinterpret_cast<uint64_t>(nameOwned.get());
        info.shape_rank = nDims;

        for (uint32_t d = 0; d < nDims; ++d) {
            uint64_t dim = 0;
            if (!readPod(ctx->stream, dim)) {
                return 0;
            }
            if (d < 4U) {
                info.shape[d] = dim;
            }
        }

        if (!readPod(ctx->stream, info.dtype) || !readPod(ctx->stream, info.file_offset)) {
            return 0;
        }

        info.param_count = AD_CountParameters(&info);
        analysis->total_params += info.param_count;
        AD_IdentifyPattern(&info, analysis);

        if (tensorTable) {
            tensorTable[i] = info;
        }
        ctx->nameStorage.push_back(std::move(nameOwned));
        ctx->tensors.push_back(info);
    }

    if (tensorTable) {
        tensorTable[ctx->header.tensor_count] = {};
    }
    return 1;
}

extern "C" int AD_AnalyzeStructure(AD_TensorInfo* tensorTable, AD_AnalysisResult* analysis) {
    if (!tensorTable || !analysis) {
        return 0;
    }

    uint32_t maxLayer = 0;
    for (uint64_t i = 0; tensorTable[i].name_len != 0; ++i) {
        const char* name = reinterpret_cast<const char*>(tensorTable[i].name_ptr);
        const int32_t layer = AD_ExtractLayerIndex(name ? name : "");
        if (layer >= 0) {
            maxLayer = std::max(maxLayer, static_cast<uint32_t>(layer + 1));
        }
    }
    analysis->layer_count = maxLayer;
    return 1;
}

extern "C" int AD_WriteExecFile(const char* outputPath, AD_AnalysisResult* analysis) {
    if (!outputPath || !*outputPath || !analysis) {
        return 0;
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        return 0;
    }

    out << "RAWRXD_EXEC_V1\n";
    out << "total_params=" << analysis->total_params << "\n";
    out << "layer_count=" << analysis->layer_count << "\n";
    out << "ffn_blocks=" << analysis->ffn_blocks << "\n";
    out << "attn_heads=" << analysis->attn_heads << "\n";
    out << "embed_tokens=" << analysis->embed_tokens << "\n";
    out << "norm_layers=" << analysis->norm_layers << "\n";
    out << "unknown_layers=" << analysis->unknown_layers << "\n";
    return out.good() ? 1 : 0;
}

extern "C" int AD_DistillToExec(const char* outputPath, AD_AnalysisResult* analysis, intptr_t fileHandle) {
    (void)fileHandle;
    return AD_WriteExecFile(outputPath, analysis);
}

extern "C" int32_t AD_ExtractLayerIndex(const char* name) {
    if (!name) {
        return -1;
    }
    std::string s(name);
    const std::string lower = toLower(s);
    const std::string markers[] = {"blk.", "layer.", "layers."};
    for (const auto& marker : markers) {
        const size_t pos = lower.find(marker);
        if (pos == std::string::npos) {
            continue;
        }
        size_t i = pos + marker.size();
        int32_t value = 0;
        bool hasDigits = false;
        while (i < lower.size() && std::isdigit(static_cast<unsigned char>(lower[i]))) {
            hasDigits = true;
            value = value * 10 + static_cast<int32_t>(lower[i] - '0');
            ++i;
        }
        if (hasDigits) {
            return value;
        }
    }
    return -1;
}

extern "C" int AD_ProcessGGUF(const char* inputPath, const char* outputExecPath) {
    const intptr_t handle = AD_OpenGGUFFile(inputPath);
    ADFileContext* ctx = toCtx(handle);
    if (!ctx) {
        return 0;
    }

    AD_GGUFHeader header{};
    AD_AnalysisResult analysis{};
    int ok = 0;

    do {
        if (!AD_ValidateGGUFHeader(&header, handle)) {
            break;
        }
        if (!AD_SkipMetadataKV(header.metadata_kv, handle)) {
            break;
        }

        std::vector<AD_TensorInfo> table(static_cast<size_t>(header.tensor_count) + 1U);
        if (!AD_ParseTensorMetadata(table.data(), &analysis, handle)) {
            break;
        }
        if (!AD_AnalyzeStructure(table.data(), &analysis)) {
            break;
        }
        if (!AD_DistillToExec(outputExecPath, &analysis, handle)) {
            break;
        }

        ok = 1;
    } while (false);

    destroyContext(ctx);
    return ok;
}
