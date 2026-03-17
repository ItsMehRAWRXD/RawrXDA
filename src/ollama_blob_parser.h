#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>

/**
 * @file ollama_blob_parser.h
 * @brief Ollama blob ingestion without Ollama runtime dependency
 * 
 * Extracts GGUF-compatible tensor data from Ollama blobs
 * - No Ollama daemon required
 * - No Ollama CLI needed
 * - Direct blob parsing
 * - Compatible with GGUF streaming loader
 */

namespace rawrxd {
namespace ollama {

//=============================================================================
// OLLAMA BLOB STRUCTURE
//=============================================================================

/**
 * Ollama blob format:
 * [ Optional Ollama Header ]
 * [ GGUF-compatible tensor blocks ]
 * [ Optional tokenizer/vocab section ]
 */

struct OllamaManifest {
    std::string model_format;      // "gguf", "safetensors", etc.
    std::string model_family;      // "llama", "mistral", etc.
    std::string model_type;        // "completion", "chat", etc.
    std::vector<std::string> layers;
    size_t total_size_bytes;
};

struct BlobInfo {
    std::string blob_id;           // SHA256 hash
    std::string blob_path;
    size_t file_size_bytes;
    bool is_model_blob;
    bool contains_gguf;
    size_t gguf_offset;            // Offset to GGUF data if present
};

//=============================================================================
// OLLAMA BLOB DETECTOR
//=============================================================================

/**
 * @class OllamaBlobDetector
 * @brief Detects if a file is an Ollama blob and locates GGUF data
 */
class OllamaBlobDetector {
public:
    OllamaBlobDetector();
    ~OllamaBlobDetector();
    
    // Check if file is Ollama blob (by path or content)
    static bool isOllamaBlob(const std::string& file_path);
    
    // Find GGUF magic number in blob
    static size_t findGGUFOffset(const std::string& file_path);
    
    // Extract blob info
    static BlobInfo analyzeBlobFile(const std::string& file_path);
    
    // Check if blob contains model weights
    static bool isModelBlob(const std::string& file_path);
    
    // Get Ollama blobs directory
    static std::string getOllamaBlobsDirectory();
    
private:
    static constexpr uint32_t GGUF_MAGIC = 0x46554747;  // "GGUF"
    static constexpr size_t SEARCH_BUFFER_SIZE = 4096;
};

//=============================================================================
// OLLAMA MANIFEST PARSER
//=============================================================================

/**
 * @class OllamaManifestParser
 * @brief Parses Ollama manifest files to locate model blobs
 */
class OllamaManifestParser {
public:
    OllamaManifestParser();
    ~OllamaManifestParser();
    
    // Parse manifest from file
    bool parseManifest(const std::string& manifest_path);
    
    // Parse manifest from JSON string
    bool parseManifestJSON(const std::string& json_content);
    
    // Get manifest data
    OllamaManifest getManifest() const { return manifest_; }
    
    // Get model blob references
    std::vector<std::string> getModelBlobRefs() const;
    
    // Resolve blob reference to file path
    std::string resolveBlobPath(const std::string& blob_ref) const;
    
private:
    OllamaManifest manifest_;
    std::string blobs_directory_;
};

//=============================================================================
// OLLAMA BLOB PARSER
//=============================================================================

/**
 * @class OllamaBlobParser
 * @brief Main parser for extracting GGUF data from Ollama blobs
 */
class OllamaBlobParser {
public:
    struct ParseResult {
        bool success;
        std::string error_message;
        std::string gguf_model_path;  // Path to extracted/accessible GGUF
        size_t gguf_offset;           // Offset if GGUF embedded in blob
        size_t gguf_size;             // Size of GGUF data
        bool requires_extraction;     // True if offset != 0
    };
    
    OllamaBlobParser();
    ~OllamaBlobParser();
    
    // Parse Ollama blob and prepare for GGUF loading
    ParseResult parseBlobToGGUF(const std::string& blob_path);
    
    // Extract GGUF data to temporary file (if embedded at offset)
    bool extractGGUFToFile(
        const std::string& blob_path,
        size_t offset,
        size_t size,
        const std::string& output_path
    );
    
    // Create memory-mapped view of GGUF data in blob
    void* mapGGUFData(
        const std::string& blob_path,
        size_t offset,
        size_t size
    );
    
    // Release mapped GGUF data
    void unmapGGUFData(void* mapped_address);
    
    // Get GGUF reader compatible path
    // Returns either:
    // - Original blob path (if GGUF at offset 0)
    // - Extracted temporary file path (if embedded)
    std::string getGGUFReaderPath(const std::string& blob_path);
    
private:
    std::string temp_directory_;
    std::vector<std::string> temp_files_;  // Track temp files for cleanup
    
    void cleanupTempFiles();
    std::string generateTempPath(const std::string& blob_id);
};

//=============================================================================
// OLLAMA MODEL LOCATOR
//=============================================================================

/**
 * @class OllamaModelLocator
 * @brief Locates Ollama models in standard directories
 */
class OllamaModelLocator {
public:
    struct ModelLocation {
        std::string model_name;
        std::string manifest_path;
        std::vector<std::string> blob_paths;
        size_t total_size_bytes;
        std::string model_family;
    };
    
    OllamaModelLocator();
    ~OllamaModelLocator();
    
    // Find all Ollama models on system
    std::vector<ModelLocation> findAllModels();
    
    // Find specific model by name
    ModelLocation findModel(const std::string& model_name);
    
    // Get standard Ollama directories
    static std::vector<std::string> getOllamaDirectories();
    
    // Check if Ollama is installed
    static bool isOllamaInstalled();
    
private:
    std::vector<std::string> search_directories_;
    
    void scanDirectory(
        const std::string& directory,
        std::vector<ModelLocation>& results
    );
};

//=============================================================================
// OLLAMA BLOB STREAM ADAPTER
//=============================================================================

/**
 * @class OllamaBlobStreamAdapter
 * @brief Adapts Ollama blob to work with GGUF streaming loader
 */
class OllamaBlobStreamAdapter {
public:
    OllamaBlobStreamAdapter(const std::string& blob_path);
    ~OllamaBlobStreamAdapter();
    
    // Open blob for streaming
    bool open();
    
    // Close blob
    void close();
    
    // Seek to position (relative to GGUF data start)
    bool seek(size_t position);
    
    // Read data
    size_t read(void* buffer, size_t size);
    
    // Get GGUF data size
    size_t getGGUFSize() const { return gguf_size_; }
    
    // Get current position
    size_t tell() const;
    
    // Check if at end
    bool eof() const;
    
private:
    std::string blob_path_;
    std::ifstream file_stream_;
    size_t gguf_offset_;
    size_t gguf_size_;
    size_t current_position_;
    bool is_open_;
};

//=============================================================================
// INTEGRATION HELPERS
//=============================================================================

/**
 * @brief Helper functions for integrating Ollama blob support
 */
namespace integration {

// Check if path is Ollama blob or regular GGUF
bool isOllamaBlobPath(const std::string& path);

// Get GGUF-compatible path from any model path (blob or GGUF)
std::string getGGUFCompatiblePath(const std::string& model_path);

// Load model from Ollama blob using existing GGUF loader
template<typename GGUFLoaderType>
bool loadModelFromOllamaBlob(
    GGUFLoaderType& loader,
    const std::string& blob_path
) {
    OllamaBlobParser parser;
    auto result = parser.parseBlobToGGUF(blob_path);
    
    if (!result.success) {
        return false;
    }
    
    return loader.loadModel(result.gguf_model_path);
}

} // namespace integration

} // namespace ollama
} // namespace rawrxd
