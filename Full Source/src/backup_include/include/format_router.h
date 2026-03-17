#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <filesystem>
#include <chrono>

enum class ModelFormat {
    GGUF_LOCAL = 0,
    HF_REPO = 1,
    HF_FILE = 2,
    OLLAMA_REMOTE = 3,
    MASM_COMPRESSED = 4,
    UNKNOWN = 5
};

enum class CompressionType {
    NONE = 0,
    GZIP = 1,
    ZSTD = 2,
    LZ4 = 3
};

struct ModelSource {
    std::string path;
    ModelFormat format;
    CompressionType compression;
    std::string display_name;
    std::unordered_map<std::string, std::string> metadata;
};

struct FormatDetectionResult {
    ModelFormat format;
    CompressionType compression;
    std::string reason;
    bool valid;
};

/**
 * @class FormatRouter
 * @brief Routes model inputs to appropriate loaders (GGUF/HF/Ollama/MASM)
 * Detects format via file magic, naming patterns, and validation.
 */
class FormatRouter {
public:
    FormatRouter();
    ~FormatRouter() = default;

    // Main routing interface
    std::optional<ModelSource> route(const std::string& input);
    
    // Validation
    bool validateModelPath(const std::string& path);
    FormatDetectionResult detectFormat(const std::string& input);

    // Getters
    static std::string formatToString(ModelFormat fmt);
    static std::string compressionToString(CompressionType cmp);

private:
    // Format detectors
    FormatDetectionResult detectGGUFLocal(const std::string& path);
    FormatDetectionResult detectHFRepo(const std::string& input);
    FormatDetectionResult detectOllamaRemote(const std::string& input);
    FormatDetectionResult detectMASMCompressed(const std::string& path);

    // Validation helpers
    bool isValidGGUFFile(const std::string& path);
    bool isValidHFRepoFormat(const std::string& input);
    bool isValidOllamaFormat(const std::string& input);
    bool isValidCompressedFile(const std::string& path);
    
    // Compression detection
    CompressionType detectCompressionType(const std::string& path);
    bool hasGzipMagic(const std::string& path);
    bool hasZstdMagic(const std::string& path);
    bool hasLz4Magic(const std::string& path);

    std::unordered_map<std::string, ModelFormat> m_cache;
    std::chrono::steady_clock::time_point m_lastDetection;
};
