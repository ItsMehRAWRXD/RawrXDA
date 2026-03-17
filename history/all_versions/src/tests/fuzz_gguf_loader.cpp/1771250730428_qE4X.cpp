// ================================================================
// fuzz_gguf_loader.cpp — Fuzz Test Harness for GGUF Model Loading
// Generates randomized/malformed GGUF headers to test robustness
// Compile: cl /O2 /std:c++20 /EHsc fuzz_gguf_loader.cpp /Fe:fuzz_gguf.exe
// ================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <random>
#include <chrono>
#include <functional>
#include <string>

// ================================================================
// GGUF format structures (minimal reproduction)
// ================================================================
namespace gguf {

constexpr uint32_t GGUF_MAGIC        = 0x46475547; // "GGUF"
constexpr uint32_t GGUF_VERSION_3    = 3;
constexpr uint32_t GGUF_VERSION_2    = 2;

enum GGUFValueType : uint32_t {
    GGUF_TYPE_UINT8   = 0,
    GGUF_TYPE_INT8    = 1,
    GGUF_TYPE_UINT16  = 2,
    GGUF_TYPE_INT16   = 3,
    GGUF_TYPE_UINT32  = 4,
    GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL    = 7,
    GGUF_TYPE_STRING  = 8,
    GGUF_TYPE_ARRAY   = 9,
    GGUF_TYPE_UINT64  = 10,
    GGUF_TYPE_INT64   = 11,
    GGUF_TYPE_FLOAT64 = 12,
};

enum GGMLType : uint32_t {
    GGML_TYPE_F32     = 0,
    GGML_TYPE_F16     = 1,
    GGML_TYPE_Q4_0    = 2,
    GGML_TYPE_Q4_1    = 3,
    GGML_TYPE_Q5_0    = 6,
    GGML_TYPE_Q5_1    = 7,
    GGML_TYPE_Q8_0    = 8,
    GGML_TYPE_Q8_1    = 9,
    GGML_TYPE_Q2_K    = 10,
    GGML_TYPE_Q3_K    = 11,
    GGML_TYPE_Q4_K    = 12,
    GGML_TYPE_Q5_K    = 13,
    GGML_TYPE_Q6_K    = 14,
};

#pragma pack(push, 1)
struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
};

struct GGUFString {
    uint64_t length;
    // followed by `length` bytes of UTF-8 data
};

struct GGUFTensorInfo {
    // GGUFString name;
    uint32_t n_dims;
    // uint64_t dims[n_dims];
    // uint32_t type; (GGMLType)
    // uint64_t offset;
};
#pragma pack(pop)

// ================================================================
// GGUF Parser (target of fuzzing)
// ================================================================
struct ParseResult {
    bool     valid;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_count;
    size_t   bytes_consumed;
    char     error[256];
};

ParseResult parseGGUFHeader(const uint8_t* data, size_t data_len) {
    ParseResult result = {};

    if (data_len < sizeof(GGUFHeader)) {
        snprintf(result.error, sizeof(result.error), "Buffer too small for header");
        return result;
    }

    const GGUFHeader* hdr = reinterpret_cast<const GGUFHeader*>(data);

    // Validate magic
    if (hdr->magic != GGUF_MAGIC) {
        snprintf(result.error, sizeof(result.error),
            "Invalid magic: 0x%08X (expected 0x%08X)", hdr->magic, GGUF_MAGIC);
        return result;
    }

    // Validate version
    if (hdr->version < 2 || hdr->version > 3) {
        snprintf(result.error, sizeof(result.error),
            "Unsupported version: %u", hdr->version);
        return result;
    }

    // Sanity check tensor count (prevent OOM)
    if (hdr->tensor_count > 100000) {
        snprintf(result.error, sizeof(result.error),
            "Unreasonable tensor count: %llu", (unsigned long long)hdr->tensor_count);
        return result;
    }

    // Sanity check metadata count
    if (hdr->metadata_kv_count > 100000) {
        snprintf(result.error, sizeof(result.error),
            "Unreasonable metadata count: %llu", (unsigned long long)hdr->metadata_kv_count);
        return result;
    }

    // Try to parse metadata keys
    size_t offset = sizeof(GGUFHeader);
    for (uint64_t i = 0; i < hdr->metadata_kv_count && i < 1000; i++) {
        // Read key string
        if (offset + sizeof(GGUFString) > data_len) {
            snprintf(result.error, sizeof(result.error),
                "Truncated at metadata key %llu", (unsigned long long)i);
            return result;
        }

        const GGUFString* key_str = reinterpret_cast<const GGUFString*>(data + offset);

        // Validate string length
        if (key_str->length > 65536) {
            snprintf(result.error, sizeof(result.error),
                "Unreasonable key length: %llu at metadata %llu",
                (unsigned long long)key_str->length, (unsigned long long)i);
            return result;
        }

        offset += sizeof(GGUFString) + key_str->length;
        if (offset > data_len) {
            snprintf(result.error, sizeof(result.error),
                "Truncated reading key data at metadata %llu", (unsigned long long)i);
            return result;
        }

        // Read value type
        if (offset + 4 > data_len) {
            snprintf(result.error, sizeof(result.error),
                "Truncated at value type, metadata %llu", (unsigned long long)i);
            return result;
        }

        uint32_t vtype = *reinterpret_cast<const uint32_t*>(data + offset);
        offset += 4;

        // Skip value based on type
        switch (vtype) {
            case GGUF_TYPE_UINT8:
            case GGUF_TYPE_INT8:
            case GGUF_TYPE_BOOL:
                offset += 1; break;
            case GGUF_TYPE_UINT16:
            case GGUF_TYPE_INT16:
                offset += 2; break;
            case GGUF_TYPE_UINT32:
            case GGUF_TYPE_INT32:
            case GGUF_TYPE_FLOAT32:
                offset += 4; break;
            case GGUF_TYPE_UINT64:
            case GGUF_TYPE_INT64:
            case GGUF_TYPE_FLOAT64:
                offset += 8; break;
            case GGUF_TYPE_STRING: {
                if (offset + sizeof(GGUFString) > data_len) {
                    snprintf(result.error, sizeof(result.error),
                        "Truncated at string value, metadata %llu", (unsigned long long)i);
                    return result;
                }
                const GGUFString* val = reinterpret_cast<const GGUFString*>(data + offset);
                if (val->length > 1048576) {  // 1MB max string
                    snprintf(result.error, sizeof(result.error),
                        "Unreasonable string value length: %llu", (unsigned long long)val->length);
                    return result;
                }
                offset += sizeof(GGUFString) + val->length;
                break;
            }
            case GGUF_TYPE_ARRAY:
                // Skip array parsing for fuzzing simplicity
                offset += 12;  // type(4) + count(8) minimum
                break;
            default:
                snprintf(result.error, sizeof(result.error),
                    "Unknown value type: %u at metadata %llu",
                    vtype, (unsigned long long)i);
                return result;
        }

        if (offset > data_len) {
            snprintf(result.error, sizeof(result.error),
                "Buffer overrun at metadata %llu", (unsigned long long)i);
            return result;
        }
    }

    result.valid = true;
    result.version = hdr->version;
    result.tensor_count = hdr->tensor_count;
    result.metadata_count = hdr->metadata_kv_count;
    result.bytes_consumed = offset;
    return result;
}

} // namespace gguf

// ================================================================
// Fuzz Engine
// ================================================================
namespace fuzz {

struct FuzzStats {
    uint64_t total_iterations   = 0;
    uint64_t valid_parses       = 0;
    uint64_t invalid_parses     = 0;
    uint64_t crashes_caught     = 0;
    uint64_t buffer_overflows   = 0;
    double   total_time_ms      = 0;
};

class GGUFFuzzer {
public:
    GGUFFuzzer(uint64_t seed = 0)
        : rng_(seed ? seed : std::random_device{}()) {}

    // ================================================================
    // Strategy 1: Completely random bytes
    // ================================================================
    std::vector<uint8_t> generateRandom(size_t min_size = 1, size_t max_size = 4096) {
        std::uniform_int_distribution<size_t> size_dist(min_size, max_size);
        std::uniform_int_distribution<int> byte_dist(0, 255);

        size_t size = size_dist(rng_);
        std::vector<uint8_t> data(size);
        for (auto& b : data) b = static_cast<uint8_t>(byte_dist(rng_));
        return data;
    }

    // ================================================================
    // Strategy 2: Valid header with corrupted metadata
    // ================================================================
    std::vector<uint8_t> generateCorruptedMetadata() {
        std::vector<uint8_t> data(512, 0);
        auto* hdr = reinterpret_cast<gguf::GGUFHeader*>(data.data());
        hdr->magic = gguf::GGUF_MAGIC;
        hdr->version = 3;
        hdr->tensor_count = randomU64(0, 100);
        hdr->metadata_kv_count = randomU64(0, 50);

        // Fill rest with random
        std::uniform_int_distribution<int> byte_dist(0, 255);
        for (size_t i = sizeof(gguf::GGUFHeader); i < data.size(); i++) {
            data[i] = static_cast<uint8_t>(byte_dist(rng_));
        }

        return data;
    }

    // ================================================================
    // Strategy 3: Valid header with extreme values
    // ================================================================
    std::vector<uint8_t> generateExtremeValues() {
        std::vector<uint8_t> data(sizeof(gguf::GGUFHeader), 0);
        auto* hdr = reinterpret_cast<gguf::GGUFHeader*>(data.data());
        hdr->magic = gguf::GGUF_MAGIC;

        // Random extreme versions
        std::uniform_int_distribution<int> choice(0, 4);
        switch (choice(rng_)) {
            case 0: hdr->version = 0; break;
            case 1: hdr->version = 1; break;
            case 2: hdr->version = 3; break;
            case 3: hdr->version = UINT32_MAX; break;
            case 4: hdr->version = 0xDEADBEEF; break;
        }

        // Extreme counts
        hdr->tensor_count = randomU64Choice({
            0, 1, UINT32_MAX, UINT64_MAX, 999999999ULL
        });
        hdr->metadata_kv_count = randomU64Choice({
            0, 1, UINT32_MAX, UINT64_MAX, 888888888ULL
        });

        return data;
    }

    // ================================================================
    // Strategy 4: Bit-flipped valid GGUF
    // ================================================================
    std::vector<uint8_t> generateBitFlipped() {
        // Start with a valid-looking GGUF
        std::vector<uint8_t> data(256, 0);
        auto* hdr = reinterpret_cast<gguf::GGUFHeader*>(data.data());
        hdr->magic = gguf::GGUF_MAGIC;
        hdr->version = 3;
        hdr->tensor_count = 2;
        hdr->metadata_kv_count = 1;

        // Add a valid metadata entry
        size_t offset = sizeof(gguf::GGUFHeader);
        auto* key = reinterpret_cast<gguf::GGUFString*>(data.data() + offset);
        key->length = 4;
        offset += sizeof(gguf::GGUFString);
        memcpy(data.data() + offset, "test", 4);
        offset += 4;
        *reinterpret_cast<uint32_t*>(data.data() + offset) = gguf::GGUF_TYPE_UINT32;
        offset += 4;
        *reinterpret_cast<uint32_t*>(data.data() + offset) = 42;

        // Flip random bits
        std::uniform_int_distribution<size_t> pos_dist(0, data.size() - 1);
        std::uniform_int_distribution<int> bit_dist(0, 7);
        int num_flips = std::uniform_int_distribution<int>(1, 8)(rng_);

        for (int i = 0; i < num_flips; i++) {
            size_t pos = pos_dist(rng_);
            int bit = bit_dist(rng_);
            data[pos] ^= (1 << bit);
        }

        return data;
    }

    // ================================================================
    // Strategy 5: Truncated valid GGUF
    // ================================================================
    std::vector<uint8_t> generateTruncated() {
        auto data = generateCorruptedMetadata();
        std::uniform_int_distribution<size_t> trunc_dist(0, data.size());
        size_t trunc_point = trunc_dist(rng_);
        data.resize(trunc_point);
        return data;
    }

    // ================================================================
    // Strategy 6: Zero-length and empty
    // ================================================================
    std::vector<uint8_t> generateEdgeCase() {
        std::uniform_int_distribution<int> choice(0, 3);
        switch (choice(rng_)) {
            case 0: return {};                              // Empty
            case 1: return { 0 };                           // Single zero byte
            case 2: return { 'G', 'G', 'U', 'F' };         // Magic only
            case 3: {
                // Header with zero counts
                std::vector<uint8_t> data(sizeof(gguf::GGUFHeader), 0);
                auto* hdr = reinterpret_cast<gguf::GGUFHeader*>(data.data());
                hdr->magic = gguf::GGUF_MAGIC;
                hdr->version = 3;
                return data;
            }
            default: return {};
        }
    }

    // Isolated SEH-protected parse (no C++ objects in this function)
    static bool tryParseWithSEH(const uint8_t* data, size_t len,
                                 gguf::ParseResult* outResult, bool* outCrashed) {
        *outCrashed = false;
        __try {
            *outResult = gguf::parseGGUFHeader(data, len);
        }
        __except(1) {
            *outCrashed = true;
        }
        return !(*outCrashed);
    }

    // ================================================================
    // Run fuzzing campaign
    // ================================================================
    FuzzStats run(uint64_t iterations = 100000, bool verbose = false) {
        FuzzStats stats;
        auto t0 = std::chrono::high_resolution_clock::now();

        printf("╔══════════════════════════════════════════════╗\n");
        printf("║  RawrXD GGUF Loader Fuzz Campaign            ║\n");
        printf("╚══════════════════════════════════════════════╝\n\n");
        printf("  Running %llu iterations...\n\n", (unsigned long long)iterations);

        for (uint64_t i = 0; i < iterations; i++) {
            std::vector<uint8_t> input;

            // Rotate through strategies
            switch (i % 6) {
                case 0: input = generateRandom(); break;
                case 1: input = generateCorruptedMetadata(); break;
                case 2: input = generateExtremeValues(); break;
                case 3: input = generateBitFlipped(); break;
                case 4: input = generateTruncated(); break;
                case 5: input = generateEdgeCase(); break;
            }

            // Run parser with SEH protection (isolated to avoid C++ unwind conflict)
            bool crashed = false;
            gguf::ParseResult result = {};

            tryParseWithSEH(input.data(), input.size(), &result, &crashed);

            if (crashed) {
                stats.crashes_caught++;
            }
                if (result.valid) {
                    stats.valid_parses++;
                } else {
                    stats.invalid_parses++;
                }

                // Check for buffer over-read (consumed more than available)
                if (result.bytes_consumed > input.size()) {
                    stats.buffer_overflows++;
                    if (verbose) {
                        printf("  [!] Buffer over-read at iteration %llu: "
                               "consumed %zu, available %zu\n",
                            (unsigned long long)i,
                            result.bytes_consumed, input.size());
                    }
                }
            }

            stats.total_iterations++;

            // Progress report every 10%
            if (i > 0 && i % (iterations / 10) == 0) {
                printf("  [%3llu%%] %llu iterations, %llu valid, %llu crashes\n",
                    (unsigned long long)(i * 100 / iterations),
                    (unsigned long long)stats.total_iterations,
                    (unsigned long long)stats.valid_parses,
                    (unsigned long long)stats.crashes_caught);
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        stats.total_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Report
        printf("\n─────────────────────────────────────────────\n");
        printf("  Iterations:      %llu\n", (unsigned long long)stats.total_iterations);
        printf("  Valid parses:    %llu (%.1f%%)\n",
            (unsigned long long)stats.valid_parses,
            100.0 * stats.valid_parses / stats.total_iterations);
        printf("  Invalid parses:  %llu\n", (unsigned long long)stats.invalid_parses);
        printf("  Crashes caught:  %llu\n", (unsigned long long)stats.crashes_caught);
        printf("  Buffer overflows:%llu\n", (unsigned long long)stats.buffer_overflows);
        printf("  Total time:      %.1f ms (%.0f iter/sec)\n",
            stats.total_time_ms,
            stats.total_iterations / (stats.total_time_ms / 1000.0));
        printf("─────────────────────────────────────────────\n");

        return stats;
    }

private:
    std::mt19937_64 rng_;

    uint64_t randomU64(uint64_t min, uint64_t max) {
        return std::uniform_int_distribution<uint64_t>(min, max)(rng_);
    }

    uint64_t randomU64Choice(std::initializer_list<uint64_t> choices) {
        std::vector<uint64_t> v(choices);
        return v[std::uniform_int_distribution<size_t>(0, v.size() - 1)(rng_)];
    }
};

} // namespace fuzz

// ================================================================
// Entry point
// ================================================================
int main(int argc, char* argv[]) {
    uint64_t iterations = 100000;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            iterations = strtoull(argv[i + 1], nullptr, 10);
            i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
    }

    fuzz::GGUFFuzzer fuzzer;
    auto stats = fuzzer.run(iterations, verbose);

    // Exit with error if any crashes or buffer overflows detected
    if (stats.crashes_caught > 0 || stats.buffer_overflows > 0) {
        printf("\n  [!] ISSUES DETECTED — Review parser robustness\n");
        return 1;
    }

    printf("\n  [OK] No crashes or buffer overflows detected\n");
    return 0;
}
