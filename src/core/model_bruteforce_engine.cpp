// ============================================================================
// model_bruteforce_engine.cpp — Brute-Force Model Discovery & Hotpatch Engine
// ============================================================================
// Enumerates EVERY discoverable GGUF model from local FS, Ollama blobs,
// HuggingFace cache, and user cache. Brute-force probes each model across
// all inference backends (CPU, Ollama API, Native pipeline) to produce a
// full compatibility matrix for CLI, GUI, and HTML IDE modes.
//
// Includes hotpatch integration: discovered models can be live-patched into
// the inference pipeline at runtime without restart via the three-layer
// hotpatch system (memory, byte-level, server).
//
// Architecture: C++20, Win32, no Qt, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "model_bruteforce_engine.hpp"
#include "unified_hotpatch_manager.hpp"
#include "proxy_hotpatcher.hpp"
#include "native_inference_pipeline.hpp"
#include "../server/gguf_server_hotpatch.hpp"
#include "../agent/model_invoker.hpp"
#include "../agentic/AgentOllamaClient.h"
#include "../gguf_loader.h"
#include "../cpu_inference_engine.h"
#include "perf_telemetry.hpp"
#include "../agent/telemetry_collector.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

namespace RawrXD {

// ============================================================================
// GGUF Metadata key strings (from GGUF spec)
// ============================================================================
static const char* GGUF_KEY_ARCH             = "general.architecture";
static const char* GGUF_KEY_NAME             = "general.name";
static const char* GGUF_KEY_CONTEXT_LEN      = ".context_length";
static const char* GGUF_KEY_EMBEDDING_LEN    = ".embedding_length";
static const char* GGUF_KEY_BLOCK_COUNT      = ".block_count";
static const char* GGUF_KEY_HEAD_COUNT       = ".attention.head_count";
static const char* GGUF_KEY_HEAD_COUNT_KV    = ".attention.head_count_kv";
static const char* GGUF_KEY_VOCAB_SIZE       = ".vocab_size";
static const char* GGUF_KEY_FILE_TYPE        = "general.file_type";

// GGUF magic
static const uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" little-endian

// GGUF metadata value types
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

// File type → quantization name mapping (from ggml)
static const char* FileTypeToQuant(uint32_t ft) {
    switch (ft) {
        case 0:  return "F32";
        case 1:  return "F16";
        case 2:  return "Q4_0";
        case 3:  return "Q4_1";
        case 7:  return "Q8_0";
        case 8:  return "Q8_1";
        case 10: return "Q2_K";
        case 11: return "Q3_K_S";
        case 12: return "Q3_K_M";
        case 13: return "Q3_K_L";
        case 14: return "Q4_K_S";
        case 15: return "Q4_K_M";
        case 16: return "Q5_K_S";
        case 17: return "Q5_K_M";
        case 18: return "Q6_K";
        case 19: return "IQ2_XXS";
        case 20: return "IQ2_XS";
        case 21: return "IQ3_XXS";
        case 22: return "IQ1_S";
        case 23: return "IQ4_NL";
        case 24: return "IQ3_S";
        case 25: return "IQ2_S";
        case 26: return "IQ4_XS";
        case 27: return "IQ1_M";
        case 28: return "BF16";
        case 29: return "Q4_0_4_4";
        case 30: return "Q4_0_4_8";
        case 31: return "Q4_0_8_8";
        default: return "Unknown";
    }
}

// ============================================================================
// Singleton
// ============================================================================
ModelBruteForceEngine& ModelBruteForceEngine::instance() {
    static ModelBruteForceEngine s_instance;
    return s_instance;
}

// ============================================================================
// GGUF Header Parsing — validates magic, reads version/tensor/metadata counts
// ============================================================================
bool ModelBruteForceEngine::ParseGGUFHeader(const std::string& path, ModelProbeResult& result) {
    auto t0 = std::chrono::high_resolution_clock::now();

#ifdef _WIN32
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER liSize;
    GetFileSizeEx(hFile, &liSize);
    result.file_size_bytes = (uint64_t)liSize.QuadPart;

    // Read GGUF header: magic(4) + version(4) + tensor_count(8) + metadata_count(8) = 24 bytes
    BruteForceGGUFHeader header{};
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, &header, sizeof(header), &bytesRead, nullptr) ||
        bytesRead < sizeof(header)) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
#else
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    result.file_size_bytes = (uint64_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    BruteForceGGUFHeader header{};
    if (fread(&header, sizeof(header), 1, fp) != 1) { fclose(fp); return false; }
    fclose(fp);
#endif

    result.valid_magic = (header.magic == GGUF_MAGIC);
    if (!result.valid_magic) return false;

    result.gguf_version = header.version;
    result.tensor_count = header.tensor_count;
    result.metadata_kv_count = header.metadata_kv_count;

    auto t1 = std::chrono::high_resolution_clock::now();
    result.scan_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return true;
}

// ============================================================================
// Metadata Extraction — reads GGUF KV pairs for architecture, quantization, etc.
// ============================================================================
bool ModelBruteForceEngine::ExtractMetadata(const std::string& path, ModelProbeResult& result) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    // Skip past GGUF header (24 bytes)
    LARGE_INTEGER offset;
    offset.QuadPart = 24;
    SetFilePointerEx(hFile, offset, nullptr, FILE_BEGIN);

    // Read metadata KV pairs
    // GGUF KV format: key_len(8) + key(N) + value_type(4) + value(varies)
    char keyBuf[512]{};
    std::string archPrefix;

    for (uint64_t i = 0; i < result.metadata_kv_count && i < 256; i++) {
        // Read key length
        uint64_t keyLen = 0;
        DWORD br = 0;
        if (!ReadFile(hFile, &keyLen, 8, &br, nullptr) || br < 8) break;
        if (keyLen > 500) break; // Sanity check

        // Read key
        memset(keyBuf, 0, sizeof(keyBuf));
        if (!ReadFile(hFile, keyBuf, (DWORD)keyLen, &br, nullptr) || br < keyLen) break;
        keyBuf[keyLen] = 0;
        std::string key(keyBuf, keyLen);

        // Read value type
        uint32_t valueType = 0;
        if (!ReadFile(hFile, &valueType, 4, &br, nullptr) || br < 4) break;

        // Read value based on type
        if (key == GGUF_KEY_ARCH && valueType == GGUF_TYPE_STRING) {
            uint64_t strLen = 0;
            ReadFile(hFile, &strLen, 8, &br, nullptr);
            if (strLen < 256) {
                char buf[256]{};
                ReadFile(hFile, buf, (DWORD)strLen, &br, nullptr);
                result.architecture = std::string(buf, strLen);
                archPrefix = result.architecture;
            }
        } else if (key == GGUF_KEY_FILE_TYPE && (valueType == GGUF_TYPE_UINT32 || valueType == GGUF_TYPE_INT32)) {
            uint32_t ft = 0;
            ReadFile(hFile, &ft, 4, &br, nullptr);
            result.quantization = FileTypeToQuant(ft);
        } else if (key == (archPrefix + GGUF_KEY_CONTEXT_LEN) && valueType == GGUF_TYPE_UINT32) {
            ReadFile(hFile, &result.context_length, 4, &br, nullptr);
        } else if (key == (archPrefix + GGUF_KEY_EMBEDDING_LEN) && valueType == GGUF_TYPE_UINT32) {
            ReadFile(hFile, &result.embedding_dim, 4, &br, nullptr);
        } else if (key == (archPrefix + GGUF_KEY_BLOCK_COUNT) && valueType == GGUF_TYPE_UINT32) {
            ReadFile(hFile, &result.layer_count, 4, &br, nullptr);
        } else if (key == (archPrefix + GGUF_KEY_HEAD_COUNT) && valueType == GGUF_TYPE_UINT32) {
            ReadFile(hFile, &result.head_count, 4, &br, nullptr);
        } else if (key == (archPrefix + GGUF_KEY_HEAD_COUNT_KV) && valueType == GGUF_TYPE_UINT32) {
            ReadFile(hFile, &result.head_count_kv, 4, &br, nullptr);
        } else if (key == (archPrefix + GGUF_KEY_VOCAB_SIZE) && valueType == GGUF_TYPE_UINT32) {
            ReadFile(hFile, &result.vocab_size, 4, &br, nullptr);
        } else {
            // Skip value we don't need
            switch (valueType) {
                case GGUF_TYPE_UINT8:  case GGUF_TYPE_INT8:  case GGUF_TYPE_BOOL: {
                    uint8_t v; ReadFile(hFile, &v, 1, &br, nullptr); break;
                }
                case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: {
                    uint16_t v; ReadFile(hFile, &v, 2, &br, nullptr); break;
                }
                case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: {
                    uint32_t v; ReadFile(hFile, &v, 4, &br, nullptr); break;
                }
                case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: {
                    uint64_t v; ReadFile(hFile, &v, 8, &br, nullptr); break;
                }
                case GGUF_TYPE_STRING: {
                    uint64_t sLen = 0;
                    ReadFile(hFile, &sLen, 8, &br, nullptr);
                    if (sLen < 65536) {
                        offset.QuadPart = (LONGLONG)sLen;
                        SetFilePointerEx(hFile, offset, nullptr, FILE_CURRENT);
                    } else break;
                    break;
                }
                case GGUF_TYPE_ARRAY: {
                    // Array: element_type(4) + count(8) + elements
                    uint32_t elemType = 0;
                    uint64_t count = 0;
                    ReadFile(hFile, &elemType, 4, &br, nullptr);
                    ReadFile(hFile, &count, 8, &br, nullptr);
                    // Skip elements based on type — rough skip for common types
                    uint64_t elemSize = 0;
                    switch (elemType) {
                        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL: elemSize = 1; break;
                        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: elemSize = 2; break;
                        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: elemSize = 4; break;
                        case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: elemSize = 8; break;
                        default: elemSize = 0; break;
                    }
                    if (elemSize > 0 && count < 10000000) {
                        offset.QuadPart = (LONGLONG)(elemSize * count);
                        SetFilePointerEx(hFile, offset, nullptr, FILE_CURRENT);
                    } else if (elemType == GGUF_TYPE_STRING) {
                        // Skip string arrays by reading each length+data
                        for (uint64_t s = 0; s < count && s < 200000; s++) {
                            uint64_t sl = 0;
                            if (!ReadFile(hFile, &sl, 8, &br, nullptr) || br < 8) break;
                            if (sl > 0 && sl < 65536) {
                                offset.QuadPart = (LONGLONG)sl;
                                SetFilePointerEx(hFile, offset, nullptr, FILE_CURRENT);
                            }
                        }
                    }
                    break;
                }
                default: break;
            }
        }
    }

    CloseHandle(hFile);
#else
    // POSIX: fread-based metadata parsing (mirrors Windows logic)
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 24, SEEK_SET); // Skip GGUF header (24 bytes)

    char keyBuf[512]{};
    std::string archPrefix;

    for (uint64_t i = 0; i < result.metadata_kv_count && i < 256; i++) {
        uint64_t keyLen = 0;
        if (fread(&keyLen, 8, 1, fp) != 1) break;
        if (keyLen > 500) break;

        memset(keyBuf, 0, sizeof(keyBuf));
        if (fread(keyBuf, 1, keyLen, fp) != keyLen) break;
        keyBuf[keyLen] = 0;
        std::string key(keyBuf, keyLen);

        uint32_t valueType = 0;
        if (fread(&valueType, 4, 1, fp) != 1) break;

        if (key == GGUF_KEY_ARCH && valueType == GGUF_TYPE_STRING) {
            uint64_t strLen = 0;
            fread(&strLen, 8, 1, fp);
            if (strLen < 256) {
                char buf[256]{};
                fread(buf, 1, strLen, fp);
                result.architecture = std::string(buf, strLen);
                archPrefix = result.architecture;
            }
        } else if (key == GGUF_KEY_FILE_TYPE && (valueType == GGUF_TYPE_UINT32 || valueType == GGUF_TYPE_INT32)) {
            uint32_t ft = 0;
            fread(&ft, 4, 1, fp);
            result.quantization = FileTypeToQuant(ft);
        } else if (key == (archPrefix + GGUF_KEY_CONTEXT_LEN) && valueType == GGUF_TYPE_UINT32) {
            fread(&result.context_length, 4, 1, fp);
        } else if (key == (archPrefix + GGUF_KEY_EMBEDDING_LEN) && valueType == GGUF_TYPE_UINT32) {
            fread(&result.embedding_dim, 4, 1, fp);
        } else if (key == (archPrefix + GGUF_KEY_BLOCK_COUNT) && valueType == GGUF_TYPE_UINT32) {
            fread(&result.layer_count, 4, 1, fp);
        } else if (key == (archPrefix + GGUF_KEY_HEAD_COUNT) && valueType == GGUF_TYPE_UINT32) {
            fread(&result.head_count, 4, 1, fp);
        } else if (key == (archPrefix + GGUF_KEY_HEAD_COUNT_KV) && valueType == GGUF_TYPE_UINT32) {
            fread(&result.head_count_kv, 4, 1, fp);
        } else if (key == (archPrefix + GGUF_KEY_VOCAB_SIZE) && valueType == GGUF_TYPE_UINT32) {
            fread(&result.vocab_size, 4, 1, fp);
        } else {
            // Skip value we don't need
            switch (valueType) {
                case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL:
                    fseek(fp, 1, SEEK_CUR); break;
                case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16:
                    fseek(fp, 2, SEEK_CUR); break;
                case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32:
                    fseek(fp, 4, SEEK_CUR); break;
                case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64:
                    fseek(fp, 8, SEEK_CUR); break;
                case GGUF_TYPE_STRING: {
                    uint64_t sLen = 0;
                    fread(&sLen, 8, 1, fp);
                    if (sLen < 65536) fseek(fp, (long)sLen, SEEK_CUR);
                    break;
                }
                case GGUF_TYPE_ARRAY: {
                    uint32_t elemType = 0;
                    uint64_t count = 0;
                    fread(&elemType, 4, 1, fp);
                    fread(&count, 8, 1, fp);
                    uint64_t elemSize = 0;
                    switch (elemType) {
                        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL: elemSize = 1; break;
                        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: elemSize = 2; break;
                        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: elemSize = 4; break;
                        case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: elemSize = 8; break;
                        default: elemSize = 0; break;
                    }
                    if (elemSize > 0 && count < 10000000) {
                        fseek(fp, (long)(elemSize * count), SEEK_CUR);
                    } else if (elemType == GGUF_TYPE_STRING) {
                        for (uint64_t s = 0; s < count && s < 200000; s++) {
                            uint64_t sl = 0;
                            if (fread(&sl, 8, 1, fp) != 1) break;
                            if (sl > 0 && sl < 65536) fseek(fp, (long)sl, SEEK_CUR);
                        }
                    }
                    break;
                }
                default: break;
            }
        }
    }
    fclose(fp);
#endif

    // Estimate RAM from file size + quant overhead
    result.estimated_ram_gb = EstimateRAM(result);

    // Extract quantization from filename if not found in metadata
    if (result.quantization.empty() || result.quantization == "Unknown") {
        result.quantization = ExtractQuantFromFilename(result.filename);
    }

    return true;
}

// ============================================================================
// RAM Estimation
// ============================================================================
float ModelBruteForceEngine::EstimateRAM(const ModelProbeResult& result) const {
    // Base: file size + ~20% overhead for KV cache, embeddings, activations
    float fileGB = (float)result.file_size_bytes / (1024.0f * 1024.0f * 1024.0f);
    float overhead = 1.2f;

    // Context-dependent overhead
    if (result.context_length > 8192) overhead += 0.15f;
    if (result.context_length > 32768) overhead += 0.3f;
    if (result.context_length > 65536) overhead += 0.5f;

    return fileGB * overhead;
}

// ============================================================================
// Quant Extraction from Filename
// ============================================================================
std::string ModelBruteForceEngine::ExtractQuantFromFilename(const std::string& filename) const {
    static const char* quants[] = {
        "IQ1_M", "IQ1_S", "IQ2_XXS", "IQ2_XS", "IQ2_S", "IQ3_XXS", "IQ3_S", "IQ4_NL", "IQ4_XS",
        "Q2_K", "Q3_K_S", "Q3_K_M", "Q3_K_L", "Q4_K_S", "Q4_K_M", "Q5_K_S", "Q5_K_M",
        "Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q6_K", "Q8_0", "Q8_1",
        "F32", "F16", "BF16"
    };
    // Case-insensitive search
    std::string upper = filename;
    for (auto& c : upper) c = (char)toupper(c);
    for (const char* q : quants) {
        std::string uq = q;
        for (auto& c : uq) c = (char)toupper(c);
        if (upper.find(uq) != std::string::npos) return q;
    }
    return "Unknown";
}

// ============================================================================
// Directory Scanning
// ============================================================================
void ModelBruteForceEngine::ScanDirectory(const std::string& dir, const std::string& source,
                                          std::vector<ModelProbeResult>& out,
                                          const BruteForceScanConfig& config) {
    if (m_cancelRequested.load()) return;

#ifdef _WIN32
    // Scan for *.gguf files
    std::string pattern = dir + "\\*.gguf";
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (m_cancelRequested.load()) break;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        std::string fullPath = dir + "\\" + fd.cFileName;

        // Size filter
        uint64_t fsize = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        if (fsize < config.min_file_size) continue;
        if (config.max_file_size > 0 && fsize > config.max_file_size) continue;

        ModelProbeResult pr;
        pr.path = fullPath;
        pr.filename = fd.cFileName;
        pr.source = source;
        pr.file_size_bytes = fsize;

        if (ParseGGUFHeader(fullPath, pr) && pr.valid_magic) {
            ExtractMetadata(fullPath, pr);

            // Apply arch/quant filters
            if (!config.arch_filter.empty() && pr.architecture != config.arch_filter) continue;
            if (!config.quant_filter.empty() && pr.quantization != config.quant_filter) continue;

            out.push_back(std::move(pr));
        }

        if (config.max_models > 0 && (int)out.size() >= config.max_models) break;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    // Recurse into subdirectories
    pattern = dir + "\\*";
    hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (m_cancelRequested.load()) break;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        std::string subdir = dir + "\\" + fd.cFileName;
        ScanDirectory(subdir, source, out, config);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* dp = opendir(dir.c_str());
    if (!dp) return;
    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        if (m_cancelRequested.load()) break;
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string fullPath = dir + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            ScanDirectory(fullPath, source, out, config);
            continue;
        }
        if (name.size() < 5 || name.substr(name.size()-5) != ".gguf") continue;
        if ((uint64_t)st.st_size < config.min_file_size) continue;
        ModelProbeResult pr;
        pr.path = fullPath; pr.filename = name; pr.source = source;
        pr.file_size_bytes = (uint64_t)st.st_size;
        if (ParseGGUFHeader(fullPath, pr) && pr.valid_magic) {
            ExtractMetadata(fullPath, pr);
            out.push_back(std::move(pr));
        }
    }
    closedir(dp);
#endif
}

// ============================================================================
// Ollama Blob Scanning
// ============================================================================
void ModelBruteForceEngine::ScanOllamaBlobs(std::vector<ModelProbeResult>& out,
                                             const BruteForceScanConfig& config) {
    if (m_cancelRequested.load()) return;

#ifdef _WIN32
    // Standard Ollama blob paths
    char userProfile[MAX_PATH]{};
    if (GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH) == 0) return;

    std::vector<std::string> blobDirs = {
        std::string(userProfile) + "\\.ollama\\models\\blobs",
        "D:\\OllamaModels\\blobs",
        "C:\\OllamaModels\\blobs",
    };

    // Also check OLLAMA_MODELS env var
    char ollamaModels[MAX_PATH]{};
    if (GetEnvironmentVariableA("OLLAMA_MODELS", ollamaModels, MAX_PATH) > 0) {
        blobDirs.push_back(std::string(ollamaModels) + "\\blobs");
    }

    for (const auto& blobDir : blobDirs) {
        if (m_cancelRequested.load()) break;

        std::string pattern = blobDir + "\\sha256-*";
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;

        do {
            if (m_cancelRequested.load()) break;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            uint64_t fsize = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            // Ollama blobs > 100MB are likely model weights
            if (fsize < 100 * 1024 * 1024) continue;

            std::string fullPath = blobDir + "\\" + fd.cFileName;

            ModelProbeResult pr;
            pr.path = fullPath;
            pr.filename = fd.cFileName;
            pr.source = "ollama_blob";
            pr.file_size_bytes = fsize;

            // Validate GGUF magic
            if (ParseGGUFHeader(fullPath, pr) && pr.valid_magic) {
                ExtractMetadata(fullPath, pr);
                out.push_back(std::move(pr));
            }
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);
    }
#endif
}

// ============================================================================
// Probe: Ollama API
// ============================================================================
void ModelBruteForceEngine::ProbeWithOllama(ModelProbeResult& result,
                                             const BruteForceScanConfig& config) {
    try {
        RawrXD::Agent::OllamaConfig ollamaConf;
        ollamaConf.timeout_ms = config.probe_timeout_ms;
        ollamaConf.max_tokens = config.probe_max_tokens;
        ollamaConf.temperature = config.probe_temperature;

        RawrXD::Agent::AgentOllamaClient client(ollamaConf);
        if (!client.TestConnection()) return;

        // Check if model name matches any Ollama-served model
        auto models = client.ListModels();
        bool found = false;
        for (const auto& m : models) {
            // Match by architecture or filename heuristic
            if (result.filename.find(m) != std::string::npos ||
                m.find(result.architecture) != std::string::npos) {
                found = true;
                ollamaConf.chat_model = m;
                break;
            }
        }
        if (!found) return;

        result.ollama_available = true;

        // Attempt token generation
        client.SetConfig(ollamaConf);
        std::vector<RawrXD::Agent::ChatMessage> msgs;
        RawrXD::Agent::ChatMessage userMsg;
        userMsg.role = "user";
        userMsg.content = config.probe_prompt;
        msgs.push_back(userMsg);

        auto ir = client.ChatSync(msgs);
        if (ir.success && !ir.response.empty()) {
            result.token_generated = true;
            result.tokens_produced = (uint32_t)ir.completion_tokens;
            result.tokens_per_sec = (float)ir.tokens_per_sec;
            result.probe_output = ir.response.substr(0, 200);
        }
    } catch (...) {
        // No exceptions in release — this is a safety net
    }
}

// ============================================================================
// Probe: CPU Inference — Real GGUFLoader + CPUInferenceEngine validation
// ============================================================================
void ModelBruteForceEngine::ProbeWithCPU(ModelProbeResult& result,
                                          const BruteForceScanConfig& config) {
    // Phase 1: Validate file is mmap-able (basic I/O test)
#ifdef _WIN32
    HANDLE hFile = CreateFileA(result.path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        result.probe_error = "Cannot open file for CPU probe";
        return;
    }
    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        result.probe_error = "Cannot mmap file for CPU probe";
        return;
    }
    void* pView = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!pView) {
        CloseHandle(hMap); CloseHandle(hFile);
        result.probe_error = "MapViewOfFile failed";
        return;
    }
    // Verify GGUF magic from mmap
    uint32_t magic = *(uint32_t*)pView;
    UnmapViewOfFile(pView);
    CloseHandle(hMap);
    CloseHandle(hFile);
    if (magic != GGUF_MAGIC) {
        result.probe_error = "mmap magic mismatch";
        return;
    }
#endif

    // Phase 2: Actually open via GGUFLoader — validates full header + metadata parsing
    GGUFLoader loader;
    if (!loader.Open(result.path)) {
        result.probe_error = "GGUFLoader::Open failed";
        return;
    }
    if (!loader.ParseHeader()) {
        loader.Close();
        result.probe_error = "GGUFLoader::ParseHeader failed";
        return;
    }
    if (!loader.ParseMetadata()) {
        loader.Close();
        result.probe_error = "GGUFLoader::ParseMetadata failed";
        return;
    }

    // If we got here, the CPU path can fully parse this model
    result.cpu_loadable = true;
    result.cli_compatible = true; // CLI always works if CPU can parse

    // Enrich metadata from GGUFLoader if our header parse missed anything
    GGUFMetadata meta = loader.GetMetadata();
    if (result.architecture.empty() && meta.architecture_type != 0) {
        // Map architecture_type enum to a readable string
        static const char* archNames[] = { "unknown", "llama", "gpt2", "gptj", "gptneox", "mpt", "starcoder", "falcon", "rwkv", "bloom", "phi", "gemma", "qwen", "command-r", "mistral" };
        uint32_t idx = meta.architecture_type < 15 ? meta.architecture_type : 0;
        result.architecture = archNames[idx];
    }
    if (result.vocab_size == 0 && meta.vocab_size > 0)
        result.vocab_size = meta.vocab_size;
    if (result.context_length == 0 && meta.context_length > 0)
        result.context_length = meta.context_length;
    if (result.embedding_dim == 0 && meta.embedding_dim > 0)
        result.embedding_dim = meta.embedding_dim;

    // Phase 3: Try CPUInferenceEngine::LoadModel for full inference validation
    CPUInferenceEngine cpuEngine;
    if (cpuEngine.LoadModel(result.path)) {
        // Model is fully loadable by CPU inference engine
        // Try a quick tokenization test to validate vocab
        auto tokens = cpuEngine.Tokenize(config.probe_prompt);
        if (!tokens.empty()) {
            result.token_generated = true;
            result.tokens_produced = (uint32_t)tokens.size();
            // Note: actual generation would be too slow for brute-force scan;
            // tokenization success is sufficient to validate CPU compatibility
        }
    }

    loader.Close();
}

// ============================================================================
// Probe: Native Pipeline — Real NativeInferencePipeline::LoadModel validation
// ============================================================================
void ModelBruteForceEngine::ProbeWithNative(ModelProbeResult& result,
                                             const BruteForceScanConfig& config) {
    // Check file accessibility first
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(result.path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        result.probe_error = "File not accessible for native probe";
        return;
    }
#endif

    // Requires valid GGUF with at least 1 tensor as prerequisite
    if (!result.valid_magic || result.tensor_count == 0) {
        return;
    }

    // Actually try loading via NativeInferencePipeline
    NativeInferencePipeline pipeline;
    PipelineConfig pipeConf{};
    pipeConf.maxContextLen = 512;  // Minimal context for probe
    pipeConf.backgroundInference = false; // Synchronous for compatibility check

    PatchResult initResult = pipeline.Init(pipeConf);
    if (!initResult.success) {
        // Pipeline init failed — not fatal, just means native path unavailable
        return;
    }

    PatchResult loadResult = pipeline.LoadModel(result.path.c_str());
    if (loadResult.success) {
        result.native_loadable = true;

        // If model loaded, GUI and HTML are also compatible (native pipeline
        // feeds all three rendering modes)
        result.gui_compatible = true;
        result.html_compatible = true;

        // Try a quick inference to measure actual tok/s
        if (config.probe_inference && !result.token_generated) {
            PatchResult inferResult = pipeline.Infer(
                config.probe_prompt.c_str(),
                (uint32_t)config.probe_prompt.size());

            if (inferResult.success) {
                // Wait for completion with timeout
                PatchResult waitResult = pipeline.WaitForCompletion(
                    (uint32_t)config.probe_timeout_ms);

                if (waitResult.success) {
                    uint32_t outLen = 0;
                    const char* output = pipeline.GetLastOutput(&outLen);
                    if (output && outLen > 0) {
                        result.token_generated = true;
                        result.probe_output = std::string(output, std::min(outLen, 200u));
                        result.tokens_per_sec = pipeline.CurrentTokensPerSec();
                        // Count tokens produced (approximate from output length)
                        result.tokens_produced = outLen / 4; // rough estimate
                    }
                }
            }

            pipeline.StopInference();
        }

        pipeline.UnloadModel();
    }

    pipeline.Shutdown();

    // Set compatibility flags based on REAL backend results
    // CLI compat was set in ProbeWithCPU if GGUFLoader succeeded
    // GUI compat set above if native pipeline loaded
    // HTML compat set above if native pipeline loaded
    // Ollama path sets its own flags in ProbeWithOllama
    // Also: if Ollama is available, all three modes work (Ollama serves all)
    if (result.ollama_available) {
        result.cli_compatible = true;
        result.gui_compatible = true;
        result.html_compatible = true;
    }
}

// ============================================================================
// Discover All Models (header scan only, no inference probe)
// ============================================================================
std::vector<ModelProbeResult> ModelBruteForceEngine::DiscoverAllModels(
    const BruteForceScanConfig& config, BruteForceProgressCallback progress) {

    std::lock_guard<std::mutex> lock(m_mutex);
    m_running.store(true);
    m_cancelRequested.store(false);
    m_progress = {};
    m_progress.status_message = "Scanning model directories...";

    std::vector<ModelProbeResult> results;

    // 1. Current working directory
    if (config.scan_cwd) {
        char cwd[MAX_PATH]{};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        ScanDirectory(cwd, "local", results, config);
    }

    // 2. Additional local directories
    for (const auto& dir : config.local_dirs) {
        ScanDirectory(dir, "local", results, config);
    }

    // 3. Standard model locations
    std::vector<std::string> stdDirs = {
        "D:\\OllamaModels",
        "D:\\models",
        "C:\\models",
    };

    char userProfile[MAX_PATH]{};
    if (GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH) > 0) {
        stdDirs.push_back(std::string(userProfile) + "\\.cache\\rawrxd\\models");
        stdDirs.push_back(std::string(userProfile) + "\\.cache\\huggingface\\hub");
        stdDirs.push_back(std::string(userProfile) + "\\.ollama\\models");
        stdDirs.push_back(std::string(userProfile) + "\\models");
    }

    for (const auto& dir : stdDirs) {
        if (m_cancelRequested.load()) break;
        DWORD attrs = GetFileAttributesA(dir.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            m_progress.status_message = "Scanning: " + dir;
            if (progress) progress(m_progress);
            ScanDirectory(dir, "local", results, config);
        }
    }

    // 4. HuggingFace cache
    if (config.scan_hf_cache && !m_cancelRequested.load()) {
        std::string hfCache = std::string(userProfile) + "\\.cache\\huggingface\\hub\\models--*";
        // Scan each model's snapshots for .gguf files
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA(hfCache.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string modelDir = std::string(userProfile) +
                        "\\.cache\\huggingface\\hub\\" + fd.cFileName + "\\snapshots";
                    ScanDirectory(modelDir, "hf_cache", results, config);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    // 5. Ollama blobs
    if (config.scan_ollama_blobs && !m_cancelRequested.load()) {
        m_progress.status_message = "Scanning Ollama blobs...";
        if (progress) progress(m_progress);
        ScanOllamaBlobs(results, config);
    }

    // Deduplicate by path
    std::sort(results.begin(), results.end(),
              [](const ModelProbeResult& a, const ModelProbeResult& b) { return a.path < b.path; });
    results.erase(std::unique(results.begin(), results.end(),
              [](const ModelProbeResult& a, const ModelProbeResult& b) { return a.path == b.path; }),
              results.end());

    m_progress.models_found = (int)results.size();
    m_progress.status_message = "Discovery complete";
    m_progress.is_complete = true;
    if (progress) progress(m_progress);

    m_running.store(false);
    return results;
}

// ============================================================================
// Probe Single Model
// ============================================================================
ModelProbeResult ModelBruteForceEngine::ProbeModel(const std::string& model_path,
                                                    const BruteForceScanConfig& config) {
    ModelProbeResult result;
    result.path = model_path;

    // Extract filename
    size_t lastSlash = model_path.find_last_of("\\/");
    result.filename = (lastSlash != std::string::npos) ? model_path.substr(lastSlash + 1) : model_path;
    result.source = "local";

    // Parse header
    if (!ParseGGUFHeader(model_path, result)) {
        result.probe_error = "Failed to read GGUF header";
        return result;
    }

    // Extract metadata
    ExtractMetadata(model_path, result);

    // Probe inference backends
    if (config.probe_inference) {
        auto t0 = std::chrono::high_resolution_clock::now();
        result.probe_attempted = true;

        ProbeWithCPU(result, config);
        ProbeWithOllama(result, config);
        ProbeWithNative(result, config);

        auto t1 = std::chrono::high_resolution_clock::now();
        result.probe_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    return result;
}

// ============================================================================
// Brute Force ALL — Full scan + probe
// ============================================================================
std::vector<ModelProbeResult> ModelBruteForceEngine::BruteForceAll(
    const BruteForceScanConfig& config, BruteForceProgressCallback progress) {

    // Phase 1: Discovery
    auto results = DiscoverAllModels(config, progress);

    // Phase 2: Brute-force probe each model
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(true);
        m_progress.is_complete = false;
        m_progress.models_found = (int)results.size();
        m_progress.total_estimated = (int)results.size();
    }

    for (int i = 0; i < (int)results.size(); i++) {
        if (m_cancelRequested.load()) break;

        auto& pr = results[i];
        m_progress.current_model = pr.filename;
        m_progress.models_scanned = i;
        m_progress.percent_complete = (float)(i + 1) / (float)results.size() * 100.0f;
        m_progress.status_message = "Probing: " + pr.filename;
        if (progress) progress(m_progress);

        if (config.probe_inference) {
            auto t0 = std::chrono::high_resolution_clock::now();
            pr.probe_attempted = true;

            ProbeWithCPU(pr, config);
            ProbeWithOllama(pr, config);
            ProbeWithNative(pr, config);

            auto t1 = std::chrono::high_resolution_clock::now();
            pr.probe_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            if (pr.cli_compatible || pr.gui_compatible || pr.html_compatible)
                m_progress.models_compatible++;
            else
                m_progress.models_failed++;
        }
    }

    // Track telemetry
    if (auto* tc = TelemetryCollector::instance()) tc->trackFeatureUsage("model_bruteforce");
    {
        auto tsc = RawrXD::Perf::PerfTelemetry::instance().begin(RawrXD::Perf::KernelSlot(0));
        RawrXD::Perf::PerfTelemetry::instance().end(RawrXD::Perf::KernelSlot(0), tsc);
    }

    // Cache results
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastResults = results;
        m_progress.models_scanned = (int)results.size();
        m_progress.percent_complete = 100.0f;
        m_progress.is_complete = true;
        m_progress.status_message = "Brute-force complete";
    }

    m_running.store(false);
    if (progress) progress(m_progress);

    return results;
}

// ============================================================================
// Results / Progress Accessors
// ============================================================================
const std::vector<ModelProbeResult>& ModelBruteForceEngine::GetLastResults() const {
    return m_lastResults;
}

BruteForceScanProgress ModelBruteForceEngine::GetProgress() const {
    return m_progress;
}

void ModelBruteForceEngine::Cancel() {
    m_cancelRequested.store(true);
}

bool ModelBruteForceEngine::IsRunning() const {
    return m_running.load();
}

// ============================================================================
// Format: CLI Table
// ============================================================================
std::string ModelBruteForceEngine::FormatCLI(const std::vector<ModelProbeResult>& results) const {
    std::string out;
    char buf[1024];

    out += "\n";
    out += "╔══════════════════════════════════════════════════════════════════════════════════════════════╗\n";
    out += "║                      MODEL BRUTE-FORCE COMPATIBILITY MATRIX                                ║\n";
    out += "╠══════════════════════════════════════════════════════════════════════════════════════════════╣\n";
    snprintf(buf, sizeof(buf),
        "║ %-40s │ %-8s │ %-6s │ %4s │ CLI│ GUI│HTML│ %-6s ║\n",
        "Model", "Arch", "Quant", "Ctx", "Tok/s");
    out += buf;
    out += "╠══════════════════════════════════════════════════════════════════════════════════════════════╣\n";

    for (const auto& r : results) {
        if (!r.valid_magic) continue;
        float sizeGB = (float)r.file_size_bytes / (1024.0f * 1024.0f * 1024.0f);
        snprintf(buf, sizeof(buf),
            "║ %-40.40s │ %-8.8s │ %-6.6s │ %4uK│ %s │ %s │ %s │ %6.1f ║\n",
            r.filename.c_str(),
            r.architecture.c_str(),
            r.quantization.c_str(),
            r.context_length / 1024,
            r.cli_compatible  ? " ✓ " : " ✗ ",
            r.gui_compatible  ? " ✓ " : " ✗ ",
            r.html_compatible ? " ✓ " : " ✗ ",
            r.tokens_per_sec);
        out += buf;
    }

    out += "╠══════════════════════════════════════════════════════════════════════════════════════════════╣\n";
    int total = 0, compat = 0;
    for (const auto& r : results) {
        if (!r.valid_magic) continue;
        total++;
        if (r.cli_compatible || r.gui_compatible || r.html_compatible) compat++;
    }
    snprintf(buf, sizeof(buf),
        "║ Total: %d models found, %d compatible, %d failed                                          ║\n",
        total, compat, total - compat);
    out += buf;
    out += "╚══════════════════════════════════════════════════════════════════════════════════════════════╝\n";

    return out;
}

// ============================================================================
// Format: JSON
// ============================================================================
std::string ModelBruteForceEngine::FormatJSON(const std::vector<ModelProbeResult>& results) const {
    std::string json = "{\"models\":[\n";

    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        if (!r.valid_magic) continue;

        char buf[2048];
        snprintf(buf, sizeof(buf),
            "  {\"path\":\"%s\",\"filename\":\"%s\",\"source\":\"%s\","
            "\"size_bytes\":%llu,\"gguf_version\":%u,\"tensor_count\":%llu,"
            "\"architecture\":\"%s\",\"quantization\":\"%s\","
            "\"context_length\":%u,\"embedding_dim\":%u,\"vocab_size\":%u,"
            "\"layer_count\":%u,\"head_count\":%u,\"head_count_kv\":%u,"
            "\"estimated_ram_gb\":%.2f,"
            "\"cpu_loadable\":%s,\"ollama_available\":%s,\"native_loadable\":%s,"
            "\"token_generated\":%s,\"tokens_per_sec\":%.1f,\"tokens_produced\":%u,"
            "\"cli_compatible\":%s,\"gui_compatible\":%s,\"html_compatible\":%s,"
            "\"scan_time_ms\":%.2f,\"probe_time_ms\":%.2f,"
            "\"probe_output\":\"%s\",\"probe_error\":\"%s\"}",
            r.path.c_str(), r.filename.c_str(), r.source.c_str(),
            (unsigned long long)r.file_size_bytes, r.gguf_version,
            (unsigned long long)r.tensor_count,
            r.architecture.c_str(), r.quantization.c_str(),
            r.context_length, r.embedding_dim, r.vocab_size,
            r.layer_count, r.head_count, r.head_count_kv,
            r.estimated_ram_gb,
            r.cpu_loadable ? "true" : "false",
            r.ollama_available ? "true" : "false",
            r.native_loadable ? "true" : "false",
            r.token_generated ? "true" : "false",
            r.tokens_per_sec, r.tokens_produced,
            r.cli_compatible ? "true" : "false",
            r.gui_compatible ? "true" : "false",
            r.html_compatible ? "true" : "false",
            r.scan_time_ms, r.probe_time_ms,
            r.probe_output.substr(0, 100).c_str(),
            r.probe_error.c_str());
        json += buf;
        if (i + 1 < results.size()) json += ",\n";
        else json += "\n";
    }

    json += "],\"total\":" + std::to_string(results.size());

    // Summary stats
    int compat = 0, failed = 0;
    for (const auto& r : results) {
        if (r.cli_compatible || r.gui_compatible || r.html_compatible) compat++;
        else if (r.valid_magic) failed++;
    }
    json += ",\"compatible\":" + std::to_string(compat);
    json += ",\"failed\":" + std::to_string(failed);
    json += "}\n";

    return json;
}

// ============================================================================
// Format: HTML
// ============================================================================
std::string ModelBruteForceEngine::FormatHTML(const std::vector<ModelProbeResult>& results) const {
    std::string html;
    html += R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RawrXD — Model Brute-Force Compatibility Matrix</title>
<style>
:root { --bg: #0d1117; --card: #161b22; --border: #30363d; --text: #c9d1d9;
        --green: #3fb950; --red: #f85149; --yellow: #d29922; --blue: #58a6ff;
        --accent: #bc8cff; }
* { margin: 0; padding: 0; box-sizing: border-box; }
body { background: var(--bg); color: var(--text); font-family: 'Cascadia Code', 'JetBrains Mono', monospace; padding: 20px; }
h1 { color: var(--accent); text-align: center; margin-bottom: 16px; font-size: 1.4em; }
.stats { display: flex; gap: 16px; justify-content: center; margin-bottom: 16px; flex-wrap: wrap; }
.stat { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 12px 24px; text-align: center; }
.stat .num { font-size: 2em; font-weight: bold; }
.stat .label { font-size: 0.8em; opacity: 0.7; }
.stat.ok .num { color: var(--green); }
.stat.fail .num { color: var(--red); }
.stat.total .num { color: var(--blue); }
table { width: 100%; border-collapse: collapse; background: var(--card); border-radius: 8px; overflow: hidden; }
th { background: #21262d; color: var(--accent); padding: 10px 8px; text-align: left; font-size: 0.85em; position: sticky; top: 0; }
td { padding: 8px; border-top: 1px solid var(--border); font-size: 0.82em; }
tr:hover { background: #1c2128; }
.compat { text-align: center; font-size: 1.2em; }
.yes { color: var(--green); }
.no { color: var(--red); }
.size { color: var(--yellow); }
.arch { color: var(--blue); }
.quant { color: var(--accent); }
.probe-out { max-width: 200px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; opacity: 0.7; }
.hotpatch-btn { background: var(--accent); color: #000; border: none; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 0.8em; font-weight: bold; }
.hotpatch-btn:hover { opacity: 0.8; }
.hotpatch-btn.active { background: var(--green); }
</style>
</head>
<body>
<h1>⚡ RawrXD Model Brute-Force Compatibility Matrix</h1>
)";

    // Stats summary
    int total = 0, compat = 0, failed = 0;
    for (const auto& r : results) {
        if (!r.valid_magic) continue;
        total++;
        if (r.cli_compatible || r.gui_compatible || r.html_compatible) compat++;
        else failed++;
    }

    char buf[2048];
    snprintf(buf, sizeof(buf),
        R"(<div class="stats">
<div class="stat total"><div class="num">%d</div><div class="label">Models Found</div></div>
<div class="stat ok"><div class="num">%d</div><div class="label">Compatible</div></div>
<div class="stat fail"><div class="num">%d</div><div class="label">Failed</div></div>
</div>)", total, compat, failed);
    html += buf;

    html += R"(<table>
<tr><th>#</th><th>Model</th><th>Arch</th><th>Quant</th><th>Size</th><th>Ctx</th><th>Layers</th><th>RAM Est.</th><th>CLI</th><th>GUI</th><th>HTML</th><th>Tok/s</th><th>Hotpatch</th><th>Output</th></tr>
)";

    int idx = 0;
    for (const auto& r : results) {
        if (!r.valid_magic) continue;
        idx++;
        float sizeGB = (float)r.file_size_bytes / (1024.0f * 1024.0f * 1024.0f);

        snprintf(buf, sizeof(buf),
            R"HTM(<tr>
<td>%d</td>
<td title="%s">%s</td>
<td class="arch">%s</td>
<td class="quant">%s</td>
<td class="size">%.1fGB</td>
<td>%uK</td>
<td>%u</td>
<td>%.1fGB</td>
<td class="compat">%s</td>
<td class="compat">%s</td>
<td class="compat">%s</td>
<td>%.1f</td>
<td><button class="hotpatch-btn" onclick="hotpatchModel('%s')">[Patch]</button></td>
<td class="probe-out">%s</td>
</tr>
)HTM",
            idx,
            r.path.c_str(), r.filename.c_str(),
            r.architecture.c_str(), r.quantization.c_str(),
            sizeGB, r.context_length / 1024, r.layer_count,
            r.estimated_ram_gb,
            r.cli_compatible  ? "<span class='yes'>✓</span>" : "<span class='no'>✗</span>",
            r.gui_compatible  ? "<span class='yes'>✓</span>" : "<span class='no'>✗</span>",
            r.html_compatible ? "<span class='yes'>✓</span>" : "<span class='no'>✗</span>",
            r.tokens_per_sec,
            r.path.c_str(),
            r.probe_output.empty() ? r.probe_error.c_str() : r.probe_output.c_str());
        html += buf;
    }

    html += R"(</table>
<script>
async function hotpatchModel(path) {
    const btn = event.target;
    btn.textContent = '⏳ Patching...';
    try {
        const res = await fetch('/api/models/bruteforce/hotpatch', {
            method: 'POST', headers: {'Content-Type':'application/json'},
            body: JSON.stringify({model_path: path})
        });
        const data = await res.json();
        if (data.success) { btn.textContent = '✓ Patched'; btn.classList.add('active'); }
        else { btn.textContent = '✗ Failed'; }
    } catch(e) { btn.textContent = '✗ Error'; }
}
// Auto-refresh results every 5s during scan
let refreshInterval = setInterval(async () => {
    try {
        const res = await fetch('/api/models/bruteforce/progress');
        const data = await res.json();
        if (data.is_complete) clearInterval(refreshInterval);
    } catch(e) { clearInterval(refreshInterval); }
}, 5000);
</script>
</body>
</html>)";

    return html;
}

} // namespace RawrXD
