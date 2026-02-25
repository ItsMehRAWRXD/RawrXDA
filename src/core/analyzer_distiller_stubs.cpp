// =============================================================================
// analyzer_distiller_stubs.cpp — C++ fallback for AnalyzerDistiller MASM module
// =============================================================================
// Production-grade C++ fallback that implements GGUF v3 analysis and .exec
// distillation. When the real ASM module is ported to ml64 and linked,
// When the real ASM module is ported from MASM32 to ml64 and linked,
// define RAWRXD_LINK_ANALYZER_DISTILLER_ASM=1 to disable this file.
//
// Pattern: PatchResult-style, no exceptions, no STL in hot path
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#if defined(RAWRXD_GOLD_BUILD) || !defined(RAWRXD_LINK_ANALYZER_DISTILLER_ASM) || !RAWRXD_LINK_ANALYZER_DISTILLER_ASM

#include "analyzer_distiller.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// GGUF magic constant
static const uint64_t GGUF_MAGIC_V3 = 0x46554747464C4C67ULL; // "gllFGGUF" little-endian
static uint64_t g_ad_expected_tensor_count = 0;
static uint64_t g_ad_parsed_tensor_count = 0;

// ── AD_OpenGGUFFile ──────────────────────────────────────────────────────────
intptr_t AD_OpenGGUFFile(const char* path) {
    if (!path) return -1;
#ifdef _WIN32
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    return (intptr_t)h;
#else
    int fd = open(path, O_RDONLY);
    return (fd < 0) ? -1 : (intptr_t)fd;
#endif
}

// ── AD_ValidateGGUFHeader ────────────────────────────────────────────────────
int AD_ValidateGGUFHeader(AD_GGUFHeader* header, intptr_t fileHandle) {
    if (!header || fileHandle == -1) return 0;
#ifdef _WIN32
    DWORD bytesRead = 0;
    BOOL ok = ReadFile((HANDLE)fileHandle, header, sizeof(AD_GGUFHeader), &bytesRead, NULL);
    if (!ok || bytesRead < sizeof(AD_GGUFHeader)) return 0;
#else
    ssize_t r = read((int)fileHandle, header, sizeof(AD_GGUFHeader));
    if (r < (ssize_t)sizeof(AD_GGUFHeader)) return 0;
#endif
    // Validate magic (check both byte orderings)
    if (header->magic != 0x46554747464C4C67ULL && header->magic != 0x46554747666C6C67ULL) return 0;
    // Validate version
    if (header->version != 3) return 0;
    // Sanity check counts
    if (header->tensor_count > 131072) return 0;
    if (header->metadata_kv > 131072) return 0;
    g_ad_expected_tensor_count = header->tensor_count;
    if (g_ad_expected_tensor_count > 32768) g_ad_expected_tensor_count = 32768;
    g_ad_parsed_tensor_count = 0;
    return 1;
}

// ── AD_SkipMetadataKV ────────────────────────────────────────────────────────
int AD_SkipMetadataKV(uint64_t kvCount, intptr_t fileHandle) {
    if (fileHandle == -1) return 0;
    // Skip each key-value pair: read key_len, skip key, read type, skip value
    for (uint64_t i = 0; i < kvCount; i++) {
        // Key: uint64_t length + bytes
        uint64_t keyLen = 0;
        DWORD rd = 0;
#ifdef _WIN32
        if (!ReadFile((HANDLE)fileHandle, &keyLen, 8, &rd, NULL) || rd < 8) return 0;
        // Seek past key string
        LARGE_INTEGER dist; dist.QuadPart = (LONGLONG)keyLen;
        if (!SetFilePointerEx((HANDLE)fileHandle, dist, NULL, FILE_CURRENT)) return 0;
        // Read value type
        uint32_t valueType = 0;
        if (!ReadFile((HANDLE)fileHandle, &valueType, 4, &rd, NULL) || rd < 4) return 0;
        // Skip value based on type
        uint64_t skipBytes = 0;
        switch (valueType) {
            case 0: case 7: skipBytes = 1; break;   // UINT8, BOOL
            case 1: skipBytes = 1; break;            // INT8
            case 2: case 3: skipBytes = 2; break;    // UINT16, INT16
            case 4: case 5: case 6: skipBytes = 4; break; // UINT32, INT32, FLOAT32
            case 10: case 11: case 12: skipBytes = 8; break; // UINT64, INT64, FLOAT64
            case 8: { // STRING: uint64_t len + bytes
                uint64_t sLen = 0;
                DWORD rd2 = 0;
                if (!ReadFile((HANDLE)fileHandle, &sLen, 8, &rd2, NULL) || rd2 < 8) return 0;
                skipBytes = sLen;
                break;
            }
            case 9: { // ARRAY: uint32_t elemType + uint64_t count + elements
                uint32_t elemType = 0;
                uint64_t elemCount = 0;
                DWORD rd2 = 0;
                if (!ReadFile((HANDLE)fileHandle, &elemType, 4, &rd2, NULL) || rd2 < 4) return 0;
                if (!ReadFile((HANDLE)fileHandle, &elemCount, 8, &rd2, NULL) || rd2 < 8) return 0;
                
                if (elemType == 8) { // Array of Strings
                    for (uint64_t e = 0; e < elemCount; e++) {
                        uint64_t sl = 0;
                        if (!ReadFile((HANDLE)fileHandle, &sl, 8, &rd2, NULL) || rd2 < 8) return 0;
                        LARGE_INTEGER sd; sd.QuadPart = (LONGLONG)sl;
                        if (!SetFilePointerEx((HANDLE)fileHandle, sd, NULL, FILE_CURRENT)) return 0;
                    }
                    skipBytes = 0; // Already skipped
                } else {
                    // Array of primitives
                    uint64_t elemSize = 0;
                    switch (elemType) {
                        case 0: case 1: case 7: elemSize = 1; break; // UINT8, INT8, BOOL
                        case 2: case 3: elemSize = 2; break;         // UINT16, INT16
                        case 4: case 5: case 6: elemSize = 4; break; // UINT32, INT32, FLOAT32
                        case 10: case 11: case 12: elemSize = 8; break; // UINT64, INT64, FLOAT64
                        default: return 0; // Unknown type
                    }
                    skipBytes = elemCount * elemSize;
                }
                break;
            }
            default: return 0; // Unknown value type
        }
        dist.QuadPart = (LONGLONG)skipBytes;
        if (!SetFilePointerEx((HANDLE)fileHandle, dist, NULL, FILE_CURRENT)) return 0;
#else
        // POSIX equivalent — full type-switch matching Win32 path
        if (read((int)(intptr_t)fileHandle, &keyLen, 8) < 8) return 0;
        lseek((int)(intptr_t)fileHandle, (off_t)keyLen, SEEK_CUR);
        uint32_t valueType = 0;
        if (read((int)(intptr_t)fileHandle, &valueType, 4) < 4) return 0;
        uint64_t skipBytes = 0;
        switch (valueType) {
            case 0: // uint8
            case 1: // int8
            case 7: // bool
                skipBytes = 1;
                break;
            case 2: // uint16
            case 3: // int16
                skipBytes = 2;
                break;
            case 4: // uint32
            case 5: // int32
            case 6: // float32
                skipBytes = 4;
                break;
            case 10: // uint64
            case 11: // int64
            case 12: // float64
                skipBytes = 8;
                break;
            case 8: { // string: uint64_t length + data
                uint64_t sLen = 0;
                if (read((int)(intptr_t)fileHandle, &sLen, 8) < 8) return 0;
                skipBytes = sLen;
                break;
            }
            case 9: { // array: uint32_t elemType + uint64_t count + elements
                uint32_t elemType = 0;
                uint64_t elemCount = 0;
                if (read((int)(intptr_t)fileHandle, &elemType, 4) < 4) return 0;
                if (read((int)(intptr_t)fileHandle, &elemCount, 8) < 8) return 0;
                if (elemType == 8) { // string array — iterate
                    for (uint64_t e = 0; e < elemCount; e++) {
                        uint64_t sl = 0;
                        if (read((int)(intptr_t)fileHandle, &sl, 8) < 8) return 0;
                        lseek((int)(intptr_t)fileHandle, (off_t)sl, SEEK_CUR);
                    }
                    continue;
                }
                uint64_t elemSize = 4;
                switch (elemType) {
                    case 0: case 1: case 7: elemSize = 1; break;
                    case 2: case 3: elemSize = 2; break;
                    case 4: case 5: case 6: elemSize = 4; break;
                    case 10: case 11: case 12: elemSize = 8; break;
                }
                skipBytes = elemCount * elemSize;
                break;
            }
            default: skipBytes = 8; break;
        }
        lseek((int)(intptr_t)fileHandle, (off_t)skipBytes, SEEK_CUR);
#endif
    }
    return 1;
}

// ── AD_ParseTensorMetadata ───────────────────────────────────────────────────
int AD_ParseTensorMetadata(AD_TensorInfo* tensorTable, AD_AnalysisResult* analysis,
                           intptr_t fileHandle) {
    if (!tensorTable || !analysis || fileHandle == -1) return 0;
    memset(analysis, 0, sizeof(AD_AnalysisResult));
    
    // Read tensor entries from current file position
    // GGUF tensor entry format: name_len(8) + name(N) + ndims(4) + dims(ndims*8) + type(4) + offset(8)
    uint64_t tensorIdx = 0;
    uint64_t maxTensors = g_ad_expected_tensor_count ? g_ad_expected_tensor_count : 32768;
    if (maxTensors > 32768) maxTensors = 32768;
    constexpr uint64_t kMaxNameLen = 4096;

    auto cleanup_current = [&](void) {
        if (tensorTable[tensorIdx].name_ptr) {
            free((void*)(uintptr_t)tensorTable[tensorIdx].name_ptr);
            tensorTable[tensorIdx].name_ptr = 0;
        }
        tensorTable[tensorIdx].name_len = 0;
    };
    
    for (uint64_t t = 0; t < maxTensors; t++) {
        // Read tensor name length
        uint64_t nameLen = 0;
#ifdef _WIN32
        DWORD rd = 0;
        if (!ReadFile((HANDLE)fileHandle, &nameLen, 8, &rd, NULL) || rd < 8) break;
#else
        if (read((int)fileHandle, &nameLen, 8) < 8) break;
#endif
        if (nameLen == 0 || nameLen > kMaxNameLen) break; // sentinel or invalid
        
        // Read tensor name
        char* nameBuf = (char*)malloc((size_t)nameLen + 1);
        if (!nameBuf) break;
        tensorTable[tensorIdx].name_len = nameLen;
        tensorTable[tensorIdx].name_ptr = (uint64_t)(uintptr_t)nameBuf;
#ifdef _WIN32
        if (!ReadFile((HANDLE)fileHandle, nameBuf, (DWORD)nameLen, &rd, NULL) || rd < nameLen) {
            cleanup_current();
            break;
        }
#else
        if (read((int)fileHandle, nameBuf, (size_t)nameLen) < (ssize_t)nameLen) {
            cleanup_current();
            break;
        }
#endif
        nameBuf[nameLen] = '\0';
        
        // Read number of dimensions
        uint32_t ndimsRaw = 0;
#ifdef _WIN32
        if (!ReadFile((HANDLE)fileHandle, &ndimsRaw, 4, &rd, NULL) || rd < 4) {
            cleanup_current();
            break;
        }
#else
        if (read((int)fileHandle, &ndimsRaw, 4) < 4) {
            cleanup_current();
            break;
        }
#endif
        if (ndimsRaw > 128) { // sanity
            cleanup_current();
            break;
        }
        
        // Read dimensions
        uint64_t dims[4] = {0};
        bool okDims = true;
        for (uint32_t d = 0; d < ndimsRaw; d++) {
            uint64_t dimVal = 0;
#ifdef _WIN32
            if (!ReadFile((HANDLE)fileHandle, &dimVal, 8, &rd, NULL) || rd < 8) { okDims = false; break; }
#else
            if (read((int)fileHandle, &dimVal, 8) < 8) { okDims = false; break; }
#endif
            if (d < 4) dims[d] = dimVal;
        }
        if (!okDims) {
            cleanup_current();
            break;
        }
        
        // Read tensor type (GGML_TYPE)
        uint32_t tensorType = 0;
#ifdef _WIN32
        if (!ReadFile((HANDLE)fileHandle, &tensorType, 4, &rd, NULL) || rd < 4) {
            cleanup_current();
            break;
        }
#else
        if (read((int)fileHandle, &tensorType, 4) < 4) {
            cleanup_current();
            break;
        }
#endif
        
        // Read data offset
        uint64_t fileOffset = 0;
#ifdef _WIN32
        if (!ReadFile((HANDLE)fileHandle, &fileOffset, 8, &rd, NULL) || rd < 8) {
            cleanup_current();
            break;
        }
#else
        if (read((int)fileHandle, &fileOffset, 8) < 8) {
            cleanup_current();
            break;
        }
#endif
        
        // Populate tensor info
        tensorTable[tensorIdx].shape_rank = (uint64_t)ndimsRaw;
        for (uint32_t d = 0; d < 4; d++) {
            tensorTable[tensorIdx].shape[d] = dims[d];
        }
        tensorTable[tensorIdx].dtype = tensorType;
        tensorTable[tensorIdx].file_offset = fileOffset;
        
        // Compute parameter count
        AD_CountParameters(&tensorTable[tensorIdx]);
        
        // Identify pattern (FFN/attention/embed/norm)
        AD_IdentifyPattern(&tensorTable[tensorIdx], analysis);
        
        tensorIdx++;
    }
    g_ad_parsed_tensor_count = tensorIdx;
    return (tensorIdx > 0) ? 1 : 0;
}

// ── AD_IdentifyPattern ───────────────────────────────────────────────────────
void AD_IdentifyPattern(AD_TensorInfo* tensorInfo, AD_AnalysisResult* analysis) {
    if (!tensorInfo || !analysis) return;
    const char* name = (const char*)tensorInfo->name_ptr;
    if (!name) {
        tensorInfo->pattern_type = AD_PATTERN_UNKNOWN;
        analysis->unknown_layers++;
        return;
    }
    // Pattern match on tensor name
    if (strstr(name, "ffn") || strstr(name, "feed_forward") || strstr(name, "mlp")) {
        tensorInfo->pattern_type = AD_PATTERN_FFN;
        analysis->ffn_blocks++;
    } else if (strstr(name, "attn") || strstr(name, "attention") || strstr(name, "self_attn")) {
        tensorInfo->pattern_type = AD_PATTERN_ATTENTION;
        analysis->attn_heads++;
    } else if (strstr(name, "embed") || strstr(name, "token_embd") || strstr(name, "wte")) {
        tensorInfo->pattern_type = AD_PATTERN_EMBED;
        analysis->embed_tokens++;
    } else if (strstr(name, "norm") || strstr(name, "ln_") || strstr(name, "layer_norm")) {
        tensorInfo->pattern_type = AD_PATTERN_NORM;
        analysis->norm_layers++;
    } else {
        tensorInfo->pattern_type = AD_PATTERN_UNKNOWN;
        analysis->unknown_layers++;
    }
}

// ── AD_AnalyzeStructure ──────────────────────────────────────────────────────
int AD_AnalyzeStructure(AD_TensorInfo* tensorTable, AD_AnalysisResult* analysis) {
    if (!tensorTable || !analysis) return 0;
    
    uint64_t count = g_ad_parsed_tensor_count ? g_ad_parsed_tensor_count : g_ad_expected_tensor_count;
    if (count > 32768) count = 32768;

    // Compute layer_count from maximum layer index seen, and total parameters across tensors.
    int32_t maxLayer = -1;
    uint64_t totalParams = 0;
    for (uint64_t i = 0; i < count; i++) {
        totalParams += tensorTable[i].param_count;
        const char* name = (const char*)(uintptr_t)tensorTable[i].name_ptr;
        if (name) {
            int32_t li = AD_ExtractLayerIndex(name);
            if (li > maxLayer) maxLayer = li;
        }
    }
    analysis->total_params = totalParams;
    analysis->layer_count = (maxLayer >= 0) ? (uint32_t)(maxLayer + 1) : 0;
    return 1;
}

// ── AD_DistillToExec ─────────────────────────────────────────────────────────
int AD_DistillToExec(const char* outputPath, AD_AnalysisResult* analysis,
                     intptr_t fileHandle) {
    if (!outputPath || !analysis) return 0;
    return AD_WriteExecFile(outputPath, analysis);
}

// ── AD_WriteExecFile ─────────────────────────────────────────────────────────
int AD_WriteExecFile(const char* outputPath, AD_AnalysisResult* analysis) {
    if (!outputPath || !analysis) return 0;
#ifdef _WIN32
    HANDLE h = CreateFileA(outputPath, GENERIC_WRITE, 0,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    // Write exec header magic
    const char magic[] = "EXEC";
    DWORD written = 0;
    WriteFile(h, magic, 4, &written, NULL);
    // Write analysis summary
    WriteFile(h, analysis, sizeof(AD_AnalysisResult), &written, NULL);
    CloseHandle(h);
    return 1;
#else
    FILE* f = fopen(outputPath, "wb");
    if (!f) return 0;
    fwrite("EXEC", 1, 4, f);
    fwrite(analysis, 1, sizeof(AD_AnalysisResult), f);
    fclose(f);
    return 1;
#endif
}

// ── AD_CountParameters ───────────────────────────────────────────────────────
uint64_t AD_CountParameters(AD_TensorInfo* tensorInfo) {
    if (!tensorInfo || tensorInfo->shape_rank == 0) return 0;
    uint64_t total = 1;
    for (uint64_t i = 0; i < tensorInfo->shape_rank && i < 4; i++) {
        if (tensorInfo->shape[i] > 0) total *= tensorInfo->shape[i];
    }
    tensorInfo->param_count = total;
    return total;
}

// ── AD_ExtractLayerIndex ─────────────────────────────────────────────────────
int32_t AD_ExtractLayerIndex(const char* name) {
    if (!name) return -1;
    // Scan for first numeric run in the string
    const char* p = name;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            int32_t val = 0;
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                p++;
            }
            return val;
        }
        p++;
    }
    return -1;
}

// ── AD_ProcessGGUF ───────────────────────────────────────────────────────────
int AD_ProcessGGUF(const char* inputPath, const char* outputExecPath) {
    if (!inputPath || !outputExecPath) return 0;
    
    intptr_t fh = AD_OpenGGUFFile(inputPath);
    if (fh == -1) return 0;
    
    AD_GGUFHeader header;
    memset(&header, 0, sizeof(header));
    if (!AD_ValidateGGUFHeader(&header, fh)) {
#ifdef _WIN32
        CloseHandle((HANDLE)fh);
#else
        close((int)fh);
#endif
        return 0;
    }
    
    if (!AD_SkipMetadataKV(header.metadata_kv, fh)) {
#ifdef _WIN32
        CloseHandle((HANDLE)fh);
#else
        close((int)fh);
#endif
        return 0;
    }
    
    // Allocate tensor table
    size_t tableSize = (size_t)(header.tensor_count < 32768 ? header.tensor_count : 32768);
    AD_TensorInfo* table = (AD_TensorInfo*)calloc(tableSize, sizeof(AD_TensorInfo));
    if (!table) {
#ifdef _WIN32
        CloseHandle((HANDLE)fh);
#else
        close((int)fh);
#endif
        return 0;
    }
    
    AD_AnalysisResult analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    int result = AD_ParseTensorMetadata(table, &analysis, fh);
    if (result) {
        result = AD_AnalyzeStructure(table, &analysis);
    }
    if (result) {
        result = AD_WriteExecFile(outputExecPath, &analysis);
    }

    // Free any per-tensor allocations produced by the C++ fallback parser.
    for (size_t i = 0; i < (size_t)g_ad_parsed_tensor_count && i < tableSize; i++) {
        if (table[i].name_ptr) {
            free((void*)(uintptr_t)table[i].name_ptr);
            table[i].name_ptr = 0;
        }
        table[i].name_len = 0;
    }

    free(table);
#ifdef _WIN32
    CloseHandle((HANDLE)fh);
#else
    close((int)fh);
#endif
    return result;
}

#ifdef __cplusplus
}
#endif

#endif // !RAWRXD_LINK_ANALYZER_DISTILLER_ASM
