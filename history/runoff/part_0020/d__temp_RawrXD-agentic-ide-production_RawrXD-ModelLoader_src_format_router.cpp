#include "format_router.h"
#include <fstream>
#include <regex>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

FormatRouter::FormatRouter() 
    : m_lastDetection(std::chrono::steady_clock::now()) {}

std::optional<ModelSource> FormatRouter::route(const std::string& input) {
    if (input.empty()) {
        std::cerr << "❌ Empty model input" << std::endl;
        return std::nullopt;
    }

    // Check cache first
    auto cache_it = m_cache.find(input);
    if (cache_it != m_cache.end()) {
        ModelSource src;
        src.path = input;
        src.format = cache_it->second;
        src.compression = detectCompressionType(input);
        return src;
    }

    // Detect format
    auto detection = detectFormat(input);
    
    if (!detection.valid || detection.format == ModelFormat::UNKNOWN) {
        std::cerr << "❌ Failed to determine model format: " << detection.reason << std::endl;
        return std::nullopt;
    }

    // Build model source
    ModelSource source;
    source.path = input;
    source.format = detection.format;
    source.compression = detection.compression;

    switch (detection.format) {
        case ModelFormat::GGUF_LOCAL:
            source.display_name = "Local GGUF: " + std::filesystem::path(input).filename().string();
            break;
        case ModelFormat::HF_REPO:
            source.display_name = "HF Repo: " + input;
            break;
        case ModelFormat::HF_FILE:
            source.display_name = "HF File: " + input;
            break;
        case ModelFormat::OLLAMA_REMOTE:
            source.display_name = "Ollama Remote: " + input;
            break;
        case ModelFormat::MASM_COMPRESSED:
            source.display_name = "Compressed GGUF: " + std::filesystem::path(input).filename().string();
            break;
        default:
            source.display_name = "Unknown";
    }

    // Cache the result
    m_cache[input] = detection.format;
    m_lastDetection = std::chrono::steady_clock::now();

    std::cout << "✅ Routed to: " << source.display_name << std::endl;
    return source;
}

FormatDetectionResult FormatRouter::detectFormat(const std::string& input) {
    // Try GGUF local first (most specific)
    auto result = detectGGUFLocal(input);
    if (result.valid && result.format != ModelFormat::UNKNOWN) {
        return result;
    }

    // Try MASM compressed (magic bytes)
    result = detectMASMCompressed(input);
    if (result.valid && result.format != ModelFormat::UNKNOWN) {
        return result;
    }

    // Try Ollama remote (owner/model:tag pattern)
    result = detectOllamaRemote(input);
    if (result.valid && result.format != ModelFormat::UNKNOWN) {
        return result;
    }

    // Try HF repo (owner/model[:revision] pattern)
    result = detectHFRepo(input);
    if (result.valid && result.format != ModelFormat::UNKNOWN) {
        return result;
    }

    return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "No matching format detected", false};
}

FormatDetectionResult FormatRouter::detectGGUFLocal(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "File does not exist", false};
    }

    if (!isValidGGUFFile(path)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Not a valid GGUF file (invalid magic)", false};
    }

    auto compression = detectCompressionType(path);
    if (compression != CompressionType::NONE) {
        return FormatDetectionResult{ModelFormat::MASM_COMPRESSED, compression, "GGUF file is compressed", true};
    }

    return FormatDetectionResult{ModelFormat::GGUF_LOCAL, CompressionType::NONE, "Valid GGUF file detected", true};
}

FormatDetectionResult FormatRouter::detectHFRepo(const std::string& input) {
    if (!isValidHFRepoFormat(input)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Not a valid HF repo format", false};
    }

    // Check if it's owner/model[:revision]
    std::regex hf_pattern(R"(^([\w\-\.]+)/([\w\-\.]+)(?::[\w\-\.]+)?$)");
    if (!std::regex_match(input, hf_pattern)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Invalid HF repo pattern", false};
    }

    return FormatDetectionResult{ModelFormat::HF_REPO, CompressionType::NONE, "Valid HF repo format", true};
}

FormatDetectionResult FormatRouter::detectOllamaRemote(const std::string& input) {
    if (!isValidOllamaFormat(input)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Not a valid Ollama format", false};
    }

    // Check if it's [owner/]model:tag
    std::regex ollama_pattern(R"(^([\w\-]+/)?[\w\-]+:[\w\-]+$)");
    if (!std::regex_match(input, ollama_pattern)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Invalid Ollama model pattern", false};
    }

    return FormatDetectionResult{ModelFormat::OLLAMA_REMOTE, CompressionType::NONE, "Valid Ollama model format", true};
}

FormatDetectionResult FormatRouter::detectMASMCompressed(const std::string& path) {
    if (!isValidCompressedFile(path)) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Not a compressed file", false};
    }

    auto compression = detectCompressionType(path);
    if (compression == CompressionType::NONE) {
        return FormatDetectionResult{ModelFormat::UNKNOWN, CompressionType::NONE, "Unrecognized compression type", false};
    }

    return FormatDetectionResult{ModelFormat::MASM_COMPRESSED, compression, "Compressed model file detected", true};
}

bool FormatRouter::validateModelPath(const std::string& path) {
    auto result = detectFormat(path);
    return result.valid && result.format != ModelFormat::UNKNOWN;
}

bool FormatRouter::isValidGGUFFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    char magic[4];
    file.read(magic, 4);
    file.close();

    // Check GGUF magic: "GGUF"
    return std::string(magic, 4) == "GGUF";
}

bool FormatRouter::isValidHFRepoFormat(const std::string& input) {
    // Basic check: contains '/' and doesn't start with '/'
    if (input.empty() || input[0] == '/' || input.find('/') == std::string::npos) {
        return false;
    }

    // HF repos shouldn't be file paths or remote URLs
    if (input.find("http://") != std::string::npos || input.find("https://") != std::string::npos) {
        return false;
    }

    // Must not contain backslashes (Windows paths)
    if (input.find('\\') != std::string::npos) {
        return false;
    }

    return true;
}

bool FormatRouter::isValidOllamaFormat(const std::string& input) {
    // Ollama models must contain a colon for tag
    if (input.find(':') == std::string::npos) {
        return false;
    }

    // Must not be a file path
    if (std::filesystem::exists(input)) {
        return false;
    }

    // Must not contain http/https
    if (input.find("http://") != std::string::npos || input.find("https://") != std::string::npos) {
        return false;
    }

    return true;
}

bool FormatRouter::isValidCompressedFile(const std::string& path) {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        return false;
    }

    // Check extension
    std::string ext = std::filesystem::path(path).extension().string();
    if (ext == ".gz" || ext == ".zst" || ext == ".lz4") {
        return true;
    }

    // Check magic bytes
    return hasGzipMagic(path) || hasZstdMagic(path) || hasLz4Magic(path);
}

CompressionType FormatRouter::detectCompressionType(const std::string& path) {
    if (hasGzipMagic(path)) {
        return CompressionType::GZIP;
    }
    if (hasZstdMagic(path)) {
        return CompressionType::ZSTD;
    }
    if (hasLz4Magic(path)) {
        return CompressionType::LZ4;
    }
    return CompressionType::NONE;
}

bool FormatRouter::hasGzipMagic(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    unsigned char magic[2];
    file.read(reinterpret_cast<char*>(magic), 2);
    file.close();

    return magic[0] == 0x1f && magic[1] == 0x8b;
}

bool FormatRouter::hasZstdMagic(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    unsigned char magic[4];
    file.read(reinterpret_cast<char*>(magic), 4);
    file.close();

    return magic[0] == 0x28 && magic[1] == 0xb5 && magic[2] == 0x2f && magic[3] == 0xfd;
}

bool FormatRouter::hasLz4Magic(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    unsigned char magic[4];
    file.read(reinterpret_cast<char*>(magic), 4);
    file.close();

    return magic[0] == 0x04 && magic[1] == 0x22 && magic[2] == 0x4d && magic[3] == 0x18;
}

std::string FormatRouter::formatToString(ModelFormat fmt) {
    switch (fmt) {
        case ModelFormat::GGUF_LOCAL: return "GGUF_LOCAL";
        case ModelFormat::HF_REPO: return "HF_REPO";
        case ModelFormat::HF_FILE: return "HF_FILE";
        case ModelFormat::OLLAMA_REMOTE: return "OLLAMA_REMOTE";
        case ModelFormat::MASM_COMPRESSED: return "MASM_COMPRESSED";
        default: return "UNKNOWN";
    }
}

std::string FormatRouter::compressionToString(CompressionType cmp) {
    switch (cmp) {
        case CompressionType::NONE: return "NONE";
        case CompressionType::GZIP: return "GZIP";
        case CompressionType::ZSTD: return "ZSTD";
        case CompressionType::LZ4: return "LZ4";
        default: return "UNKNOWN";
    }
}
