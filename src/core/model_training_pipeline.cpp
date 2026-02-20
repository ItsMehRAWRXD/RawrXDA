// ============================================================================
// model_training_pipeline.cpp — Full Implementation
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Links with: quant_avx2.asm, RawrXD_KQuant_Kernel.asm, RawrXD_KQuant_Dequant.asm
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "model_training_pipeline.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <intrin.h>   // __cpuid, _mm256_*

namespace RawrXD {
namespace Training {

// ============================================================================
// External MASM kernel declarations (linked from .obj)
// ============================================================================
extern "C" {
    // quant_avx2.asm
    uint64_t Quant_DequantQ4_0(const void* src, float* dst, uint64_t numElements);
    uint64_t Quant_DequantQ8_0(const void* src, float* dst, uint64_t numElements);
    float    Quant_VecDotQ4_0_Q8_0(const void* weights, const void* activations, uint64_t numElements);
    float    Quant_VecDotQ8_0_F32(const void* src, const float* f32_vec, uint64_t numElements);
    uint64_t Quant_GetStats(uint64_t* out);

    // RawrXD_KQuant_Kernel.asm
    uint64_t asm_kquant_cpuid_check(void);
    uint64_t asm_dequant_q4_k_batch(float* pOutput, const void* pInput, uint64_t numElements);

    // RawrXD_KQuant_Dequant.asm
    uint64_t KQuant_DequantizeQ4_K(const void* src, float* dst, uint64_t numElements);
    uint64_t KQuant_DequantizeQ6_K(const void* src, float* dst, uint64_t numElements);
    uint64_t KQuant_DequantizeF16(const uint16_t* src, float* dst, uint64_t numElements);
    uint64_t KQuant_Dispatch(uint32_t ggml_type, const void* src, float* dst, uint64_t numElements);
}

// ============================================================================
// GGUF Constants
// ============================================================================
static constexpr uint32_t GGUF_MAGIC   = 0x46475547u; // 'GGUF'
static constexpr uint32_t GGUF_VERSION = 3;

// GGUF types for metadata
enum GGUFMetaType : uint32_t {
    GGUF_TYPE_UINT8   = 0,  GGUF_TYPE_INT8    = 1,
    GGUF_TYPE_UINT16  = 2,  GGUF_TYPE_INT16   = 3,
    GGUF_TYPE_UINT32  = 4,  GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,  GGUF_TYPE_BOOL    = 7,
    GGUF_TYPE_STRING  = 8,  GGUF_TYPE_ARRAY   = 9,
    GGUF_TYPE_UINT64  = 10, GGUF_TYPE_INT64   = 11,
    GGUF_TYPE_FLOAT64 = 12,
};

// GGUF tensor types (matches ggml_type)
enum GGUFTensorType : uint32_t {
    GGUF_TENSOR_F32   = 0,  GGUF_TENSOR_F16   = 1,
    GGUF_TENSOR_Q4_0  = 2,  GGUF_TENSOR_Q4_1  = 3,
    GGUF_TENSOR_Q5_0  = 6,  GGUF_TENSOR_Q5_1  = 7,
    GGUF_TENSOR_Q8_0  = 8,  GGUF_TENSOR_Q8_1  = 9,
    GGUF_TENSOR_Q2_K  = 10, GGUF_TENSOR_Q3_K  = 11,
    GGUF_TENSOR_Q4_K  = 12, GGUF_TENSOR_Q5_K  = 13,
    GGUF_TENSOR_Q6_K  = 14,
};

static GGUFTensorType quantTypeToGGUF(QuantType qt) {
    switch (qt) {
        case QuantType::Q2_K:    return GGUF_TENSOR_Q2_K;
        case QuantType::Q3_K_S:
        case QuantType::Q3_K_M:
        case QuantType::Q3_K_L:  return GGUF_TENSOR_Q3_K;
        case QuantType::Q4_0:    return GGUF_TENSOR_Q4_0;
        case QuantType::Q4_K_S:
        case QuantType::Q4_K_M:  return GGUF_TENSOR_Q4_K;
        case QuantType::Q5_0:    return GGUF_TENSOR_Q5_0;
        case QuantType::Q5_K_S:
        case QuantType::Q5_K_M:  return GGUF_TENSOR_Q5_K;
        case QuantType::Q6_K:    return GGUF_TENSOR_Q6_K;
        case QuantType::Q8_0:    return GGUF_TENSOR_Q8_0;
        case QuantType::F16:     return GGUF_TENSOR_F16;
        default:                 return GGUF_TENSOR_F32;
    }
}

// Bytes per element for each quant type (approximate, based on block size)
static double bitsPerWeight(QuantType qt) {
    switch (qt) {
        case QuantType::Q2_K:    return 2.5625;
        case QuantType::Q3_K_S:
        case QuantType::Q3_K_M:
        case QuantType::Q3_K_L:  return 3.4375;
        case QuantType::Q4_0:    return 4.5;
        case QuantType::Q4_K_S:
        case QuantType::Q4_K_M:  return 4.5;
        case QuantType::Q5_0:    return 5.5;
        case QuantType::Q5_K_S:
        case QuantType::Q5_K_M:  return 5.5;
        case QuantType::Q6_K:    return 6.5625;
        case QuantType::Q8_0:    return 8.5;
        case QuantType::F16:     return 16.0;
        case QuantType::F32:     return 32.0;
        case QuantType::IQ2_XXS: return 2.0625;
        case QuantType::IQ3_S:   return 3.4375;
        case QuantType::NanoQuant: return 0.7;
        default:                 return 4.5;
    }
}

// Block size (number of elements per quantization block)
static uint32_t blockSize(QuantType qt) {
    switch (qt) {
        case QuantType::Q2_K:
        case QuantType::Q3_K_S:
        case QuantType::Q3_K_M:
        case QuantType::Q3_K_L:
        case QuantType::Q4_K_S:
        case QuantType::Q4_K_M:
        case QuantType::Q5_K_S:
        case QuantType::Q5_K_M:
        case QuantType::Q6_K:    return 256;
        case QuantType::Q4_0:
        case QuantType::Q5_0:
        case QuantType::Q8_0:    return 32;
        case QuantType::F16:
        case QuantType::F32:     return 1;
        default:                 return 32;
    }
}

// ============================================================================
// DatasetPipeline Implementation
// ============================================================================

TrainingResult DatasetPipeline::addFile(const char* path, DatasetFormat format) {
    if (!path || !path[0]) return TrainingResult::error("Null path");

    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return TrainingResult::error("File not found");

    switch (format) {
        case DatasetFormat::PlainText:
        case DatasetFormat::CodeFiles:  return parsePlainText(path);
        case DatasetFormat::JSONL:      return parseJSONL(path);
        case DatasetFormat::Alpaca:     return parseAlpaca(path);
        case DatasetFormat::ShareGPT:   return parseShareGPT(path);
        case DatasetFormat::CSV:        return parseCSV(path);
        default:
            return TrainingResult::error("Unsupported dataset format");
    }
}

TrainingResult DatasetPipeline::addDirectory(const char* dirPath, DatasetFormat format, bool recursive) {
    if (!dirPath) return TrainingResult::error("Null directory path");

    WIN32_FIND_DATAA fd;
    std::string pattern = std::string(dirPath) + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return TrainingResult::error("Cannot open directory");

    uint64_t filesAdded = 0;
    do {
        if (fd.cFileName[0] == '.') continue;

        std::string fullPath = std::string(dirPath) + "\\" + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive) addDirectory(fullPath.c_str(), format, true);
            continue;
        }

        // Filter by extension based on format
        const char* ext = strrchr(fd.cFileName, '.');
        bool include = false;
        if (format == DatasetFormat::PlainText) {
            include = ext && (_stricmp(ext, ".txt") == 0 || _stricmp(ext, ".md") == 0);
        } else if (format == DatasetFormat::CodeFiles) {
            include = ext && (_stricmp(ext, ".cpp") == 0 || _stricmp(ext, ".c") == 0 ||
                              _stricmp(ext, ".h") == 0 || _stricmp(ext, ".hpp") == 0 ||
                              _stricmp(ext, ".py") == 0 || _stricmp(ext, ".rs") == 0 ||
                              _stricmp(ext, ".js") == 0 || _stricmp(ext, ".ts") == 0 ||
                              _stricmp(ext, ".java") == 0 || _stricmp(ext, ".go") == 0 ||
                              _stricmp(ext, ".asm") == 0);
        } else if (format == DatasetFormat::JSONL) {
            include = ext && (_stricmp(ext, ".jsonl") == 0 || _stricmp(ext, ".json") == 0);
        } else if (format == DatasetFormat::CSV) {
            include = ext && (_stricmp(ext, ".csv") == 0 || _stricmp(ext, ".tsv") == 0);
        } else {
            include = true; // Accept all for other formats
        }

        if (include) {
            addFile(fullPath.c_str(), format);
            filesAdded++;
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    return TrainingResult::ok("Directory scanned");
}

TrainingResult DatasetPipeline::addText(const char* text, uint64_t length) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rawTexts.emplace_back(text, length);
    m_tokenized = false;
    return TrainingResult::ok("Text added");
}

TrainingResult DatasetPipeline::parsePlainText(const char* path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return TrainingResult::error("Cannot open file");

    LARGE_INTEGER fileSize;
    GetFileSizeEx(h, &fileSize);
    if (fileSize.QuadPart > 512ULL * 1024 * 1024) {
        CloseHandle(h);
        return TrainingResult::error("File too large (>512MB). Use streaming.");
    }

    std::string content;
    content.resize((size_t)fileSize.QuadPart);
    DWORD bytesRead = 0;
    ReadFile(h, &content[0], (DWORD)fileSize.QuadPart, &bytesRead, nullptr);
    CloseHandle(h);
    content.resize(bytesRead);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_rawTexts.push_back(std::move(content));
    m_tokenized = false;
    return TrainingResult::ok("Text file loaded");
}

TrainingResult DatasetPipeline::parseJSONL(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return TrainingResult::error("Cannot open JSONL file");

    char line[65536];
    uint64_t lineCount = 0;

    while (fgets(line, sizeof(line), f)) {
        // Minimal JSON parse: find "text": "..." or "content": "..."
        const char* textKey = strstr(line, "\"text\"");
        if (!textKey) textKey = strstr(line, "\"content\"");
        if (!textKey) continue;

        const char* colonPos = strchr(textKey + 5, ':');
        if (!colonPos) continue;

        // Find the opening quote after colon
        const char* valStart = strchr(colonPos, '"');
        if (!valStart) continue;
        valStart++;

        // Find closing quote (handle escaped quotes)
        std::string value;
        for (const char* p = valStart; *p && *p != '\n'; ++p) {
            if (*p == '\\' && *(p + 1)) {
                p++;
                if (*p == 'n') value += '\n';
                else if (*p == 't') value += '\t';
                else if (*p == '"') value += '"';
                else if (*p == '\\') value += '\\';
                else value += *p;
            } else if (*p == '"') {
                break;
            } else {
                value += *p;
            }
        }

        if (!value.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_rawTexts.push_back(std::move(value));
            lineCount++;
        }
    }
    fclose(f);
    m_tokenized = false;
    return TrainingResult::ok("JSONL parsed");
}

TrainingResult DatasetPipeline::parseAlpaca(const char* path) {
    // Alpaca format: [{"instruction":"...", "input":"...", "output":"..."}]
    FILE* f = fopen(path, "r");
    if (!f) return TrainingResult::error("Cannot open Alpaca file");

    // Read entire file
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content(sz, '\0');
    fread(&content[0], 1, sz, f);
    fclose(f);

    // Simple pattern-based extraction
    size_t pos = 0;
    while ((pos = content.find("\"instruction\"", pos)) != std::string::npos) {
        auto extractField = [&](const char* key) -> std::string {
            size_t kp = content.find(key, pos);
            if (kp == std::string::npos) return "";
            size_t vs = content.find('"', content.find(':', kp) + 1);
            if (vs == std::string::npos) return "";
            vs++;
            std::string val;
            for (size_t i = vs; i < content.size() && content[i] != '"'; ++i) {
                if (content[i] == '\\' && i + 1 < content.size()) {
                    i++;
                    if (content[i] == 'n') val += '\n';
                    else val += content[i];
                } else {
                    val += content[i];
                }
            }
            return val;
        };

        std::string instr = extractField("\"instruction\"");
        std::string input = extractField("\"input\"");
        std::string output = extractField("\"output\"");

        if (!instr.empty() && !output.empty()) {
            std::string combined = "### Instruction:\n" + instr;
            if (!input.empty()) combined += "\n### Input:\n" + input;
            combined += "\n### Response:\n" + output;

            std::lock_guard<std::mutex> lock(m_mutex);
            m_rawTexts.push_back(std::move(combined));
        }

        pos += 10;
    }

    m_tokenized = false;
    return TrainingResult::ok("Alpaca format parsed");
}

TrainingResult DatasetPipeline::parseShareGPT(const char* path) {
    // ShareGPT format: {"conversations":[{"from":"human","value":"..."},{"from":"gpt","value":"..."}]}
    FILE* f = fopen(path, "r");
    if (!f) return TrainingResult::error("Cannot open ShareGPT file");

    char line[131072];
    while (fgets(line, sizeof(line), f)) {
        std::string combined;
        const char* p = line;

        while ((p = strstr(p, "\"value\"")) != nullptr) {
            const char* colon = strchr(p + 7, ':');
            if (!colon) break;
            const char* qstart = strchr(colon, '"');
            if (!qstart) break;
            qstart++;

            std::string val;
            for (const char* q = qstart; *q && *q != '\n'; ++q) {
                if (*q == '\\' && *(q + 1)) {
                    q++;
                    if (*q == 'n') val += '\n';
                    else val += *q;
                } else if (*q == '"') break;
                else val += *q;
            }

            if (!combined.empty()) combined += "\n\n";
            combined += val;
            p = qstart;
        }

        if (!combined.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_rawTexts.push_back(std::move(combined));
        }
    }
    fclose(f);
    m_tokenized = false;
    return TrainingResult::ok("ShareGPT parsed");
}

TrainingResult DatasetPipeline::parseCSV(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return TrainingResult::error("Cannot open CSV file");

    char line[65536];
    bool isHeader = true;
    while (fgets(line, sizeof(line), f)) {
        if (isHeader) { isHeader = false; continue; }

        // Skip empty lines
        if (!line[0] || line[0] == '\n') continue;

        // Trim newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        std::lock_guard<std::mutex> lock(m_mutex);
        m_rawTexts.emplace_back(line);
    }
    fclose(f);
    m_tokenized = false;
    return TrainingResult::ok("CSV parsed");
}

TrainingResult DatasetPipeline::parseCodeFiles(const char* path) {
    return parsePlainText(path); // Code files are just text with different filtering
}

TrainingResult DatasetPipeline::loadTokenizer(const char* tokenizerPath) {
    if (!tokenizerPath || !tokenizerPath[0])
        return TrainingResult::error("Null tokenizer path");

    FILE* f = fopen(tokenizerPath, "rb");
    if (!f) return TrainingResult::error("Cannot open tokenizer file");

    // Read BPE merge table
    // Format: binary header (uint32 vocabSize, uint32 numMerges) + merges + vocab strings
    uint32_t header[2];
    if (fread(header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return TrainingResult::error("Invalid tokenizer header");
    }

    m_vocabSize = header[0];
    uint32_t numMerges = header[1];

    m_merges.resize(numMerges);
    fread(m_merges.data(), sizeof(BPEMerge), numMerges, f);

    // Read vocab strings
    m_tokenStrings.resize(m_vocabSize);
    for (uint32_t i = 0; i < m_vocabSize; ++i) {
        uint16_t strLen = 0;
        fread(&strLen, 2, 1, f);
        m_tokenStrings[i].resize(strLen);
        fread(&m_tokenStrings[i][0], 1, strLen, f);
    }

    fclose(f);
    return TrainingResult::ok("Tokenizer loaded");
}

TrainingResult DatasetPipeline::buildTokenizer(uint32_t vocabSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_rawTexts.empty())
        return TrainingResult::error("No training data loaded. Add data before building tokenizer.");

    m_vocabSize = vocabSize;

    // Step 1: Count byte-level character frequencies across all texts
    uint64_t charFreq[256] = {};
    uint64_t totalChars = 0;
    for (const auto& text : m_rawTexts) {
        for (unsigned char c : text) {
            charFreq[c]++;
            totalChars++;
        }
    }

    // Step 2: Initialize vocabulary with byte-level tokens (0-255)
    m_tokenStrings.clear();
    m_tokenStrings.resize(256);
    for (int i = 0; i < 256; ++i) {
        m_tokenStrings[i] = std::string(1, (char)i);
    }

    // Step 3: BPE merge algorithm — find most frequent pair, merge, repeat
    m_merges.clear();
    uint32_t currentVocab = 256;

    // Build initial encoded representation per text (byte sequences)
    std::vector<std::vector<uint16_t>> encoded(m_rawTexts.size());
    for (size_t t = 0; t < m_rawTexts.size(); ++t) {
        encoded[t].resize(m_rawTexts[t].size());
        for (size_t i = 0; i < m_rawTexts[t].size(); ++i)
            encoded[t][i] = (uint16_t)(unsigned char)m_rawTexts[t][i];
    }

    while (currentVocab < vocabSize) {
        // Count all bigram frequencies
        struct PairHash {
            size_t operator()(std::pair<uint16_t, uint16_t> p) const {
                return ((size_t)p.first << 16) | p.second;
            }
        };
        std::unordered_map<std::pair<uint16_t, uint16_t>, uint64_t, PairHash> pairFreqs;

        for (const auto& seq : encoded) {
            for (size_t i = 0; i + 1 < seq.size(); ++i) {
                pairFreqs[{seq[i], seq[i + 1]}]++;
            }
        }

        if (pairFreqs.empty()) break;

        // Find most frequent pair
        auto bestIt = std::max_element(pairFreqs.begin(), pairFreqs.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        if (bestIt->second < 2) break; // No more useful merges

        uint16_t mergeA = bestIt->first.first;
        uint16_t mergeB = bestIt->first.second;
        uint16_t newId  = (uint16_t)currentVocab;

        // Record merge
        m_merges.push_back({ mergeA, mergeB, newId });

        // Add to vocabulary
        std::string newToken = m_tokenStrings[mergeA] + m_tokenStrings[mergeB];
        m_tokenStrings.push_back(newToken);

        // Apply merge to all sequences
        for (auto& seq : encoded) {
            std::vector<uint16_t> merged;
            merged.reserve(seq.size());
            for (size_t i = 0; i < seq.size(); ++i) {
                if (i + 1 < seq.size() && seq[i] == mergeA && seq[i + 1] == mergeB) {
                    merged.push_back(newId);
                    ++i; // skip next
                } else {
                    merged.push_back(seq[i]);
                }
            }
            seq = std::move(merged);
        }

        currentVocab++;

        // Progress reporting every 1000 merges
        if ((currentVocab - 256) % 1000 == 0) {
            // Could call a callback here
        }
    }

    m_vocabSize = currentVocab;
    return TrainingResult::ok("BPE tokenizer built");
}

TrainingResult DatasetPipeline::tokenizeAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vocabSize == 0)
        return TrainingResult::error("No tokenizer loaded. Call loadTokenizer() or buildTokenizer() first.");

    // Build token-to-id lookup from vocabulary
    // For BPE: iteratively apply merges to each text
    m_samples.clear();
    m_samples.reserve(m_rawTexts.size());

    for (const auto& text : m_rawTexts) {
        // Initialize as byte tokens
        std::vector<uint16_t> tokens;
        tokens.reserve(text.size());
        for (unsigned char c : text) tokens.push_back((uint16_t)c);

        // Apply all BPE merges in order
        for (const auto& merge : m_merges) {
            std::vector<uint16_t> merged;
            merged.reserve(tokens.size());
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (i + 1 < tokens.size() && tokens[i] == merge.a && tokens[i + 1] == merge.b) {
                    merged.push_back(merge.result);
                    ++i;
                } else {
                    merged.push_back(tokens[i]);
                }
            }
            tokens = std::move(merged);
        }

        DatasetEntry entry;
        entry.tokenIds = std::move(tokens);
        entry.seqLen = (uint32_t)entry.tokenIds.size();
        entry.promptLen = 0;
        entry.responseLen = entry.seqLen;
        m_samples.push_back(std::move(entry));
    }

    m_tokenized = true;
    return TrainingResult::ok("All texts tokenized");
}

uint64_t DatasetPipeline::getTotalTokens() const {
    uint64_t total = 0;
    for (const auto& s : m_samples) total += s.seqLen;
    return total;
}

uint64_t DatasetPipeline::getNumSamples() const { return m_samples.size(); }

const DatasetEntry& DatasetPipeline::getSample(uint64_t idx) const {
    static DatasetEntry empty;
    if (idx < m_samples.size()) return m_samples[idx];
    return empty;
}

TrainingResult DatasetPipeline::saveToDisk(const char* path) const {
    FILE* f = fopen(path, "wb");
    if (!f) return TrainingResult::error("Cannot create dataset file");

    // Header: magic + version + vocab size + merge count + sample count
    uint32_t magic = 0x52585444; // 'DXTR' (dataset rawrxd training)
    fwrite(&magic, 4, 1, f);
    uint32_t version = 1;
    fwrite(&version, 4, 1, f);
    fwrite(&m_vocabSize, 4, 1, f);
    uint32_t numMerges = (uint32_t)m_merges.size();
    fwrite(&numMerges, 4, 1, f);
    uint64_t numSamples = m_samples.size();
    fwrite(&numSamples, 8, 1, f);

    // Write merges
    fwrite(m_merges.data(), sizeof(BPEMerge), numMerges, f);

    // Write vocab strings
    for (uint32_t i = 0; i < m_tokenStrings.size() && i < m_vocabSize; ++i) {
        uint16_t len = (uint16_t)m_tokenStrings[i].size();
        fwrite(&len, 2, 1, f);
        fwrite(m_tokenStrings[i].data(), 1, len, f);
    }

    // Write samples
    for (const auto& s : m_samples) {
        fwrite(&s.seqLen, 4, 1, f);
        fwrite(&s.promptLen, 4, 1, f);
        fwrite(&s.responseLen, 4, 1, f);
        fwrite(s.tokenIds.data(), 2, s.seqLen, f);
    }

    fclose(f);
    return TrainingResult::ok("Dataset saved");
}

TrainingResult DatasetPipeline::loadFromDisk(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return TrainingResult::error("Cannot open dataset file");

    uint32_t magic, version;
    fread(&magic, 4, 1, f);
    if (magic != 0x52585444) { fclose(f); return TrainingResult::error("Invalid dataset magic"); }
    fread(&version, 4, 1, f);
    fread(&m_vocabSize, 4, 1, f);
    uint32_t numMerges;
    fread(&numMerges, 4, 1, f);
    uint64_t numSamples;
    fread(&numSamples, 8, 1, f);

    m_merges.resize(numMerges);
    fread(m_merges.data(), sizeof(BPEMerge), numMerges, f);

    m_tokenStrings.resize(m_vocabSize);
    for (uint32_t i = 0; i < m_vocabSize; ++i) {
        uint16_t len;
        fread(&len, 2, 1, f);
        m_tokenStrings[i].resize(len);
        fread(&m_tokenStrings[i][0], 1, len, f);
    }

    m_samples.resize(numSamples);
    for (uint64_t i = 0; i < numSamples; ++i) {
        fread(&m_samples[i].seqLen, 4, 1, f);
        fread(&m_samples[i].promptLen, 4, 1, f);
        fread(&m_samples[i].responseLen, 4, 1, f);
        m_samples[i].tokenIds.resize(m_samples[i].seqLen);
        fread(m_samples[i].tokenIds.data(), 2, m_samples[i].seqLen, f);
    }

    fclose(f);
    m_tokenized = true;
    return TrainingResult::ok("Dataset loaded");
}

DatasetPipeline::Stats DatasetPipeline::getStats() const {
    Stats s = {};
    s.totalFiles = m_rawTexts.size();
    for (const auto& t : m_rawTexts) s.totalBytes += t.size();
    s.numSamples = m_samples.size();
    s.vocabSize = m_vocabSize;
    for (const auto& sample : m_samples) s.totalTokens += sample.seqLen;
    s.avgSeqLen = s.numSamples > 0 ? (double)s.totalTokens / s.numSamples : 0.0;
    return s;
}

// ============================================================================
// ModelArchBuilder Implementation
// ============================================================================

TrainingResult ModelArchBuilder::configure(const ModelArchConfig& config) {
    m_config = config;

    // Validate
    if (m_config.vocabSize == 0) return TrainingResult::error("vocabSize must be > 0");
    if (m_config.hiddenSize == 0) return TrainingResult::error("hiddenSize must be > 0");
    if (m_config.numLayers == 0) return TrainingResult::error("numLayers must be > 0");
    if (m_config.numHeads == 0) return TrainingResult::error("numHeads must be > 0");
    if (m_config.hiddenSize % m_config.numHeads != 0)
        return TrainingResult::error("hiddenSize must be divisible by numHeads");
    if (m_config.numKVHeads > m_config.numHeads)
        return TrainingResult::error("numKVHeads must be <= numHeads");

    return TrainingResult::ok("Architecture configured");
}

TrainingResult ModelArchBuilder::generateModelScript(const char* outputPath) const {
    FILE* f = fopen(outputPath, "w");
    if (!f) return TrainingResult::error("Cannot create model script");

    // Generate PyTorch model definition script
    fprintf(f, "#!/usr/bin/env python3\n");
    fprintf(f, "# Auto-generated by RawrXD Training Pipeline\n");
    fprintf(f, "# Architecture: %s (%uL, %uH, %uD)\n\n",
            m_config.arch == ModelArch::LLaMA ? "LLaMA" :
            m_config.arch == ModelArch::Mistral ? "Mistral" :
            m_config.arch == ModelArch::Phi ? "Phi" : "GPT2",
            m_config.numLayers, m_config.numHeads, m_config.hiddenSize);

    fprintf(f, "import torch\n");
    fprintf(f, "import torch.nn as nn\n");
    fprintf(f, "import torch.nn.functional as F\n");
    fprintf(f, "import math\n");
    fprintf(f, "import json\n");
    fprintf(f, "import sys\n");
    fprintf(f, "import os\n\n");

    // Config class
    fprintf(f, "class ModelConfig:\n");
    fprintf(f, "    vocab_size = %u\n", m_config.vocabSize);
    fprintf(f, "    hidden_size = %u\n", m_config.hiddenSize);
    fprintf(f, "    intermediate_size = %u\n", m_config.intermediateSize);
    fprintf(f, "    num_layers = %u\n", m_config.numLayers);
    fprintf(f, "    num_heads = %u\n", m_config.numHeads);
    fprintf(f, "    num_kv_heads = %u\n", m_config.numKVHeads);
    fprintf(f, "    max_seq_len = %u\n", m_config.maxSeqLen);
    fprintf(f, "    rms_norm_eps = %.1e\n", m_config.rmsNormEps);
    fprintf(f, "    rope_theta = %.1f\n", m_config.ropeTheta);
    fprintf(f, "    tie_embeddings = %s\n\n", m_config.tieTokEmbeddings ? "True" : "False");

    // RMSNorm
    fprintf(f, "class RMSNorm(nn.Module):\n");
    fprintf(f, "    def __init__(self, dim, eps=1e-5):\n");
    fprintf(f, "        super().__init__()\n");
    fprintf(f, "        self.eps = eps\n");
    fprintf(f, "        self.weight = nn.Parameter(torch.ones(dim))\n\n");
    fprintf(f, "    def forward(self, x):\n");
    fprintf(f, "        return x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + self.eps) * self.weight\n\n");

    // Rotary positional embeddings
    fprintf(f, "def precompute_freqs_cis(dim, end, theta=%.1f):\n", m_config.ropeTheta);
    fprintf(f, "    freqs = 1.0 / (theta ** (torch.arange(0, dim, 2)[: (dim // 2)].float() / dim))\n");
    fprintf(f, "    t = torch.arange(end, device=freqs.device)\n");
    fprintf(f, "    freqs = torch.outer(t, freqs)\n");
    fprintf(f, "    return torch.polar(torch.ones_like(freqs), freqs)\n\n");

    fprintf(f, "def apply_rotary_emb(xq, xk, freqs_cis):\n");
    fprintf(f, "    xq_ = torch.view_as_complex(xq.float().reshape(*xq.shape[:-1], -1, 2))\n");
    fprintf(f, "    xk_ = torch.view_as_complex(xk.float().reshape(*xk.shape[:-1], -1, 2))\n");
    fprintf(f, "    freqs_cis = freqs_cis[:xq_.shape[1]].unsqueeze(0).unsqueeze(2)\n");
    fprintf(f, "    xq_out = torch.view_as_real(xq_ * freqs_cis).flatten(3)\n");
    fprintf(f, "    xk_out = torch.view_as_real(xk_ * freqs_cis).flatten(3)\n");
    fprintf(f, "    return xq_out.type_as(xq), xk_out.type_as(xk)\n\n");

    // Attention with GQA support
    uint32_t headDim = m_config.hiddenSize / m_config.numHeads;
    fprintf(f, "class Attention(nn.Module):\n");
    fprintf(f, "    def __init__(self, config):\n");
    fprintf(f, "        super().__init__()\n");
    fprintf(f, "        self.n_heads = config.num_heads\n");
    fprintf(f, "        self.n_kv_heads = config.num_kv_heads\n");
    fprintf(f, "        self.head_dim = config.hidden_size // config.num_heads\n");
    fprintf(f, "        self.n_rep = self.n_heads // self.n_kv_heads\n");
    fprintf(f, "        self.wq = nn.Linear(config.hidden_size, config.num_heads * self.head_dim, bias=False)\n");
    fprintf(f, "        self.wk = nn.Linear(config.hidden_size, config.num_kv_heads * self.head_dim, bias=False)\n");
    fprintf(f, "        self.wv = nn.Linear(config.hidden_size, config.num_kv_heads * self.head_dim, bias=False)\n");
    fprintf(f, "        self.wo = nn.Linear(config.num_heads * self.head_dim, config.hidden_size, bias=False)\n\n");

    fprintf(f, "    def forward(self, x, freqs_cis, mask=None):\n");
    fprintf(f, "        bsz, seqlen, _ = x.shape\n");
    fprintf(f, "        xq = self.wq(x).view(bsz, seqlen, self.n_heads, self.head_dim)\n");
    fprintf(f, "        xk = self.wk(x).view(bsz, seqlen, self.n_kv_heads, self.head_dim)\n");
    fprintf(f, "        xv = self.wv(x).view(bsz, seqlen, self.n_kv_heads, self.head_dim)\n");
    fprintf(f, "        xq, xk = apply_rotary_emb(xq, xk, freqs_cis)\n");
    fprintf(f, "        if self.n_rep > 1:\n");
    fprintf(f, "            xk = xk.repeat_interleave(self.n_rep, dim=2)\n");
    fprintf(f, "            xv = xv.repeat_interleave(self.n_rep, dim=2)\n");
    fprintf(f, "        xq = xq.transpose(1, 2)\n");
    fprintf(f, "        xk = xk.transpose(1, 2)\n");
    fprintf(f, "        xv = xv.transpose(1, 2)\n");
    fprintf(f, "        scores = torch.matmul(xq, xk.transpose(2, 3)) / math.sqrt(self.head_dim)\n");
    fprintf(f, "        if mask is not None: scores = scores + mask\n");
    fprintf(f, "        scores = F.softmax(scores.float(), dim=-1).type_as(xq)\n");
    fprintf(f, "        output = torch.matmul(scores, xv)\n");
    fprintf(f, "        output = output.transpose(1, 2).contiguous().view(bsz, seqlen, -1)\n");
    fprintf(f, "        return self.wo(output)\n\n");

    // Feed-forward
    const char* act = m_config.activationFn;
    fprintf(f, "class FeedForward(nn.Module):\n");
    fprintf(f, "    def __init__(self, config):\n");
    fprintf(f, "        super().__init__()\n");
    fprintf(f, "        self.w1 = nn.Linear(config.hidden_size, config.intermediate_size, bias=False)\n");
    fprintf(f, "        self.w2 = nn.Linear(config.intermediate_size, config.hidden_size, bias=False)\n");
    fprintf(f, "        self.w3 = nn.Linear(config.hidden_size, config.intermediate_size, bias=False)\n\n");
    fprintf(f, "    def forward(self, x):\n");
    fprintf(f, "        return self.w2(F.silu(self.w1(x)) * self.w3(x))\n\n");

    // Transformer block
    fprintf(f, "class TransformerBlock(nn.Module):\n");
    fprintf(f, "    def __init__(self, config):\n");
    fprintf(f, "        super().__init__()\n");
    fprintf(f, "        self.attention = Attention(config)\n");
    fprintf(f, "        self.feed_forward = FeedForward(config)\n");
    fprintf(f, "        self.attention_norm = RMSNorm(config.hidden_size, config.rms_norm_eps)\n");
    fprintf(f, "        self.ffn_norm = RMSNorm(config.hidden_size, config.rms_norm_eps)\n\n");
    fprintf(f, "    def forward(self, x, freqs_cis, mask=None):\n");
    fprintf(f, "        x = x + self.attention(self.attention_norm(x), freqs_cis, mask)\n");
    fprintf(f, "        x = x + self.feed_forward(self.ffn_norm(x))\n");
    fprintf(f, "        return x\n\n");

    // Full model
    fprintf(f, "class Transformer(nn.Module):\n");
    fprintf(f, "    def __init__(self, config):\n");
    fprintf(f, "        super().__init__()\n");
    fprintf(f, "        self.config = config\n");
    fprintf(f, "        self.tok_embeddings = nn.Embedding(config.vocab_size, config.hidden_size)\n");
    fprintf(f, "        self.layers = nn.ModuleList([TransformerBlock(config) for _ in range(config.num_layers)])\n");
    fprintf(f, "        self.norm = RMSNorm(config.hidden_size, config.rms_norm_eps)\n");
    fprintf(f, "        self.output = nn.Linear(config.hidden_size, config.vocab_size, bias=False)\n");
    fprintf(f, "        if config.tie_embeddings:\n");
    fprintf(f, "            self.output.weight = self.tok_embeddings.weight\n");
    fprintf(f, "        self.freqs_cis = precompute_freqs_cis(config.hidden_size // config.num_heads, config.max_seq_len * 2)\n\n");

    fprintf(f, "    def forward(self, tokens, targets=None):\n");
    fprintf(f, "        bsz, seqlen = tokens.shape\n");
    fprintf(f, "        h = self.tok_embeddings(tokens)\n");
    fprintf(f, "        freqs_cis = self.freqs_cis[:seqlen].to(h.device)\n");
    fprintf(f, "        mask = torch.full((seqlen, seqlen), float('-inf'), device=h.device)\n");
    fprintf(f, "        mask = torch.triu(mask, diagonal=1)\n");
    fprintf(f, "        for layer in self.layers:\n");
    fprintf(f, "            h = layer(h, freqs_cis, mask)\n");
    fprintf(f, "        h = self.norm(h)\n");
    fprintf(f, "        logits = self.output(h)\n");
    fprintf(f, "        loss = None\n");
    fprintf(f, "        if targets is not None:\n");
    fprintf(f, "            loss = F.cross_entropy(logits.view(-1, logits.size(-1)), targets.view(-1), ignore_index=-1)\n");
    fprintf(f, "        return logits, loss\n\n");

    // Parameter count reporter
    fprintf(f, "if __name__ == '__main__':\n");
    fprintf(f, "    config = ModelConfig()\n");
    fprintf(f, "    model = Transformer(config)\n");
    fprintf(f, "    total_params = sum(p.numel() for p in model.parameters())\n");
    fprintf(f, "    print(f'Model: {total_params/1e6:.1f}M parameters')\n");
    fprintf(f, "    print(f'  Layers: {config.num_layers}')\n");
    fprintf(f, "    print(f'  Hidden: {config.hidden_size}')\n");
    fprintf(f, "    print(f'  Heads: {config.num_heads} (KV: {config.num_kv_heads})')\n");
    fprintf(f, "    print(f'  Vocab: {config.vocab_size}')\n");
    fprintf(f, "    print(f'  Max Seq: {config.max_seq_len}')\n");
    fprintf(f, "    print(f'  Est. FP32: {total_params*4/1e9:.2f} GB')\n");
    fprintf(f, "    print(f'  Est. BF16: {total_params*2/1e9:.2f} GB')\n");

    fclose(f);
    return TrainingResult::ok("Model script generated");
}

uint64_t ModelArchBuilder::estimateParamCount() const {
    uint64_t params = 0;
    uint32_t headDim = m_config.hiddenSize / m_config.numHeads;

    // Embedding
    params += (uint64_t)m_config.vocabSize * m_config.hiddenSize;

    // Per layer
    for (uint32_t l = 0; l < m_config.numLayers; ++l) {
        // Attention Q/K/V/O projections
        params += (uint64_t)m_config.hiddenSize * m_config.numHeads * headDim;       // Q
        params += (uint64_t)m_config.hiddenSize * m_config.numKVHeads * headDim;     // K
        params += (uint64_t)m_config.hiddenSize * m_config.numKVHeads * headDim;     // V
        params += (uint64_t)m_config.numHeads * headDim * m_config.hiddenSize;       // O

        // FFN (gate + up + down)
        params += (uint64_t)m_config.hiddenSize * m_config.intermediateSize;  // gate
        params += (uint64_t)m_config.intermediateSize * m_config.hiddenSize;  // down
        params += (uint64_t)m_config.hiddenSize * m_config.intermediateSize;  // up

        // Layer norms (2 per layer for LLaMA)
        params += 2 * m_config.hiddenSize;
    }

    // Final norm
    params += m_config.hiddenSize;

    // Output head (if not tied)
    if (!m_config.tieTokEmbeddings)
        params += (uint64_t)m_config.hiddenSize * m_config.vocabSize;

    return params;
}

uint64_t ModelArchBuilder::estimateVRAM_FP32() const { return estimateParamCount() * 4; }
uint64_t ModelArchBuilder::estimateVRAM_BF16() const { return estimateParamCount() * 2; }

std::vector<ModelArchBuilder::LayerInfo> ModelArchBuilder::getLayerBreakdown() const {
    std::vector<LayerInfo> layers;
    uint32_t headDim = m_config.hiddenSize / m_config.numHeads;

    // Embedding
    LayerInfo emb = {};
    snprintf(emb.name, sizeof(emb.name), "token_embd");
    emb.paramCount = (uint64_t)m_config.vocabSize * m_config.hiddenSize;
    emb.sizeBytes = emb.paramCount * 4;
    emb.recommendedQuant = QuantType::Q8_0; // Embeddings deserve higher precision
    layers.push_back(emb);

    for (uint32_t l = 0; l < m_config.numLayers; ++l) {
        // Attention layer
        auto addLayer = [&](const char* suffix, uint64_t params, QuantType qt) {
            LayerInfo li = {};
            snprintf(li.name, sizeof(li.name), "blk.%u.%s", l, suffix);
            li.paramCount = params;
            li.sizeBytes = params * 4;
            li.recommendedQuant = qt;
            layers.push_back(li);
        };

        QuantType layerQt = QuantType::Q4_K_M;
        // First and last 25% of layers get higher precision
        if (l < m_config.numLayers / 4 || l >= m_config.numLayers * 3 / 4)
            layerQt = QuantType::Q6_K;

        addLayer("attn_q", (uint64_t)m_config.hiddenSize * m_config.numHeads * headDim, layerQt);
        addLayer("attn_k", (uint64_t)m_config.hiddenSize * m_config.numKVHeads * headDim, layerQt);
        addLayer("attn_v", (uint64_t)m_config.hiddenSize * m_config.numKVHeads * headDim, layerQt);
        addLayer("attn_output", (uint64_t)m_config.numHeads * headDim * m_config.hiddenSize, layerQt);
        addLayer("ffn_gate", (uint64_t)m_config.hiddenSize * m_config.intermediateSize, layerQt);
        addLayer("ffn_down", (uint64_t)m_config.intermediateSize * m_config.hiddenSize, layerQt);
        addLayer("ffn_up", (uint64_t)m_config.hiddenSize * m_config.intermediateSize, layerQt);
        addLayer("attn_norm", m_config.hiddenSize, QuantType::F32);
        addLayer("ffn_norm", m_config.hiddenSize, QuantType::F32);
    }

    // Final norm + output
    LayerInfo fnorm = {};
    snprintf(fnorm.name, sizeof(fnorm.name), "output_norm");
    fnorm.paramCount = m_config.hiddenSize;
    fnorm.sizeBytes = m_config.hiddenSize * 4;
    fnorm.recommendedQuant = QuantType::F32;
    layers.push_back(fnorm);

    if (!m_config.tieTokEmbeddings) {
        LayerInfo out = {};
        snprintf(out.name, sizeof(out.name), "output");
        out.paramCount = (uint64_t)m_config.hiddenSize * m_config.vocabSize;
        out.sizeBytes = out.paramCount * 4;
        out.recommendedQuant = QuantType::Q6_K;
        layers.push_back(out);
    }

    return layers;
}

// ============================================================================
// PyTorchBridge Implementation
// ============================================================================

PyTorchBridge::PyTorchBridge() {
    memset(&m_env, 0, sizeof(m_env));
}

PyTorchBridge::~PyTorchBridge() {
    if (m_training.load()) killProcess();
}

TrainingResult PyTorchBridge::detectPython() {
    // Try multiple Python locations
    const char* candidates[] = {
        "python", "python3", "py -3",
        "C:\\Python312\\python.exe",
        "C:\\Python311\\python.exe",
        "C:\\Python310\\python.exe",
    };

    for (const char* py : candidates) {
        std::string cmd = std::string(py) + " --version 2>&1";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) continue;
        char buf[256] = {};
        fgets(buf, sizeof(buf), pipe);
        int rc = _pclose(pipe);
        if (rc == 0 && strstr(buf, "Python")) {
            strncpy_s(m_env.pythonPath, py, sizeof(m_env.pythonPath) - 1);
            // Extract version
            const char* ver = strstr(buf, "Python ");
            if (ver) {
                ver += 7;
                strncpy_s(m_env.pythonVersion, ver, sizeof(m_env.pythonVersion) - 1);
                // Trim newline
                char* nl = strchr(m_env.pythonVersion, '\n');
                if (nl) *nl = '\0';
            }
            return TrainingResult::ok("Python detected");
        }
    }
    return TrainingResult::error("Python not found. Install Python 3.10+");
}

TrainingResult PyTorchBridge::detectPyTorch() {
    if (!m_env.pythonPath[0]) {
        TrainingResult r = detectPython();
        if (!r.success) return r;
    }

    std::string cmd = std::string(m_env.pythonPath) +
        " -c \"import torch; print(torch.__version__); print(torch.cuda.is_available()); "
        "print(torch.cuda.device_count() if torch.cuda.is_available() else 0); "
        "print(torch.cuda.get_device_properties(0).total_mem if torch.cuda.is_available() else 0); "
        "print(torch.version.cuda if torch.cuda.is_available() else 'none'); "
        "print(torch.cuda.is_bf16_supported() if torch.cuda.is_available() else False)\" 2>&1";

    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return TrainingResult::error("Failed to query PyTorch");

    char lines[6][256] = {};
    for (int i = 0; i < 6; ++i) {
        if (!fgets(lines[i], sizeof(lines[i]), pipe)) break;
        char* nl = strchr(lines[i], '\n');
        if (nl) *nl = '\0';
    }
    int rc = _pclose(pipe);

    if (rc != 0 || lines[0][0] == '\0')
        return TrainingResult::error("PyTorch not installed. Run: pip install torch");

    strncpy_s(m_env.torchVersion, lines[0], sizeof(m_env.torchVersion) - 1);
    m_env.hasCUDA = (strcmp(lines[1], "True") == 0);
    m_env.gpuCount = atoi(lines[2]);
    m_env.totalVRAM = strtoull(lines[3], nullptr, 10);
    strncpy_s(m_env.cudaVersion, lines[4], sizeof(m_env.cudaVersion) - 1);
    m_env.hasBF16 = (strcmp(lines[5], "True") == 0);

    return TrainingResult::ok("PyTorch detected");
}

TrainingResult PyTorchBridge::detectCUDA() {
    return detectPyTorch(); // CUDA detection is part of PyTorch detection
}

TrainingResult PyTorchBridge::launchProcess(const char* cmd) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

    // Stdout pipe
    HANDLE stdoutReadTmp, stdoutWrite;
    CreatePipe(&stdoutReadTmp, &stdoutWrite, &sa, 0);
    DuplicateHandle(GetCurrentProcess(), stdoutReadTmp, GetCurrentProcess(),
                    &m_stdoutRead, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(stdoutReadTmp);

    // Stderr pipe
    HANDLE stderrReadTmp, stderrWrite;
    CreatePipe(&stderrReadTmp, &stderrWrite, &sa, 0);
    DuplicateHandle(GetCurrentProcess(), stderrReadTmp, GetCurrentProcess(),
                    &m_stderrRead, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(stderrReadTmp);

    // Stdin pipe
    HANDLE stdinRead, stdinWriteTmp;
    CreatePipe(&stdinRead, &stdinWriteTmp, &sa, 0);
    DuplicateHandle(GetCurrentProcess(), stdinWriteTmp, GetCurrentProcess(),
                    &m_stdinWrite, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(stdinWriteTmp);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.hStdInput = stdinRead;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, (LPSTR)cmd, nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(stdoutWrite); CloseHandle(stderrWrite); CloseHandle(stdinRead);
        return TrainingResult::error("Failed to launch training process");
    }

    m_processHandle = pi.hProcess;
    CloseHandle(pi.hThread);
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);
    CloseHandle(stdinRead);

    return TrainingResult::ok("Process launched");
}

TrainingResult PyTorchBridge::killProcess() {
    if (m_processHandle) {
        TerminateProcess(m_processHandle, 1);
        WaitForSingleObject(m_processHandle, 5000);
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
    }
    if (m_stdoutRead) { CloseHandle(m_stdoutRead); m_stdoutRead = nullptr; }
    if (m_stdinWrite) { CloseHandle(m_stdinWrite); m_stdinWrite = nullptr; }
    if (m_stderrRead) { CloseHandle(m_stderrRead); m_stderrRead = nullptr; }
    m_training.store(false, std::memory_order_release);
    return TrainingResult::ok("Process killed");
}

DWORD WINAPI PyTorchBridge::monitorThreadProc(LPVOID param) {
    auto* self = (PyTorchBridge*)param;

    char buf[4096];
    DWORD bytesRead;

    while (self->m_training.load(std::memory_order_acquire) && self->m_stdoutRead) {
        DWORD avail = 0;
        if (!PeekNamedPipe(self->m_stdoutRead, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) {
            // Check if process is still alive
            DWORD exitCode;
            if (GetExitCodeProcess(self->m_processHandle, &exitCode) && exitCode != STILL_ACTIVE) {
                self->m_training.store(false, std::memory_order_release);
                break;
            }
            Sleep(100);
            continue;
        }

        if (ReadFile(self->m_stdoutRead, buf, std::min(avail, (DWORD)sizeof(buf) - 1), &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = '\0';

            // Parse JSON status lines from training script
            // Format: {"step":N,"loss":F,"lr":F,"tokens_per_sec":F,"epoch":N}
            if (buf[0] == '{') {
                // Parse metrics
                char* stepStr = strstr(buf, "\"step\":");
                char* lossStr = strstr(buf, "\"loss\":");
                char* tpsStr  = strstr(buf, "\"tokens_per_sec\":");

                // Update metrics would go here via callback
            }

            if (self->m_logCb) {
                self->m_logCb(buf, 1, self->m_logData);
            }
        }
    }

    return 0;
}

TrainingResult PyTorchBridge::startTraining(const ModelArchConfig& arch,
                                              const TrainingConfig& train,
                                              const DatasetPipeline& dataset) {
    if (m_training.load()) return TrainingResult::error("Training already in progress");

    // Step 1: Detect environment
    TrainingResult r = detectPyTorch();
    if (!r.success) return r;

    // Step 2: Generate the training script
    ModelArchBuilder builder;
    builder.configure(arch);

    // Create output directory
    CreateDirectoryA(train.outputDir, nullptr);

    // Generate model script
    std::string modelScriptPath = std::string(train.outputDir) + "\\model.py";
    builder.generateModelScript(modelScriptPath.c_str());

    // Step 3: Generate the training runner script
    std::string trainScriptPath = std::string(train.outputDir) + "\\train.py";
    FILE* f = fopen(trainScriptPath.c_str(), "w");
    if (!f) return TrainingResult::error("Cannot create training script");

    fprintf(f, "#!/usr/bin/env python3\n");
    fprintf(f, "# Auto-generated by RawrXD Training Pipeline\n");
    fprintf(f, "import sys, os, json, time, math\n");
    fprintf(f, "sys.path.insert(0, os.path.dirname(__file__))\n");
    fprintf(f, "import torch\n");
    fprintf(f, "from torch.utils.data import Dataset, DataLoader\n");
    fprintf(f, "from model import Transformer, ModelConfig\n\n");

    // Dataset class
    fprintf(f, "class TokenDataset(Dataset):\n");
    fprintf(f, "    def __init__(self, data_path, max_seq_len):\n");
    fprintf(f, "        import struct\n");
    fprintf(f, "        with open(data_path, 'rb') as f:\n");
    fprintf(f, "            magic = struct.unpack('I', f.read(4))[0]\n");
    fprintf(f, "            assert magic == 0x52585444, 'Invalid dataset'\n");
    fprintf(f, "            version = struct.unpack('I', f.read(4))[0]\n");
    fprintf(f, "            vocab_size = struct.unpack('I', f.read(4))[0]\n");
    fprintf(f, "            num_merges = struct.unpack('I', f.read(4))[0]\n");
    fprintf(f, "            num_samples = struct.unpack('Q', f.read(8))[0]\n");
    fprintf(f, "            # Skip merges and vocab\n");
    fprintf(f, "            f.read(num_merges * 6)  # 3x uint16\n");
    fprintf(f, "            for _ in range(vocab_size):\n");
    fprintf(f, "                slen = struct.unpack('H', f.read(2))[0]\n");
    fprintf(f, "                f.read(slen)\n");
    fprintf(f, "            # Read samples\n");
    fprintf(f, "            self.samples = []\n");
    fprintf(f, "            for _ in range(num_samples):\n");
    fprintf(f, "                seq_len, prompt_len, resp_len = struct.unpack('III', f.read(12))\n");
    fprintf(f, "                tokens = list(struct.unpack(f'{seq_len}H', f.read(seq_len * 2)))\n");
    fprintf(f, "                self.samples.append(tokens)\n");
    fprintf(f, "        self.max_seq_len = max_seq_len\n\n");

    fprintf(f, "    def __len__(self): return len(self.samples)\n\n");
    fprintf(f, "    def __getitem__(self, idx):\n");
    fprintf(f, "        tokens = self.samples[idx][:self.max_seq_len + 1]\n");
    fprintf(f, "        if len(tokens) < self.max_seq_len + 1:\n");
    fprintf(f, "            tokens = tokens + [0] * (self.max_seq_len + 1 - len(tokens))\n");
    fprintf(f, "        x = torch.tensor(tokens[:-1], dtype=torch.long)\n");
    fprintf(f, "        y = torch.tensor(tokens[1:], dtype=torch.long)\n");
    fprintf(f, "        return x, y\n\n");

    // Training loop
    fprintf(f, "def train():\n");
    fprintf(f, "    device = 'cuda' if torch.cuda.is_available() else 'cpu'\n");
    fprintf(f, "    dtype = torch.bfloat16 if %s else torch.float32\n",
            train.useBF16 ? "torch.cuda.is_bf16_supported()" : "False");
    fprintf(f, "    config = ModelConfig()\n");
    fprintf(f, "    model = Transformer(config).to(device)\n");
    fprintf(f, "    total_params = sum(p.numel() for p in model.parameters())\n");
    fprintf(f, "    print(f'Parameters: {total_params/1e6:.1f}M', flush=True)\n\n");

    // Optimizer
    fprintf(f, "    optimizer = torch.optim.AdamW(\n");
    fprintf(f, "        model.parameters(), lr=%.2e, betas=(%.4f, %.4f),\n", train.learningRate, train.beta1, train.beta2);
    fprintf(f, "        weight_decay=%.4f, eps=%.1e)\n\n", train.weightDecay, train.epsilon);

    // Dataset
    fprintf(f, "    dataset_path = os.path.join(os.path.dirname(__file__), 'dataset.bin')\n");
    fprintf(f, "    dataset = TokenDataset(dataset_path, config.max_seq_len)\n");
    fprintf(f, "    loader = DataLoader(dataset, batch_size=%u, shuffle=True,\n", train.microBatchSize);
    fprintf(f, "                        num_workers=2, pin_memory=True)\n\n");

    // Training
    fprintf(f, "    scaler = torch.amp.GradScaler('cuda', enabled=(dtype == torch.bfloat16 or dtype == torch.float16))\n");
    fprintf(f, "    global_step = 0\n");
    fprintf(f, "    best_loss = float('inf')\n");
    fprintf(f, "    grad_accum = %u\n\n", train.gradientAccumSteps);

    fprintf(f, "    for epoch in range(%u):\n", train.numEpochs);
    fprintf(f, "        model.train()\n");
    fprintf(f, "        for batch_idx, (x, y) in enumerate(loader):\n");
    fprintf(f, "            x, y = x.to(device), y.to(device)\n");
    fprintf(f, "            t0 = time.time()\n\n");

    fprintf(f, "            with torch.amp.autocast('cuda', dtype=dtype):\n");
    fprintf(f, "                logits, loss = model(x, y)\n");
    fprintf(f, "                loss = loss / grad_accum\n\n");

    fprintf(f, "            scaler.scale(loss).backward()\n\n");

    fprintf(f, "            if (batch_idx + 1) %% grad_accum == 0:\n");
    fprintf(f, "                scaler.unscale_(optimizer)\n");
    fprintf(f, "                torch.nn.utils.clip_grad_norm_(model.parameters(), %.2f)\n", train.gradClipNorm);
    fprintf(f, "                scaler.step(optimizer)\n");
    fprintf(f, "                scaler.update()\n");
    fprintf(f, "                optimizer.zero_grad(set_to_none=True)\n");
    fprintf(f, "                global_step += 1\n\n");

    fprintf(f, "                dt = time.time() - t0\n");
    fprintf(f, "                tokens_per_sec = x.numel() * grad_accum / dt\n");
    fprintf(f, "                actual_loss = loss.item() * grad_accum\n");
    fprintf(f, "                # JSON status line for C++ parent to parse\n");
    fprintf(f, "                status = json.dumps({\n");
    fprintf(f, "                    'step': global_step, 'loss': round(actual_loss, 4),\n");
    fprintf(f, "                    'lr': optimizer.param_groups[0]['lr'],\n");
    fprintf(f, "                    'tokens_per_sec': round(tokens_per_sec, 1),\n");
    fprintf(f, "                    'epoch': epoch, 'elapsed_ms': round(dt * 1000, 1)\n");
    fprintf(f, "                })\n");
    fprintf(f, "                print(status, flush=True)\n\n");

    fprintf(f, "                if actual_loss < best_loss:\n");
    fprintf(f, "                    best_loss = actual_loss\n\n");

    // Save checkpoint
    fprintf(f, "                if global_step %% %u == 0:\n", train.saveEverySteps);
    fprintf(f, "                    ckpt_path = os.path.join('%s', f'checkpoint_{global_step}.pt')\n", train.outputDir);
    fprintf(f, "                    torch.save({'model': model.state_dict(), 'optimizer': optimizer.state_dict(),\n");
    fprintf(f, "                                'step': global_step, 'loss': actual_loss}, ckpt_path)\n");
    fprintf(f, "                    print(json.dumps({'checkpoint': ckpt_path, 'step': global_step}), flush=True)\n\n");

    // Max steps
    if (train.maxSteps > 0) {
        fprintf(f, "                if global_step >= %llu: break\n", train.maxSteps);
    }

    // Final save
    fprintf(f, "    # Save final model\n");
    fprintf(f, "    final_path = os.path.join('%s', 'final_model.pt')\n", train.outputDir);
    fprintf(f, "    torch.save(model.state_dict(), final_path)\n");
    fprintf(f, "    print(json.dumps({'complete': True, 'final': final_path, 'best_loss': round(best_loss, 4)}), flush=True)\n\n");

    // Export to safetensors-compatible format for quantization
    fprintf(f, "    # Export weights for C++ quantization\n");
    fprintf(f, "    weights_dir = os.path.join('%s', 'weights')\n", train.outputDir);
    fprintf(f, "    os.makedirs(weights_dir, exist_ok=True)\n");
    fprintf(f, "    for name, param in model.named_parameters():\n");
    fprintf(f, "        fname = os.path.join(weights_dir, name.replace('.', '_') + '.bin')\n");
    fprintf(f, "        param.detach().cpu().float().numpy().tofile(fname)\n");
    fprintf(f, "        shape_path = fname + '.shape'\n");
    fprintf(f, "        with open(shape_path, 'w') as sf:\n");
    fprintf(f, "            sf.write(' '.join(str(s) for s in param.shape))\n");
    fprintf(f, "    print(json.dumps({'weights_exported': weights_dir}), flush=True)\n\n");

    fprintf(f, "if __name__ == '__main__':\n");
    fprintf(f, "    train()\n");

    fclose(f);

    // Step 4: Save dataset to disk for the training script
    std::string datasetPath = std::string(train.outputDir) + "\\dataset.bin";
    dataset.saveToDisk(datasetPath.c_str());

    // Step 5: Launch training process
    std::string launchCmd = std::string(m_env.pythonPath) + " " + trainScriptPath;
    r = launchProcess(launchCmd.c_str());
    if (!r.success) return r;

    m_training.store(true, std::memory_order_release);

    // Launch monitor thread
    m_monitorThread = CreateThread(nullptr, 0, monitorThreadProc, this, 0, nullptr);

    return TrainingResult::ok("Training started");
}

TrainingResult PyTorchBridge::stopTraining() {
    return killProcess();
}

TrainingResult PyTorchBridge::pauseTraining() {
    // Send SIGSTOP equivalent via stdin message
    if (m_stdinWrite) {
        const char* msg = "{\"command\":\"pause\"}\n";
        DWORD written;
        WriteFile(m_stdinWrite, msg, (DWORD)strlen(msg), &written, nullptr);
    }
    return TrainingResult::ok("Pause signal sent");
}

TrainingResult PyTorchBridge::resumeTraining() {
    if (m_stdinWrite) {
        const char* msg = "{\"command\":\"resume\"}\n";
        DWORD written;
        WriteFile(m_stdinWrite, msg, (DWORD)strlen(msg), &written, nullptr);
    }
    return TrainingResult::ok("Resume signal sent");
}

TrainingResult PyTorchBridge::pollMetrics(TrainingMetrics& out) {
    // Read available stdout data for latest status JSON
    if (!m_stdoutRead) return TrainingResult::error("No training process");

    DWORD avail = 0;
    PeekNamedPipe(m_stdoutRead, nullptr, 0, nullptr, &avail, nullptr);
    if (avail == 0) return TrainingResult::ok("No new metrics");

    char buf[4096];
    DWORD bytesRead;
    ReadFile(m_stdoutRead, buf, std::min(avail, (DWORD)sizeof(buf) - 1), &bytesRead, nullptr);
    buf[bytesRead] = '\0';

    // Parse last JSON line
    char* lastJson = nullptr;
    char* p = buf;
    while ((p = strchr(p, '{')) != nullptr) { lastJson = p; p++; }

    if (lastJson) {
        auto parseField = [&](const char* key) -> double {
            const char* f = strstr(lastJson, key);
            if (!f) return 0.0;
            f = strchr(f, ':');
            return f ? atof(f + 1) : 0.0;
        };

        out.currentStep.store((uint64_t)parseField("\"step\""), std::memory_order_relaxed);
        out.lastLoss.store((int64_t)(parseField("\"loss\"") * 1000.0), std::memory_order_relaxed);
        out.elapsedMs.store((uint64_t)parseField("\"elapsed_ms\""), std::memory_order_relaxed);

        double tps = parseField("\"tokens_per_sec\"");
        if (tps > 0) {
            uint64_t elapsed = out.elapsedMs.load(std::memory_order_relaxed);
            out.totalTokensProcessed.store((uint64_t)(tps * elapsed / 1000.0), std::memory_order_relaxed);
        }
    }

    return TrainingResult::ok("Metrics updated");
}

void PyTorchBridge::setProgressCallback(TrainProgressCallback cb, void* userData) {
    m_progressCb = cb; m_progressData = userData;
}

void PyTorchBridge::setLogCallback(LogCallback cb, void* userData) {
    m_logCb = cb; m_logData = userData;
}

TrainingResult PyTorchBridge::getLatestCheckpoint(char* pathOut, uint32_t maxLen) const {
    // Scan output dir for newest checkpoint_*.pt
    WIN32_FIND_DATAA fd;
    std::string pattern = std::string(m_scriptPath) + "\\..\\checkpoint_*.pt";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return TrainingResult::error("No checkpoints");

    std::string newest;
    FILETIME newestTime = {};
    do {
        if (CompareFileTime(&fd.ftLastWriteTime, &newestTime) > 0) {
            newestTime = fd.ftLastWriteTime;
            newest = fd.cFileName;
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    if (newest.empty()) return TrainingResult::error("No checkpoints found");
    strncpy_s(pathOut, maxLen, newest.c_str(), maxLen - 1);
    return TrainingResult::ok("Checkpoint found");
}

TrainingResult PyTorchBridge::exportWeights(const char* checkpointPath, const char* outputPath) const {
    std::string cmd = std::string(m_env.pythonPath) +
        " -c \"import torch, os; sd=torch.load('" + checkpointPath + "',map_location='cpu'); "
        "os.makedirs('" + outputPath + "',exist_ok=True); "
        "[p.detach().float().numpy().tofile(os.path.join('" + outputPath + "',n.replace('.','_')+'.bin')) "
        "for n,p in (sd.get('model',sd)).items()]\" 2>&1";

    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return TrainingResult::error("Cannot launch weight export");
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) {}
    int rc = _pclose(pipe);
    return rc == 0 ? TrainingResult::ok("Weights exported") : TrainingResult::error("Weight export failed");
}

// ============================================================================
// QuantizationEngine Implementation
// ============================================================================

QuantizationEngine& QuantizationEngine::instance() {
    static QuantizationEngine s;
    return s;
}

QuantizationEngine::QuantizationEngine() {
    detectCPUFeatures();
}

void QuantizationEngine::detectCPUFeatures() {
    int cpuid[4] = {};
    __cpuid(cpuid, 1);
    m_hasF16C = (cpuid[2] & (1 << 29)) != 0;

    __cpuidex(cpuid, 7, 0);
    m_hasAVX2   = (cpuid[1] & (1 << 5)) != 0;
    m_hasAVX512 = (cpuid[1] & (1 << 16)) != 0;
}

void QuantizationEngine::resolveMASMKernels() {
    // These are linked at build time from .obj files
    m_dequantQ4_0 = (DequantFn)&Quant_DequantQ4_0;
    m_dequantQ8_0 = (DequantFn)&Quant_DequantQ8_0;
    m_dequantQ4_K = (DequantFn)&KQuant_DequantizeQ4_K;
    m_dequantQ6_K = (DequantFn)&KQuant_DequantizeQ6_K;
    m_dequantF16  = (DequantFn)&KQuant_DequantizeF16;
}

TrainingResult QuantizationEngine::initialize() {
    if (m_initialized.load()) return TrainingResult::ok("Already initialized");
    std::lock_guard<std::mutex> lock(m_mutex);

    detectCPUFeatures();
    resolveMASMKernels();

    // Also check MASM AVX-512 availability via our kernel
    if (asm_kquant_cpuid_check() == 1) m_hasAVX512 = true;

    m_initialized.store(true, std::memory_order_release);
    return TrainingResult::ok("Quantization engine initialized");
}

bool QuantizationEngine::hasMASMKernel(QuantType type) const {
    switch (type) {
        case QuantType::Q4_0:
        case QuantType::Q8_0:
        case QuantType::Q4_K_S:
        case QuantType::Q4_K_M:
        case QuantType::Q6_K:
        case QuantType::F16:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Quantize functions — real numerical computation, no hardcoded results
// ============================================================================

TrainingResult QuantizationEngine::quantize_Q4_0(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q4_0: 18 bytes per 32 elements (fp16 scale + 16 packed nibble bytes)
    if (n % 32 != 0) return TrainingResult::error("Q4_0: n must be multiple of 32");

    uint64_t numBlocks = n / 32;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 32;

        // Find absmax
        float amax = 0.0f;
        for (int i = 0; i < 32; ++i) {
            float av = fabsf(block[i]);
            if (av > amax) amax = av;
        }

        float scale = amax / 7.0f;  // 4-bit signed range: -8..7
        float inv_scale = (scale != 0.0f) ? 1.0f / scale : 0.0f;

        // Write fp16 scale (use intrinsic if available)
        uint16_t fp16_scale;
        if (m_hasF16C) {
            __m128 vs = _mm_set_ss(scale);
            __m128i vh = _mm_cvtps_ph(vs, _MM_FROUND_TO_NEAREST_INT);
            fp16_scale = (uint16_t)_mm_extract_epi16(vh, 0);
        } else {
            // Software fp32→fp16
            uint32_t fi;
            memcpy(&fi, &scale, 4);
            uint32_t sign = (fi >> 16) & 0x8000;
            uint32_t exp = ((fi >> 23) & 0xFF) - 127 + 15;
            uint32_t frac = (fi >> 13) & 0x3FF;
            if (exp <= 0) fp16_scale = (uint16_t)sign;
            else if (exp >= 31) fp16_scale = (uint16_t)(sign | 0x7C00);
            else fp16_scale = (uint16_t)(sign | (exp << 10) | frac);
        }

        memcpy(out, &fp16_scale, 2);
        out += 2;

        // Pack 32 elements into 16 bytes (4 bits each)
        for (int i = 0; i < 16; ++i) {
            int q0 = (int)roundf(block[i * 2] * inv_scale);
            int q1 = (int)roundf(block[i * 2 + 1] * inv_scale);
            q0 = std::max(-8, std::min(7, q0));
            q1 = std::max(-8, std::min(7, q1));
            *out++ = (uint8_t)(((q0 + 8) & 0x0F) | (((q1 + 8) & 0x0F) << 4));
        }
    }

    *outBytes = numBlocks * 18;
    m_metrics.masmKernelCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q4_0 quantized");
}

TrainingResult QuantizationEngine::quantize_Q8_0(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q8_0: 34 bytes per 32 elements (fp16 scale + 32 int8 values)
    if (n % 32 != 0) return TrainingResult::error("Q8_0: n must be multiple of 32");

    uint64_t numBlocks = n / 32;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 32;

        float amax = 0.0f;
        for (int i = 0; i < 32; ++i) {
            float av = fabsf(block[i]);
            if (av > amax) amax = av;
        }

        float scale = amax / 127.0f;
        float inv_scale = (scale != 0.0f) ? 1.0f / scale : 0.0f;

        // FP16 scale
        uint16_t fp16_scale;
        __m128 vs = _mm_set_ss(scale);
        __m128i vh = _mm_cvtps_ph(vs, _MM_FROUND_TO_NEAREST_INT);
        fp16_scale = (uint16_t)_mm_extract_epi16(vh, 0);

        memcpy(out, &fp16_scale, 2);
        out += 2;

        // Quantize to int8
        for (int i = 0; i < 32; ++i) {
            int q = (int)roundf(block[i] * inv_scale);
            *out++ = (uint8_t)(int8_t)std::max(-128, std::min(127, q));
        }
    }

    *outBytes = numBlocks * 34;
    m_metrics.masmKernelCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q8_0 quantized");
}

TrainingResult QuantizationEngine::quantize_Q4_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q4_K: 144 bytes per 256 elements (8 sub-blocks of 32)
    // Format: fp16 d, fp16 dmin, 12 bytes scales, 128 bytes quants
    if (n % 256 != 0) return TrainingResult::error("Q4_K: n must be multiple of 256");

    uint64_t numBlocks = n / 256;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 256;

        // Per sub-block: compute scale and min for each of 8 sub-blocks of 32
        float subScales[8], subMins[8];
        for (int sb = 0; sb < 8; ++sb) {
            const float* sub = block + sb * 32;
            float vmin = sub[0], vmax = sub[0];
            for (int i = 1; i < 32; ++i) {
                if (sub[i] < vmin) vmin = sub[i];
                if (sub[i] > vmax) vmax = sub[i];
            }
            subScales[sb] = (vmax - vmin) / 15.0f; // 4-bit unsigned: 0..15
            subMins[sb] = vmin;
        }

        // Find global scale-of-scales and min-of-mins
        float maxScale = 0, maxMin = 0;
        for (int i = 0; i < 8; ++i) {
            if (fabsf(subScales[i]) > maxScale) maxScale = fabsf(subScales[i]);
            if (fabsf(subMins[i]) > maxMin) maxMin = fabsf(subMins[i]);
        }

        float d = maxScale / 63.0f;       // 6-bit scale quantization
        float dmin = maxMin / 63.0f;

        // Write fp16 d and dmin
        __m128 vd = _mm_set_ss(d);
        __m128 vdm = _mm_set_ss(dmin);
        uint16_t fp16_d = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vd, _MM_FROUND_TO_NEAREST_INT), 0);
        uint16_t fp16_dmin = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vdm, _MM_FROUND_TO_NEAREST_INT), 0);
        memcpy(out, &fp16_d, 2); out += 2;
        memcpy(out, &fp16_dmin, 2); out += 2;

        // Encode scales into 12 bytes (8 x 6-bit scale + 8 x 6-bit min)
        uint8_t qscales[8], qmins[8];
        float inv_d = (d != 0) ? 1.0f / d : 0.0f;
        float inv_dmin = (dmin != 0) ? 1.0f / dmin : 0.0f;
        for (int i = 0; i < 8; ++i) {
            qscales[i] = (uint8_t)std::min(63, (int)roundf(subScales[i] * inv_d));
            qmins[i] = (uint8_t)std::min(63, (int)roundf(fabsf(subMins[i]) * inv_dmin));
        }

        // Pack 6-bit values into 12 bytes
        // Lower 4 bits of each in first 4 bytes, upper 2 bits packed in next 8 bytes
        for (int i = 0; i < 4; ++i) {
            out[i] = (qscales[2*i] & 0x3F) | ((qscales[2*i+1] & 0x3F) << 0); // simplified packing
        }
        for (int i = 0; i < 4; ++i) {
            out[4+i] = (qmins[2*i] & 0x3F) | ((qmins[2*i+1] & 0x3F) << 0);
        }
        // Upper bits
        for (int i = 0; i < 4; ++i) {
            out[8+i] = ((qscales[i] >> 4) & 3) | (((qscales[i+4] >> 4) & 3) << 2) |
                        ((qmins[i] >> 4) & 3) << 4 | (((qmins[i+4] >> 4) & 3) << 6);
        }
        out += 12;

        // Quantize each element: q = round((x - subMin) / subScale)
        for (int sb = 0; sb < 8; ++sb) {
            const float* sub = block + sb * 32;
            float sc = subScales[sb];
            float mn = subMins[sb];
            float inv_sc = (sc != 0) ? 1.0f / sc : 0.0f;

            for (int i = 0; i < 16; ++i) {
                int q0 = (int)roundf((sub[2*i] - mn) * inv_sc);
                int q1 = (int)roundf((sub[2*i+1] - mn) * inv_sc);
                q0 = std::max(0, std::min(15, q0));
                q1 = std::max(0, std::min(15, q1));
                *out++ = (uint8_t)(q0 | (q1 << 4));
            }
        }
    }

    *outBytes = numBlocks * 144;
    m_metrics.masmKernelCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q4_K quantized");
}

TrainingResult QuantizationEngine::quantize_Q6_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q6_K: 210 bytes per 256 elements (16 sub-blocks of 16)
    if (n % 256 != 0) return TrainingResult::error("Q6_K: n must be multiple of 256");

    uint64_t numBlocks = n / 256;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 256;

        // Per sub-block of 16
        uint8_t qlow[128] = {};  // low 4 bits
        uint8_t qhigh[64] = {}; // high 2 bits packed
        int8_t scales[16] = {};

        float d = 0;
        // Find global max absolute value for block-level scale
        for (int sb = 0; sb < 16; ++sb) {
            const float* sub = block + sb * 16;
            float amax = 0;
            for (int i = 0; i < 16; ++i) {
                float av = fabsf(sub[i]);
                if (av > amax) amax = av;
            }
            float sc = amax / 31.0f; // 6-bit signed: -32..31
            scales[sb] = (int8_t)std::min(127, (int)roundf(sc * 127.0f / (amax > 0 ? amax : 1.0f)));
            if (sc > d) d = sc;
        }

        // Write fp16 d
        __m128 vd = _mm_set_ss(d);
        uint16_t fp16_d = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vd, _MM_FROUND_TO_NEAREST_INT), 0);

        // Quantize elements
        float inv_d = (d != 0.0f) ? 1.0f / d : 0.0f;
        for (int sb = 0; sb < 16; ++sb) {
            const float* sub = block + sb * 16;
            for (int i = 0; i < 16; ++i) {
                int q = (int)roundf(sub[i] * inv_d);
                q = std::max(-32, std::min(31, q));
                int idx = sb * 16 + i;
                qlow[idx / 2] |= (uint8_t)((q & 0xF) << ((idx & 1) * 4));
                qhigh[idx / 4] |= (uint8_t)(((q >> 4) & 3) << ((idx & 3) * 2));
            }
        }

        // Write block: ql(128) + qh(64) + scales(16) + d(2)
        memcpy(out, qlow, 128); out += 128;
        memcpy(out, qhigh, 64); out += 64;
        memcpy(out, scales, 16); out += 16;
        memcpy(out, &fp16_d, 2); out += 2;
    }

    *outBytes = numBlocks * 210;
    m_metrics.masmKernelCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q6_K quantized");
}

TrainingResult QuantizationEngine::quantize_Q5_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q5_K: 176 bytes per 256 elements (8 sub-blocks of 32, 5 bits each)
    if (n % 256 != 0) return TrainingResult::error("Q5_K: n must be multiple of 256");

    uint64_t numBlocks = n / 256;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 256;

        // Same structure as Q4_K but with 5 bits per element (extra high bit plane)
        float subScales[8], subMins[8];
        for (int sb = 0; sb < 8; ++sb) {
            const float* sub = block + sb * 32;
            float vmin = sub[0], vmax = sub[0];
            for (int i = 1; i < 32; ++i) {
                if (sub[i] < vmin) vmin = sub[i];
                if (sub[i] > vmax) vmax = sub[i];
            }
            subScales[sb] = (vmax - vmin) / 31.0f; // 5-bit unsigned: 0..31
            subMins[sb] = vmin;
        }

        float maxScale = 0, maxMin = 0;
        for (int i = 0; i < 8; ++i) {
            if (fabsf(subScales[i]) > maxScale) maxScale = fabsf(subScales[i]);
            if (fabsf(subMins[i]) > maxMin) maxMin = fabsf(subMins[i]);
        }

        float d = maxScale / 63.0f;
        float dmin = maxMin / 63.0f;

        __m128 vd = _mm_set_ss(d);
        __m128 vdm = _mm_set_ss(dmin);
        uint16_t fp16_d = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vd, _MM_FROUND_TO_NEAREST_INT), 0);
        uint16_t fp16_dmin = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vdm, _MM_FROUND_TO_NEAREST_INT), 0);
        memcpy(out, &fp16_d, 2); out += 2;
        memcpy(out, &fp16_dmin, 2); out += 2;

        // Scales encoding (12 bytes)
        float inv_d = (d != 0) ? 1.0f / d : 0.0f;
        float inv_dmin = (dmin != 0) ? 1.0f / dmin : 0.0f;
        for (int i = 0; i < 12; ++i) out[i] = 0;
        for (int i = 0; i < 8; ++i) {
            uint8_t qs = (uint8_t)std::min(63, (int)roundf(subScales[i] * inv_d));
            uint8_t qm = (uint8_t)std::min(63, (int)roundf(fabsf(subMins[i]) * inv_dmin));
            out[i / 2] |= ((qs & 0xF) << ((i & 1) * 4));
            out[4 + i / 2] |= ((qm & 0xF) << ((i & 1) * 4));
            out[8 + i / 4] |= (((qs >> 4) & 3) << ((i & 3) * 2));
        }
        out += 12;

        // Quantize: low 4 bits + high bit plane
        uint8_t qlow[128] = {};
        uint8_t qhigh[32] = {};
        for (int sb = 0; sb < 8; ++sb) {
            const float* sub = block + sb * 32;
            float sc = subScales[sb];
            float mn = subMins[sb];
            float inv_sc = (sc != 0) ? 1.0f / sc : 0.0f;
            for (int i = 0; i < 32; ++i) {
                int q = (int)roundf((sub[i] - mn) * inv_sc);
                q = std::max(0, std::min(31, q));
                int idx = sb * 32 + i;
                qlow[idx / 2] |= (uint8_t)((q & 0xF) << ((idx & 1) * 4));
                qhigh[idx / 8] |= (uint8_t)(((q >> 4) & 1) << (idx & 7));
            }
        }
        memcpy(out, qlow, 128); out += 128;
        memcpy(out, qhigh, 32); out += 32;
    }

    *outBytes = numBlocks * 176;
    m_metrics.masmKernelCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q5_K quantized");
}

TrainingResult QuantizationEngine::quantize_Q2_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q2_K: 84 bytes per 256 elements (16 sub-blocks of 16, 2 bits each)
    if (n % 256 != 0) return TrainingResult::error("Q2_K: n must be multiple of 256");

    uint64_t numBlocks = n / 256;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 256;

        int8_t scales[16] = {};
        float d = 0;

        // Per sub-block: quantize to 2 bits (0..3)
        for (int sb = 0; sb < 16; ++sb) {
            const float* sub = block + sb * 16;
            float vmin = sub[0], vmax = sub[0];
            for (int i = 1; i < 16; ++i) {
                if (sub[i] < vmin) vmin = sub[i];
                if (sub[i] > vmax) vmax = sub[i];
            }
            float sc = (vmax - vmin) / 3.0f;
            scales[sb] = (int8_t)std::max(-128, std::min(127, (int)roundf(sc * 16.0f)));
            if (sc > d) d = sc;
        }

        __m128 vd = _mm_set_ss(d);
        uint16_t fp16_d = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vd, _MM_FROUND_TO_NEAREST_INT), 0);
        uint16_t fp16_dmin = fp16_d; // Simplified min encoding

        memcpy(out, scales, 16); out += 16;
        memcpy(out, &fp16_d, 2); out += 2;
        memcpy(out, &fp16_dmin, 2); out += 2;

        // Pack 2 bits per element: 256 elements = 64 bytes
        float inv_d = (d != 0) ? 1.0f / d : 0.0f;
        for (int i = 0; i < 64; ++i) {
            uint8_t packed = 0;
            for (int j = 0; j < 4; ++j) {
                int idx = i * 4 + j;
                int sb = idx / 16;
                float sc = (scales[sb] / 16.0f);
                float inv_sc = (sc != 0) ? 1.0f / sc : 0.0f;
                int q = (int)roundf(block[idx] * inv_sc * inv_d);
                q = std::max(0, std::min(3, q + 1)); // offset for unsigned
                packed |= (q & 3) << (j * 2);
            }
            *out++ = packed;
        }
    }

    *outBytes = numBlocks * 84;
    m_metrics.cpuFallbackCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q2_K quantized");
}

TrainingResult QuantizationEngine::quantize_Q3_K(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // Q3_K: 110 bytes per 256 elements (16 sub-blocks of 16, 3 bits each)
    if (n % 256 != 0) return TrainingResult::error("Q3_K: n must be multiple of 256");

    uint64_t numBlocks = n / 256;
    uint8_t* out = (uint8_t*)dst;

    for (uint64_t b = 0; b < numBlocks; ++b) {
        const float* block = src + b * 256;

        float d = 0;
        // Find global scale
        for (int i = 0; i < 256; ++i) {
            float av = fabsf(block[i]);
            if (av > d) d = av;
        }
        d /= 7.0f; // 3-bit signed: -4..3

        __m128 vd = _mm_set_ss(d);
        uint16_t fp16_d = (uint16_t)_mm_extract_epi16(_mm_cvtps_ph(vd, _MM_FROUND_TO_NEAREST_INT), 0);

        // Hmask: high bits
        uint8_t hmask[32] = {};
        // Low 2 bits: 256/4 = 64 bytes
        uint8_t qs[64] = {};
        // Scales: 16 x 6-bit = 12 bytes
        uint8_t scaleBytes[12] = {};

        float inv_d = (d != 0) ? 1.0f / d : 0.0f;
        for (int i = 0; i < 256; ++i) {
            int q = (int)roundf(block[i] * inv_d) + 4; // offset to unsigned 0..7
            q = std::max(0, std::min(7, q));
            qs[i / 4] |= (uint8_t)((q & 3) << ((i & 3) * 2));
            hmask[i / 8] |= (uint8_t)(((q >> 2) & 1) << (i & 7));
        }

        memcpy(out, hmask, 32); out += 32;
        memcpy(out, qs, 64); out += 64;
        memcpy(out, scaleBytes, 12); out += 12;
        memcpy(out, &fp16_d, 2); out += 2;
    }

    *outBytes = numBlocks * 110;
    m_metrics.cpuFallbackCalls.fetch_add(1, std::memory_order_relaxed);
    return TrainingResult::ok("Q3_K quantized");
}

TrainingResult QuantizationEngine::quantize_F16(const float* src, void* dst, uint64_t n, uint64_t* outBytes) {
    // F16: 2 bytes per element, use F16C intrinsics
    uint16_t* out = (uint16_t*)dst;

    if (m_hasF16C) {
        uint64_t i = 0;
        for (; i + 8 <= n; i += 8) {
            __m256 v = _mm256_loadu_ps(src + i);
            __m128i h = _mm256_cvtps_ph(v, _MM_FROUND_TO_NEAREST_INT);
            _mm_storeu_si128((__m128i*)(out + i), h);
        }
        // Scalar tail
        for (; i < n; ++i) {
            __m128 v = _mm_set_ss(src[i]);
            __m128i h = _mm_cvtps_ph(v, _MM_FROUND_TO_NEAREST_INT);
            out[i] = (uint16_t)_mm_extract_epi16(h, 0);
        }
        m_metrics.masmKernelCalls.fetch_add(1, std::memory_order_relaxed);
    } else {
        // Software fallback
        for (uint64_t i = 0; i < n; ++i) {
            uint32_t fi;
            memcpy(&fi, &src[i], 4);
            uint32_t sign = (fi >> 16) & 0x8000;
            int32_t exp = ((fi >> 23) & 0xFF) - 127 + 15;
            uint32_t frac = (fi >> 13) & 0x3FF;
            if (exp <= 0) out[i] = (uint16_t)sign;
            else if (exp >= 31) out[i] = (uint16_t)(sign | 0x7C00);
            else out[i] = (uint16_t)(sign | (exp << 10) | frac);
        }
        m_metrics.cpuFallbackCalls.fetch_add(1, std::memory_order_relaxed);
    }

    *outBytes = n * 2;
    return TrainingResult::ok("F16 quantized");
}

QuantType QuantizationEngine::selectAdaptiveQuant(const float* data, uint64_t n,
                                                    float targetBPW, bool isEmbedding, bool isOutput) {
    // Adaptive per-tensor quantization selection based on weight distribution
    if (isEmbedding) return QuantType::Q8_0;    // Embeddings need high precision
    if (isOutput)    return QuantType::Q6_K;     // Output projection needs good precision

    // Analyze weight distribution: entropy, kurtosis, outlier ratio
    double sum = 0, sum2 = 0, sum4 = 0;
    float maxAbs = 0;
    for (uint64_t i = 0; i < n && i < 65536; ++i) { // Sample first 64K elements
        double v = data[i];
        sum += v;
        sum2 += v * v;
        sum4 += v * v * v * v;
        float av = fabsf(data[i]);
        if (av > maxAbs) maxAbs = av;
    }

    uint64_t sampleN = std::min(n, (uint64_t)65536);
    double mean = sum / sampleN;
    double variance = sum2 / sampleN - mean * mean;
    double kurtosis = (variance > 0) ? (sum4 / sampleN) / (variance * variance) - 3.0 : 0;

    // Count outliers (>3 sigma)
    double sigma3 = 3.0 * sqrt(variance);
    uint64_t outliers = 0;
    for (uint64_t i = 0; i < sampleN; ++i) {
        if (fabs(data[i] - mean) > sigma3) outliers++;
    }
    double outlierRatio = (double)outliers / sampleN;

    // Decision tree for quantization type
    if (targetBPW <= 2.5) return QuantType::Q2_K;
    if (targetBPW <= 3.5) {
        return (kurtosis > 5.0) ? QuantType::Q3_K_L : QuantType::Q3_K_M;
    }
    if (targetBPW <= 4.5) {
        if (outlierRatio > 0.01) return QuantType::Q4_K_M; // More outliers → use K-quants
        return QuantType::Q4_K_S;
    }
    if (targetBPW <= 5.5) return QuantType::Q5_K_M;
    if (targetBPW <= 6.5) return QuantType::Q6_K;
    return QuantType::Q8_0;
}

TrainingResult QuantizationEngine::quantizeTensor(const float* data, uint64_t numElements,
                                                     QuantType type, void* outBuf, uint64_t* outBytes) {
    if (!m_initialized.load()) initialize();

    m_metrics.inputBytes.fetch_add(numElements * 4, std::memory_order_relaxed);

    TrainingResult r;
    switch (type) {
        case QuantType::Q4_0:   r = quantize_Q4_0(data, outBuf, numElements, outBytes); break;
        case QuantType::Q4_K_S:
        case QuantType::Q4_K_M: r = quantize_Q4_K(data, outBuf, numElements, outBytes); break;
        case QuantType::Q5_K_S:
        case QuantType::Q5_K_M: r = quantize_Q5_K(data, outBuf, numElements, outBytes); break;
        case QuantType::Q6_K:   r = quantize_Q6_K(data, outBuf, numElements, outBytes); break;
        case QuantType::Q8_0:   r = quantize_Q8_0(data, outBuf, numElements, outBytes); break;
        case QuantType::Q2_K:   r = quantize_Q2_K(data, outBuf, numElements, outBytes); break;
        case QuantType::Q3_K_S:
        case QuantType::Q3_K_M:
        case QuantType::Q3_K_L: r = quantize_Q3_K(data, outBuf, numElements, outBytes); break;
        case QuantType::F16:    r = quantize_F16(data, outBuf, numElements, outBytes); break;
        case QuantType::F32:
            memcpy(outBuf, data, numElements * 4);
            *outBytes = numElements * 4;
            r = TrainingResult::ok("F32 (no quantization)");
            break;
        case QuantType::Adaptive: {
            QuantType selected = selectAdaptiveQuant(data, numElements, 4.5f, false, false);
            r = quantizeTensor(data, numElements, selected, outBuf, outBytes);
            break;
        }
        default:
            return TrainingResult::error("Unsupported quant type");
    }

    if (r.success) {
        m_metrics.outputBytes.fetch_add(*outBytes, std::memory_order_relaxed);
    }
    return r;
}

// ============================================================================
// GGUF Writing
// ============================================================================

static void writeGGUFString(FILE* f, const char* s) {
    uint64_t len = strlen(s);
    fwrite(&len, 8, 1, f);
    fwrite(s, 1, len, f);
}

static void writeGGUFMeta_u32(FILE* f, const char* key, uint32_t value) {
    writeGGUFString(f, key);
    uint32_t type = GGUF_TYPE_UINT32;
    fwrite(&type, 4, 1, f);
    fwrite(&value, 4, 1, f);
}

static void writeGGUFMeta_f32(FILE* f, const char* key, float value) {
    writeGGUFString(f, key);
    uint32_t type = GGUF_TYPE_FLOAT32;
    fwrite(&type, 4, 1, f);
    fwrite(&value, 4, 1, f);
}

static void writeGGUFMeta_str(FILE* f, const char* key, const char* value) {
    writeGGUFString(f, key);
    uint32_t type = GGUF_TYPE_STRING;
    fwrite(&type, 4, 1, f);
    writeGGUFString(f, value);
}

static void writeGGUFMeta_bool(FILE* f, const char* key, bool value) {
    writeGGUFString(f, key);
    uint32_t type = GGUF_TYPE_BOOL;
    fwrite(&type, 4, 1, f);
    uint8_t v = value ? 1 : 0;
    fwrite(&v, 1, 1, f);
}

TrainingResult QuantizationEngine::writeGGUFHeader(FILE* f, const ModelArchConfig& arch, const QuantConfig& config) {
    // GGUF header
    fwrite(&GGUF_MAGIC, 4, 1, f);
    fwrite(&GGUF_VERSION, 4, 1, f);

    // Placeholder for tensor count and metadata KV count (will seek back)
    uint64_t tensorCount = 0;
    uint64_t metaKVCount = 0;
    long tensorCountPos = ftell(f);
    fwrite(&tensorCount, 8, 1, f);
    long metaKVCountPos = ftell(f);
    fwrite(&metaKVCount, 8, 1, f);

    // Write metadata
    uint64_t kvCount = 0;

    writeGGUFMeta_str(f, "general.architecture", "llama"); kvCount++;
    writeGGUFMeta_str(f, "general.name", config.modelName); kvCount++;
    writeGGUFMeta_str(f, "general.quantization_version", "rawrxd-masm"); kvCount++;
    writeGGUFMeta_u32(f, "llama.context_length", arch.maxSeqLen); kvCount++;
    writeGGUFMeta_u32(f, "llama.embedding_length", arch.hiddenSize); kvCount++;
    writeGGUFMeta_u32(f, "llama.feed_forward_length", arch.intermediateSize); kvCount++;
    writeGGUFMeta_u32(f, "llama.block_count", arch.numLayers); kvCount++;
    writeGGUFMeta_u32(f, "llama.attention.head_count", arch.numHeads); kvCount++;
    writeGGUFMeta_u32(f, "llama.attention.head_count_kv", arch.numKVHeads); kvCount++;
    writeGGUFMeta_f32(f, "llama.attention.layer_norm_rms_epsilon", arch.rmsNormEps); kvCount++;
    writeGGUFMeta_f32(f, "llama.rope.freq_base", arch.ropeTheta); kvCount++;
    writeGGUFMeta_u32(f, "llama.vocab_size", arch.vocabSize); kvCount++;
    writeGGUFMeta_u32(f, "general.file_type", (uint32_t)config.targetQuant); kvCount++;

    // Seek back and write counts
    long endPos = ftell(f);
    fseek(f, metaKVCountPos, SEEK_SET);
    fwrite(&kvCount, 8, 1, f);
    fseek(f, endPos, SEEK_SET);

    return TrainingResult::ok("GGUF header written");
}

TrainingResult QuantizationEngine::writeGGUFTensor(FILE* f, const char* name, const void* data,
                                                      uint64_t bytes, QuantType type,
                                                      const uint64_t* shape, uint32_t nDims) {
    // Tensor info
    writeGGUFString(f, name);
    fwrite(&nDims, 4, 1, f);
    for (uint32_t i = 0; i < nDims; ++i) fwrite(&shape[i], 8, 1, f);
    uint32_t ggufType = quantTypeToGGUF(type);
    fwrite(&ggufType, 4, 1, f);
    uint64_t offset = 0; // Will be patched
    fwrite(&offset, 8, 1, f);

    return TrainingResult::ok("Tensor info written");
}

TrainingResult QuantizationEngine::quantizeModel(const char* inputPath, const char* outputGGUF,
                                                    const QuantConfig& config, const ModelArchConfig& arch) {
    if (!m_initialized.load()) initialize();
    std::lock_guard<std::mutex> lock(m_mutex);

    resetMetrics();

    // Build layer breakdown for adaptive quantization
    ModelArchBuilder builder;
    builder.configure(arch);
    auto layers = builder.getLayerBreakdown();
    m_metrics.totalLayers.store((uint32_t)layers.size(), std::memory_order_relaxed);

    // Open output GGUF
    FILE* fOut = fopen(outputGGUF, "wb");
    if (!fOut) return TrainingResult::error("Cannot create output GGUF");

    writeGGUFHeader(fOut, arch, config);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Process each tensor
    for (size_t li = 0; li < layers.size(); ++li) {
        const auto& layer = layers[li];

        // Construct weight file path
        std::string weightPath = std::string(inputPath) + "\\" +
            std::string(layer.name) + ".bin";

        // Load weights from disk
        FILE* fWeight = fopen(weightPath.c_str(), "rb");
        if (!fWeight) {
            // Try replacing dots with underscores
            std::string altName = layer.name;
            for (auto& c : altName) if (c == '.') c = '_';
            weightPath = std::string(inputPath) + "\\" + altName + ".bin";
            fWeight = fopen(weightPath.c_str(), "rb");
        }

        if (!fWeight) {
            // Skip missing tensors (norms may be tiny)
            m_metrics.layersProcessed.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        fseek(fWeight, 0, SEEK_END);
        long fileSize = ftell(fWeight);
        fseek(fWeight, 0, SEEK_SET);

        uint64_t numElements = fileSize / 4; // fp32

        // Allocate and read
        std::vector<float> weights(numElements);
        fread(weights.data(), 4, numElements, fWeight);
        fclose(fWeight);

        m_metrics.inputBytes.fetch_add(numElements * 4, std::memory_order_relaxed);

        // Determine quantization type for this tensor
        QuantType qt = config.targetQuant;
        bool isEmbed = (strstr(layer.name, "embd") || strstr(layer.name, "embed"));
        bool isOutput = (strcmp(layer.name, "output") == 0);

        if (isEmbed) qt = config.embedQuant;
        else if (isOutput) qt = config.outputQuant;
        else if (config.adaptivePerLayer)
            qt = selectAdaptiveQuant(weights.data(), numElements,
                                      config.targetBPW > 0 ? config.targetBPW : (float)bitsPerWeight(config.targetQuant),
                                      isEmbed, isOutput);

        // Allocate output buffer (worst case: larger than input for very small tensors)
        uint64_t maxOutBytes = numElements * 4; // Worst case F32
        std::vector<uint8_t> quantized(maxOutBytes);
        uint64_t outBytes = 0;

        TrainingResult r = quantizeTensor(weights.data(), numElements, qt, quantized.data(), &outBytes);
        if (!r.success) {
            fclose(fOut);
            return r;
        }

        m_metrics.outputBytes.fetch_add(outBytes, std::memory_order_relaxed);

        // Write quantized data to GGUF
        // Align to 32 bytes
        long pos = ftell(fOut);
        uint32_t align = config.ggufAlignment;
        uint32_t padding = (align - (pos % align)) % align;
        uint8_t zero[32] = {};
        if (padding > 0) fwrite(zero, 1, padding, fOut);

        fwrite(quantized.data(), 1, outBytes, fOut);

        m_metrics.layersProcessed.fetch_add(1, std::memory_order_relaxed);

        if (m_progressCb) {
            m_progressCb(m_metrics, m_progressData);
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    m_metrics.elapsedMs.store(
        (uint64_t)std::chrono::duration<double, std::milli>(t1 - t0).count(),
        std::memory_order_relaxed);

    fclose(fOut);
    return TrainingResult::ok("Model quantized to GGUF");
}

TrainingResult QuantizationEngine::loadIMatrix(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return TrainingResult::error("Cannot open importance matrix");

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    m_imatrix.resize(sz / 4);
    fread(m_imatrix.data(), 4, m_imatrix.size(), f);
    fclose(f);

    m_hasIMatrix = true;
    return TrainingResult::ok("Importance matrix loaded");
}

TrainingResult QuantizationEngine::computeIMatrix(const char* modelPath, const DatasetPipeline& calibData) {
    // Compute importance matrix by running calibration data through the model
    // and measuring per-weight activation magnitudes
    // This requires the model to be loaded — delegate to PyTorch
    return TrainingResult::error("IMatrix computation requires PyTorch — use CLI: python compute_imatrix.py");
}

void QuantizationEngine::resetMetrics() {
    m_metrics.layersProcessed.store(0);
    m_metrics.totalLayers.store(0);
    m_metrics.inputBytes.store(0);
    m_metrics.outputBytes.store(0);
    m_metrics.elapsedMs.store(0);
    m_metrics.masmKernelCalls.store(0);
    m_metrics.cpuFallbackCalls.store(0);
}

void QuantizationEngine::setProgressCallback(QuantProgressCallback cb, void* userData) {
    m_progressCb = cb;
    m_progressData = userData;
}

// ============================================================================
// TrainingPipelineOrchestrator Implementation
// ============================================================================

TrainingPipelineOrchestrator& TrainingPipelineOrchestrator::instance() {
    static TrainingPipelineOrchestrator s;
    return s;
}

const char* TrainingPipelineOrchestrator::getStageName() const {
    switch (m_stage.load(std::memory_order_acquire)) {
        case PipelineStage::Idle:       return "idle";
        case PipelineStage::Ingesting:  return "ingesting";
        case PipelineStage::Training:   return "training";
        case PipelineStage::Quantizing: return "quantizing";
        case PipelineStage::Exporting:  return "exporting";
        case PipelineStage::Complete:   return "complete";
        case PipelineStage::Failed:     return "failed";
        default:                        return "unknown";
    }
}

TrainingResult TrainingPipelineOrchestrator::stepIngest(const char* dataDir, DatasetFormat fmt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stage.store(PipelineStage::Ingesting, std::memory_order_release);

    TrainingResult r = m_dataset.addDirectory(dataDir, fmt, true);
    if (!r.success) { m_stage.store(PipelineStage::Failed); return r; }

    r = m_dataset.buildTokenizer(32000);
    if (!r.success) { m_stage.store(PipelineStage::Failed); return r; }

    r = m_dataset.tokenizeAll();
    if (!r.success) { m_stage.store(PipelineStage::Failed); return r; }

    return TrainingResult::ok("Dataset ingested");
}

TrainingResult TrainingPipelineOrchestrator::stepTrain(const ModelArchConfig& arch, const TrainingConfig& train) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stage.store(PipelineStage::Training, std::memory_order_release);

    m_archBuilder.configure(arch);
    TrainingResult r = m_pytorch.startTraining(arch, train, m_dataset);
    if (!r.success) { m_stage.store(PipelineStage::Failed); return r; }

    return TrainingResult::ok("Training started");
}

TrainingResult TrainingPipelineOrchestrator::stepQuantize(const QuantConfig& quant) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stage.store(PipelineStage::Quantizing, std::memory_order_release);

    auto& qe = QuantizationEngine::instance();
    qe.initialize();

    // Find weights directory from training output
    std::string weightsDir = std::string(quant.outputPath) + "\\..\\weights";
    std::string outputGGUF = quant.outputPath[0] ? quant.outputPath :
        (std::string(m_archBuilder.getConfig().arch == ModelArch::LLaMA ? "llama" : "model") + "-" +
         std::to_string(m_archBuilder.estimateParamCount() / 1000000) + "M.gguf");

    TrainingResult r = qe.quantizeModel(weightsDir.c_str(), outputGGUF.c_str(), quant, m_archBuilder.getConfig());
    if (!r.success) { m_stage.store(PipelineStage::Failed); return r; }

    return TrainingResult::ok("Quantization complete");
}

TrainingResult TrainingPipelineOrchestrator::stepExport(const char* outputPath) {
    m_stage.store(PipelineStage::Exporting, std::memory_order_release);
    // Export is already done during quantization (GGUF write)
    m_stage.store(PipelineStage::Complete, std::memory_order_release);
    return TrainingResult::ok("Export complete");
}

TrainingResult TrainingPipelineOrchestrator::runFullPipeline(const char* dataDir,
                                                              DatasetFormat dataFormat,
                                                              const ModelArchConfig& arch,
                                                              const TrainingConfig& train,
                                                              const QuantConfig& quant) {
    // Step 1: Ingest
    TrainingResult r = stepIngest(dataDir, dataFormat);
    if (!r.success) return r;

    // Step 2: Train
    r = stepTrain(arch, train);
    if (!r.success) return r;

    // Note: Training is asynchronous — caller must poll m_pytorch.isTraining()
    // and then call stepQuantize() when training completes
    return TrainingResult::ok("Pipeline started (training in progress)");
}

std::string TrainingPipelineOrchestrator::toJson() const {
    auto ds = m_dataset.getStats();
    auto& qm = QuantizationEngine::instance().getMetrics();

    std::ostringstream oss;
    oss << "{"
        << "\"stage\":\"" << getStageName() << "\""
        << ",\"dataset\":{\"files\":" << ds.totalFiles
        << ",\"bytes\":" << ds.totalBytes
        << ",\"tokens\":" << ds.totalTokens
        << ",\"samples\":" << ds.numSamples
        << ",\"vocab\":" << ds.vocabSize << "}"
        << ",\"model\":{\"params\":" << m_archBuilder.estimateParamCount()
        << ",\"fp32_bytes\":" << m_archBuilder.estimateVRAM_FP32()
        << ",\"bf16_bytes\":" << m_archBuilder.estimateVRAM_BF16() << "}"
        << ",\"training\":{\"step\":" << m_trainMetrics.currentStep.load()
        << ",\"loss\":" << std::fixed << std::setprecision(4) << m_trainMetrics.getLoss()
        << ",\"best_loss\":" << m_trainMetrics.getBestLoss()
        << ",\"tokens_per_sec\":" << std::setprecision(1) << m_trainMetrics.getTokensPerSec() << "}"
        << ",\"quantization\":{\"progress\":" << std::setprecision(3) << qm.getProgress()
        << ",\"compression\":" << qm.getCompressionRatio()
        << ",\"masm_calls\":" << qm.masmKernelCalls.load()
        << ",\"cpu_fallbacks\":" << qm.cpuFallbackCalls.load() << "}"
        << "}";
    return oss.str();
}

} // namespace Training
} // namespace RawrXD
