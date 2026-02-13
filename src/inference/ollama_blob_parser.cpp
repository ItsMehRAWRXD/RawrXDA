#include "ollama_blob_parser.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace rawrxd {
namespace ollama {

// ============================================================================
// OllamaBlobDetector Implementation
// ============================================================================

class OllamaBlobDetector::Impl {
public:
    static const uint32_t GGUF_MAGIC = 0x46554747;
};

OllamaBlobDetector::OllamaBlobDetector() : pimpl(std::make_unique<Impl>()) {}

OllamaBlobDetector::~OllamaBlobDetector() = default;

bool OllamaBlobDetector::IsGGUFFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    return magic == Impl::GGUF_MAGIC;
}

bool OllamaBlobDetector::ContainsGGUF(const std::string& blob_path, uint64_t& offset) {
    std::ifstream file(blob_path, std::ios::binary);
    if (!file) return false;
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Search for GGUF magic in chunks
    const size_t CHUNK_SIZE = 1024 * 1024;  // 1MB chunks
    std::vector<char> buffer(CHUNK_SIZE);
    uint64_t position = 0;
    
    while (file) {
        file.read(buffer.data(), CHUNK_SIZE);
        size_t bytes_read = file.gcount();
        
        if (bytes_read == 0) break;
        
        // Search for magic in this chunk
        for (size_t i = 0; i + 4 <= bytes_read; ++i) {
            uint32_t potential_magic = *reinterpret_cast<uint32_t*>(buffer.data() + i);
            if (potential_magic == Impl::GGUF_MAGIC) {
                offset = position + i;
                return true;
            }
        }
        
        position += bytes_read;
        
        // Overlap last 3 bytes in case magic spans chunk boundary
        if (bytes_read >= 4) {
            buffer[0] = buffer[bytes_read - 3];
            buffer[1] = buffer[bytes_read - 2];
            buffer[2] = buffer[bytes_read - 1];
            position -= 3;
            file.seekg(position + 3, std::ios::beg);
        }
    }
    
    return false;
}

std::vector<GGUFBlobInfo> OllamaBlobDetector::DetectAllGGUFBlobs(const std::string& directory) {
    std::vector<GGUFBlobInfo> results;
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                uint64_t offset = 0;
                if (ContainsGGUF(entry.path().string(), offset)) {
                    GGUFBlobInfo info;
                    info.blob_path = entry.path().string();
                    info.gguf_offset = offset;
                    info.blob_size = fs::file_size(entry.path());
                    info.is_pure_gguf = (offset == 0);
                    
                    results.push_back(info);
                }
            }
        }
    } catch (const std::exception& e) {
        // Directory access error - return partial results
    }
    
    return results;
}

// ============================================================================
// OllamaBlobParser Implementation
// ============================================================================

class OllamaBlobParser::Impl {
public:
    struct GGUFHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t tensor_count;
        uint64_t metadata_count;
    };
    
    std::string last_error;
};

OllamaBlobParser::OllamaBlobParser() : pimpl(std::make_unique<Impl>()) {}

OllamaBlobParser::~OllamaBlobParser() = default;

bool OllamaBlobParser::ParseGGUFHeader(
    const std::string& blob_path,
    uint64_t offset,
    GGUFHeader& header
) {
    std::ifstream file(blob_path, std::ios::binary);
    if (!file) {
        pimpl->last_error = "Cannot open blob file";
        return false;
    }
    
    file.seekg(offset, std::ios::beg);
    if (!file) {
        pimpl->last_error = "Cannot seek to GGUF offset";
        return false;
    }
    
    // Read header (24 bytes)
    uint32_t magic = 0, version = 0;
    uint64_t tensor_count = 0, metadata_count = 0;
    
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&tensor_count), sizeof(tensor_count));
    file.read(reinterpret_cast<char*>(&metadata_count), sizeof(metadata_count));
    
    if (magic != 0x46554747) {  // GGUF magic
        pimpl->last_error = "Invalid GGUF magic";
        return false;
    }
    
    header.magic = 0x46554747;
    header.version = version;
    header.tensor_count = tensor_count;
    header.metadata_count = metadata_count;
    
    return true;
}

bool OllamaBlobParser::ExtractGGUFData(
    const std::string& blob_path,
    uint64_t offset,
    const std::string& output_path
) {
    std::ifstream input(blob_path, std::ios::binary);
    if (!input) {
        pimpl->last_error = "Cannot open input blob";
        return false;
    }
    
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        pimpl->last_error = "Cannot create output file";
        return false;
    }
    
    // Copy from offset to end of file
    input.seekg(0, std::ios::end);
    size_t remaining = static_cast<size_t>(input.tellg()) - static_cast<size_t>(offset);
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    
    const size_t BUFFER_SIZE = 1024 * 1024;  // 1MB
    std::vector<char> buffer(BUFFER_SIZE);
    
    while (remaining > 0) {
        size_t to_read = std::min(remaining, BUFFER_SIZE);
        input.read(buffer.data(), to_read);
        output.write(buffer.data(), input.gcount());
        
        remaining -= input.gcount();
        
        if (!input) break;
    }
    
    return true;
}

std::vector<std::string> OllamaBlobParser::ExtractMetadataKeys(
    const std::string& blob_path,
    uint64_t offset
) {
    std::vector<std::string> keys;
    
    std::ifstream file(blob_path, std::ios::binary);
    if (!file) return keys;
    
    file.seekg(offset + 24, std::ios::beg);  // Skip GGUF header
    
    // Read a reasonable number of metadata keys
    for (int i = 0; i < 100; ++i) {
        uint64_t key_len = 0;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        
        if (key_len > 1000 || !file) break;  // Invalid length
        
        std::vector<char> key_data(key_len);
        file.read(key_data.data(), key_len);
        
        if (!file) break;
        
        std::string key(key_data.begin(), key_data.end());
        keys.push_back(key);
        
        // Skip value (we don't parse it, just count it)
        uint32_t value_type = 0;
        file.read(reinterpret_cast<char*>(&value_type), sizeof(value_type));
        
        if (!file) break;
        
        // Skip value data based on type (simplified)
        // For full implementation, would need proper type handling
    }
    
    return keys;
}

// ============================================================================
// OllamaModelLocator Implementation
// ============================================================================

class OllamaModelLocator::Impl {
public:
    std::string last_error;
};

OllamaModelLocator::OllamaModelLocator() : pimpl(std::make_unique<Impl>()) {}

OllamaModelLocator::~OllamaModelLocator() = default;

std::string OllamaModelLocator::FindOllamaModelsDirectory() {
    // Common Ollama installation paths on Windows
    const std::vector<std::string> possible_paths = {
        (fs::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "") / ".ollama" / "models" / "blobs").string(),
        "C:\\Users\\AppData\\Local\\Ollama\\models\\blobs",
        "C:\\ProgramData\\Ollama\\models\\blobs"
    };
    
    for (const auto& path : possible_paths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            return path;
        }
    }
    
    pimpl->last_error = "Ollama models directory not found";
    return "";
}

std::vector<OllamaModel> OllamaModelLocator::FindAllModels() {
    std::vector<OllamaModel> models;
    
    std::string models_dir = FindOllamaModelsDirectory();
    if (models_dir.empty()) return models;
    
    OllamaBlobDetector detector;
    auto blobs = detector.DetectAllGGUFBlobs(models_dir);
    
    for (const auto& blob : blobs) {
        OllamaModel model;
        model.blob_path = blob.blob_path;
        model.gguf_offset = blob.gguf_offset;
        model.size_bytes = blob.blob_size;
        
        // Extract model name from path (sha256-xxx format)
        size_t pos = blob.blob_path.find_last_of("\\/");
        if (pos != std::string::npos) {
            model.name = blob.blob_path.substr(pos + 1);
        }
        
        models.push_back(model);
    }
    
    return models;
}

std::vector<OllamaModel> OllamaModelLocator::FindModelsInDirectory(const std::string& directory) {
    std::vector<OllamaModel> models;
    
    OllamaBlobDetector detector;
    auto blobs = detector.DetectAllGGUFBlobs(directory);
    
    for (const auto& blob : blobs) {
        OllamaModel model;
        model.blob_path = blob.blob_path;
        model.gguf_offset = blob.gguf_offset;
        model.size_bytes = blob.blob_size;
        
        size_t pos = blob.blob_path.find_last_of("\\/");
        if (pos != std::string::npos) {
            model.name = blob.blob_path.substr(pos + 1);
        }
        
        models.push_back(model);
    }
    
    return models;
}

// ============================================================================
// OllamaBlobStreamAdapter Implementation
// ============================================================================

class OllamaBlobStreamAdapter::Impl {
public:
    std::ifstream file;
    uint64_t gguf_offset = 0;
    uint64_t current_position = 0;
};

OllamaBlobStreamAdapter::OllamaBlobStreamAdapter(
    const std::string& blob_path,
    uint64_t offset
) : pimpl(std::make_unique<Impl>()) {
    pimpl->file.open(blob_path, std::ios::binary);
    pimpl->gguf_offset = offset;
    pimpl->file.seekg(offset, std::ios::beg);
}

OllamaBlobStreamAdapter::~OllamaBlobStreamAdapter() {
    if (pimpl->file.is_open()) {
        pimpl->file.close();
    }
}

bool OllamaBlobStreamAdapter::Read(void* buffer, size_t size) {
    if (!pimpl->file) return false;
    
    pimpl->file.read(reinterpret_cast<char*>(buffer), size);
    pimpl->current_position += pimpl->file.gcount();
    
    return pimpl->file.gcount() == size;
}

bool OllamaBlobStreamAdapter::Seek(uint64_t offset) {
    pimpl->file.seekg(pimpl->gguf_offset + offset, std::ios::beg);
    pimpl->current_position = offset;
    return pimpl->file.good();
}

uint64_t OllamaBlobStreamAdapter::Tell() const {
    return pimpl->current_position;
}

bool OllamaBlobStreamAdapter::IsOpen() const {
    return pimpl->file.is_open() && pimpl->file.good();
}

} // namespace ollama
} // namespace rawrxd
