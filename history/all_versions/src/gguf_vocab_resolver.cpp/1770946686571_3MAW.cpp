// gguf_vocab_resolver.cpp — GGUF Vocabulary Detection & Resolution
// Reads GGUF binary metadata to detect vocab size, model family, tokenizer type.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "gguf_vocab_resolver.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// GGUF binary format constants
// ---------------------------------------------------------------------------
static constexpr uint32_t GGUF_MAGIC = 0x46465547;  // "GGUF" little-endian

// GGUF value types
enum GGUFValueType : uint32_t {
    GGUF_TYPE_UINT8    = 0,
    GGUF_TYPE_INT8     = 1,
    GGUF_TYPE_UINT16   = 2,
    GGUF_TYPE_INT16    = 3,
    GGUF_TYPE_UINT32   = 4,
    GGUF_TYPE_INT32    = 5,
    GGUF_TYPE_FLOAT32  = 6,
    GGUF_TYPE_BOOL     = 7,
    GGUF_TYPE_STRING   = 8,
    GGUF_TYPE_ARRAY    = 9,
    GGUF_TYPE_UINT64   = 10,
    GGUF_TYPE_INT64    = 11,
    GGUF_TYPE_FLOAT64  = 12,
};

// ---------------------------------------------------------------------------
// Little-endian readers (safe, no alignment issues)
// ---------------------------------------------------------------------------
static uint32_t read_u32(std::ifstream& f) {
    uint8_t b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    return static_cast<uint32_t>(b[0])
         | (static_cast<uint32_t>(b[1]) << 8)
         | (static_cast<uint32_t>(b[2]) << 16)
         | (static_cast<uint32_t>(b[3]) << 24);
}

static uint64_t read_u64(std::ifstream& f) {
    uint8_t b[8];
    f.read(reinterpret_cast<char*>(b), 8);
    uint64_t lo = static_cast<uint64_t>(b[0])
                | (static_cast<uint64_t>(b[1]) << 8)
                | (static_cast<uint64_t>(b[2]) << 16)
                | (static_cast<uint64_t>(b[3]) << 24);
    uint64_t hi = static_cast<uint64_t>(b[4])
                | (static_cast<uint64_t>(b[5]) << 8)
                | (static_cast<uint64_t>(b[6]) << 16)
                | (static_cast<uint64_t>(b[7]) << 24);
    return lo | (hi << 32);
}

static float read_f32(std::ifstream& f) {
    uint32_t bits = read_u32(f);
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
}

static std::string read_gguf_string(std::ifstream& f) {
    uint64_t len = read_u64(f);
    if (len == 0 || len > 1024 * 1024) return {};
    std::string s(static_cast<size_t>(len), '\0');
    f.read(s.data(), static_cast<std::streamsize>(len));
    return s;
}

// Skip a GGUF typed value, returns false on failure
static bool skip_gguf_value(std::ifstream& f, uint32_t vtype) {
    switch (vtype) {
        case GGUF_TYPE_UINT8:
        case GGUF_TYPE_INT8:
        case GGUF_TYPE_BOOL:     f.seekg(1, std::ios::cur); break;
        case GGUF_TYPE_UINT16:
        case GGUF_TYPE_INT16:    f.seekg(2, std::ios::cur); break;
        case GGUF_TYPE_UINT32:
        case GGUF_TYPE_INT32:
        case GGUF_TYPE_FLOAT32:  f.seekg(4, std::ios::cur); break;
        case GGUF_TYPE_UINT64:
        case GGUF_TYPE_INT64:
        case GGUF_TYPE_FLOAT64:  f.seekg(8, std::ios::cur); break;
        case GGUF_TYPE_STRING: {
            uint64_t len = read_u64(f);
            f.seekg(static_cast<std::streamoff>(len), std::ios::cur);
            break;
        }
        case GGUF_TYPE_ARRAY: {
            uint32_t elemType = read_u32(f);
            uint64_t count    = read_u64(f);
            for (uint64_t i = 0; i < count && f.good(); ++i) {
                skip_gguf_value(f, elemType);
            }
            break;
        }
        default: return false;
    }
    return f.good();
}

// Read a GGUF value as string (numeric values → to_string)
static std::string read_gguf_value_as_string(std::ifstream& f, uint32_t vtype) {
    switch (vtype) {
        case GGUF_TYPE_UINT8: {
            uint8_t v; f.read(reinterpret_cast<char*>(&v), 1);
            return std::to_string(v);
        }
        case GGUF_TYPE_INT8: {
            int8_t v; f.read(reinterpret_cast<char*>(&v), 1);
            return std::to_string(v);
        }
        case GGUF_TYPE_UINT16: {
            uint8_t b[2]; f.read(reinterpret_cast<char*>(b), 2);
            return std::to_string(static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8));
        }
        case GGUF_TYPE_INT16: {
            int16_t v; uint16_t u;
            uint8_t b[2]; f.read(reinterpret_cast<char*>(b), 2);
            u = static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
            std::memcpy(&v, &u, 2);
            return std::to_string(v);
        }
        case GGUF_TYPE_UINT32: return std::to_string(read_u32(f));
        case GGUF_TYPE_INT32: {
            uint32_t u = read_u32(f);
            int32_t v; std::memcpy(&v, &u, 4);
            return std::to_string(v);
        }
        case GGUF_TYPE_FLOAT32: return std::to_string(read_f32(f));
        case GGUF_TYPE_UINT64: return std::to_string(read_u64(f));
        case GGUF_TYPE_INT64: {
            uint64_t u = read_u64(f);
            int64_t v; std::memcpy(&v, &u, 8);
            return std::to_string(v);
        }
        case GGUF_TYPE_FLOAT64: {
            uint64_t bits = read_u64(f);
            double v; std::memcpy(&v, &bits, 8);
            return std::to_string(v);
        }
        case GGUF_TYPE_BOOL: {
            uint8_t v; f.read(reinterpret_cast<char*>(&v), 1);
            return v ? "true" : "false";
        }
        case GGUF_TYPE_STRING: return read_gguf_string(f);
        case GGUF_TYPE_ARRAY: {
            // For arrays, record element count
            uint32_t elemType = read_u32(f);
            uint64_t count    = read_u64(f);
            std::string result = "[array:" + std::to_string(count) + "]";
            for (uint64_t i = 0; i < count && f.good(); ++i) {
                skip_gguf_value(f, elemType);
            }
            return result;
        }
        default: return "";
    }
}

// ---------------------------------------------------------------------------
// Read GGUF file metadata into key→string map
// ---------------------------------------------------------------------------
static bool read_gguf_metadata(const std::string& path,
                               std::map<std::string, std::string>& outMeta) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    uint32_t magic = read_u32(f);
    if (magic != GGUF_MAGIC) return false;

    uint32_t version    = read_u32(f);
    uint64_t tensorCnt  = read_u64(f);
    uint64_t kvCount    = read_u64(f);

    (void)version;
    (void)tensorCnt;

    for (uint64_t i = 0; i < kvCount && f.good(); ++i) {
        std::string key    = read_gguf_string(f);
        uint32_t    vtype  = read_u32(f);
        std::string value  = read_gguf_value_as_string(f, vtype);

        if (!key.empty()) {
            outMeta[key] = value;
        }
    }

    return !outMeta.empty();
}

// ---------------------------------------------------------------------------
// Helper: case-insensitive contains
// ---------------------------------------------------------------------------
static bool icontains(const std::string& haystack, const char* needle) {
    std::string hLow = haystack;
    std::transform(hLow.begin(), hLow.end(), hLow.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string nLow(needle);
    std::transform(nLow.begin(), nLow.end(), nLow.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return hLow.find(nLow) != std::string::npos;
}

// ===========================================================================
// GGUFVocabResolver Implementation
// ===========================================================================

GGUFVocabResolver::GGUFVocabResolver() {
    setupVocabMappings();
}

void GGUFVocabResolver::setupVocabMappings() {
    // Known model family → expected vocab sizes
    expectedVocabSizes["llama"]     = 32000;
    expectedVocabSizes["llama2"]    = 32000;
    expectedVocabSizes["llama3"]    = 128256;
    expectedVocabSizes["codellama"] = 32016;
    expectedVocabSizes["mistral"]   = 32000;
    expectedVocabSizes["mixtral"]   = 32000;
    expectedVocabSizes["phi"]       = 51200;
    expectedVocabSizes["phi2"]      = 51200;
    expectedVocabSizes["phi3"]      = 32064;
    expectedVocabSizes["qwen"]      = 151936;
    expectedVocabSizes["qwen2"]     = 151936;
    expectedVocabSizes["gemma"]     = 256000;
    expectedVocabSizes["gemma2"]    = 256000;
    expectedVocabSizes["falcon"]    = 65024;
    expectedVocabSizes["starcoder"] = 49152;
    expectedVocabSizes["gpt2"]      = 50257;
    expectedVocabSizes["gptneox"]   = 50432;
    expectedVocabSizes["mpt"]       = 50432;
    expectedVocabSizes["baichuan"]  = 64000;
    expectedVocabSizes["internlm"]  = 103168;
    expectedVocabSizes["deepseek"]  = 102400;
    expectedVocabSizes["yi"]        = 64000;
    expectedVocabSizes["command-r"] = 255029;
    expectedVocabSizes["tinyllama"] = 32000;

    // Known GGUF metadata keys that may hold vocab size info
    vocabKeyMappings["direct"] = {
        "tokenizer.ggml.tokens",
        "tokenizer.ggml.token_type",
    };
    vocabKeyMappings["metadata"] = {
        "general.architecture",
        "general.name",
        "general.file_type",
    };
    vocabKeyMappings["tensor"] = {
        "token_embd.weight",
        "output.weight",
    };
}

// ---------------------------------------------------------------------------
// detectVocabSize — Main entry point
// ---------------------------------------------------------------------------
VocabSizeDetection GGUFVocabResolver::detectVocabSize(
    const std::map<std::string, std::string>& metadata,
    const std::string& modelPath)
{
    // Strategy 1: If modelPath provided, read GGUF directly for authoritative data
    if (!modelPath.empty()) {
        std::map<std::string, std::string> fileMeta;
        if (read_gguf_metadata(modelPath, fileMeta)) {
            // Merge file metadata with provided metadata (file takes precedence)
            std::map<std::string, std::string> merged = metadata;
            for (const auto& kv : fileMeta) {
                merged[kv.first] = kv.second;
            }

            // Try detecting from the merged metadata
            VocabSizeDetection result = detectFromMetadata(merged);
            if (result.isConfident) return result;

            // Try tensor dimensions
            result = detectFromTensorDimensions(merged);
            if (result.isConfident) return result;

            // Try TinyLlama special case
            result = detectForTinyLlama(merged);
            if (result.isConfident) return result;

            // Determine family and use expected size
            std::string family = determineModelFamily(merged, modelPath);
            if (!family.empty() && family != "unknown") {
                auto it = expectedVocabSizes.find(family);
                if (it != expectedVocabSizes.end()) {
                    return createDetection("family_lookup", it->second, true,
                                           {"general.architecture"}, family);
                }
            }
        }
    }

    // Strategy 2: Use provided metadata only
    VocabSizeDetection result = detectFromMetadata(metadata);
    if (result.isConfident) return result;

    result = detectFromTensorDimensions(metadata);
    if (result.isConfident) return result;

    // Strategy 3: Family-based fallback
    std::string family = determineModelFamily(metadata, modelPath);
    uint32_t specialSize = handleSpecialCases(family, metadata);
    if (specialSize != 0) {
        return createDetection("special_case", specialSize, true, {}, family);
    }

    // Default fallback
    return createDetection("default_fallback", 32000, false, {}, family);
}

// ---------------------------------------------------------------------------
// detectFromMetadata — Check tokenizer.ggml.tokens array size
// ---------------------------------------------------------------------------
VocabSizeDetection GGUFVocabResolver::detectFromMetadata(
    const std::map<std::string, std::string>& metadata)
{
    // Check for tokenizer.ggml.tokens (array — stored as "[array:N]")
    auto it = metadata.find("tokenizer.ggml.tokens");
    if (it != metadata.end()) {
        const std::string& val = it->second;
        // Parse "[array:N]" format
        if (val.size() > 7 && val.substr(0, 7) == "[array:") {
            size_t endPos = val.find(']', 7);
            if (endPos != std::string::npos) {
                std::string countStr = val.substr(7, endPos - 7);
                try {
                    uint32_t count = static_cast<uint32_t>(std::stoul(countStr));
                    if (count >= minVocabSize && count <= maxVocabSize) {
                        std::string family = determineModelFamily(metadata, "");
                        return createDetection("tokenizer.ggml.tokens", count, true,
                                               {"tokenizer.ggml.tokens"}, family);
                    }
                } catch (...) { /* parse failure — fall through */ }
            }
        }
    }

    // Check for explicit vocab_size in metadata
    auto vsIt = metadata.find("vocab_size");
    if (vsIt != metadata.end()) {
        try {
            uint32_t sz = static_cast<uint32_t>(std::stoul(vsIt->second));
            if (sz >= minVocabSize && sz <= maxVocabSize) {
                return createDetection("vocab_size_key", sz, true,
                                       {"vocab_size"}, determineModelFamily(metadata, ""));
            }
        } catch (...) {}
    }

    return createDetection("metadata_miss", 0, false, {}, "");
}

// ---------------------------------------------------------------------------
// detectFromTensorDimensions
// ---------------------------------------------------------------------------
VocabSizeDetection GGUFVocabResolver::detectFromTensorDimensions(
    const std::map<std::string, std::string>& metadata)
{
    // Check token_embd.weight dimensions (first dim = vocab size)
    for (const char* key : {"token_embd.weight.shape", "output.weight.shape"}) {
        auto it = metadata.find(key);
        if (it != metadata.end()) {
            // Shape string format depends on GGUF writer — try parsing first number
            try {
                uint32_t dim = static_cast<uint32_t>(std::stoul(it->second));
                if (dim >= minVocabSize && dim <= maxVocabSize) {
                    return createDetection("tensor_dimension", dim, true,
                                           {key}, determineModelFamily(metadata, ""));
                }
            } catch (...) {}
        }
    }

    return createDetection("tensor_miss", 0, false, {}, "");
}

// ---------------------------------------------------------------------------
// detectForTinyLlama — special case for TinyLlama 1.1B
// ---------------------------------------------------------------------------
VocabSizeDetection GGUFVocabResolver::detectForTinyLlama(
    const std::map<std::string, std::string>& metadata)
{
    auto nameIt = metadata.find("general.name");
    if (nameIt != metadata.end() && icontains(nameIt->second, "tinyllama")) {
        return createDetection("tinyllama_special", 32000, true,
                               {"general.name"}, "tinyllama");
    }
    return createDetection("not_tinyllama", 0, false, {}, "");
}

// ---------------------------------------------------------------------------
// determineModelFamily
// ---------------------------------------------------------------------------
std::string GGUFVocabResolver::determineModelFamily(
    const std::map<std::string, std::string>& metadata,
    const std::string& modelPath) const
{
    // Priority 1: general.architecture key
    auto archIt = metadata.find("general.architecture");
    if (archIt != metadata.end() && !archIt->second.empty()) {
        std::string arch = archIt->second;
        std::transform(arch.begin(), arch.end(), arch.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return arch;
    }

    // Priority 2: general.name key — scan for known family names
    auto nameIt = metadata.find("general.name");
    if (nameIt != metadata.end()) {
        const std::string& name = nameIt->second;
        static const char* families[] = {
            "llama3", "llama2", "llama", "codellama",
            "mistral", "mixtral", "phi3", "phi2", "phi",
            "qwen2", "qwen", "gemma2", "gemma",
            "falcon", "starcoder", "gpt2", "gptneox", "mpt",
            "baichuan", "internlm", "deepseek", "yi",
            "command-r", "tinyllama"
        };
        for (const char* fam : families) {
            if (icontains(name, fam)) return fam;
        }
    }

    // Priority 3: filename heuristics
    if (!modelPath.empty()) {
        // Extract filename
        std::string filename = modelPath;
        size_t lastSlash  = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) filename = filename.substr(lastSlash + 1);

        static const char* families[] = {
            "llama3", "llama2", "llama", "codellama",
            "mistral", "mixtral", "phi3", "phi2", "phi",
            "qwen2", "qwen", "gemma2", "gemma",
            "falcon", "starcoder", "tinyllama", "deepseek", "yi"
        };
        for (const char* fam : families) {
            if (icontains(filename, fam)) return fam;
        }
    }

    return "unknown";
}

// ---------------------------------------------------------------------------
// handleSpecialCases
// ---------------------------------------------------------------------------
uint32_t GGUFVocabResolver::handleSpecialCases(
    const std::string& modelFamily,
    const std::map<std::string, std::string>& metadata)
{
    if (modelFamily.empty() || modelFamily == "unknown") return 0;

    auto it = expectedVocabSizes.find(modelFamily);
    if (it != expectedVocabSizes.end()) return it->second;

    // Check for "llama" prefix matching
    if (modelFamily.find("llama") != std::string::npos) {
        // Check if llama3 (128k vocab) vs llama2/1 (32k)
        auto nameIt = metadata.find("general.name");
        if (nameIt != metadata.end() && icontains(nameIt->second, "llama-3")) {
            return 128256;
        }
        return 32000;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------
bool GGUFVocabResolver::isVocabSizeReasonable(uint32_t vocabSize,
                                               const std::string& modelFamily) const
{
    if (vocabSize < minVocabSize || vocabSize > maxVocabSize) return false;

    // If family known, check within 20% of expected
    if (!modelFamily.empty() && modelFamily != "unknown") {
        auto it = expectedVocabSizes.find(modelFamily);
        if (it != expectedVocabSizes.end()) {
            uint32_t expected = it->second;
            uint32_t lo = expected * 80 / 100;
            uint32_t hi = expected * 120 / 100;
            return vocabSize >= lo && vocabSize <= hi;
        }
    }
    return true;
}

uint32_t GGUFVocabResolver::applySanityBounds(uint32_t detectedSize) {
    if (detectedSize < minVocabSize) return minVocabSize;
    if (detectedSize > maxVocabSize) return maxVocabSize;
    return detectedSize;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
std::vector<std::string> GGUFVocabResolver::getAllPossibleVocabKeys() const {
    return {
        "tokenizer.ggml.tokens",
        "tokenizer.ggml.token_type",
        "tokenizer.ggml.scores",
        "tokenizer.ggml.merges",
        "tokenizer.ggml.model",
        "tokenizer.ggml.bos_token_id",
        "tokenizer.ggml.eos_token_id",
        "tokenizer.ggml.unknown_token_id",
        "tokenizer.ggml.padding_token_id",
        "vocab_size",
        "token_embd.weight",
        "output.weight",
    };
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
VocabSizeDetection GGUFVocabResolver::createDetection(
    const std::string& method, uint32_t size, bool confident,
    const std::vector<std::string>& evidence, const std::string& family)
{
    VocabSizeDetection result;
    result.detectionMethod = method;
    result.detectedSize    = size;
    result.isConfident     = confident;
    result.evidenceKeys    = evidence;
    result.modelFamily     = family;
    return result;
}

