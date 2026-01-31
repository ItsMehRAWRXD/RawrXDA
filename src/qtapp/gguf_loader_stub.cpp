#include "gguf_loader.hpp"


#include <cstring>

GGUFLoader::GGUFLoader(const std::string& path)
    : m_path(path), m_initialized(false)
{
    try {
        initializeNativeLoader();
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

GGUFLoader::~GGUFLoader() = default;

void GGUFLoader::initializeNativeLoader()
{
    if (m_path.isEmpty()) {
        throw std::runtime_error("Model path is empty");
    }
    
    std::fstream file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open GGUF file");
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read and verify magic bytes "GGUF"
    uint32_t magic;
    stream >> magic;
    if (magic != 0x46554747) {  // "GGUF"
        throw std::runtime_error("Invalid GGUF magic bytes");
    }
    
    // Read version
    uint32_t version;
    stream >> version;
    
    // Read tensor count
    uint64_t tensorCount;
    stream >> tensorCount;
    
    // Read metadata KV count
    uint64_t kvCount;
    stream >> kvCount;
    
    
    // For now, just count the tensors - full parsing would be complex
    // The real loader in src/gguf_loader.cpp handles the full parsing
    m_cachedTensorNames.clear();
    for (uint64_t i = 0; i < tensorCount && i < 1000; i++) {
        m_cachedTensorNames.append(std::string("tensor_%1"));
    }
    
    m_initialized = true;
    file.close();
}

bool GGUFLoader::isOpen() const
{
    return m_initialized;
}

std::any GGUFLoader::getParam(const std::string& key, const std::any& defaultValue) const
{
    // Check cache first
    if (m_metadataCache.contains(key)) {
        return m_metadataCache.value(key);
    }
    return defaultValue;
}

std::vector<uint8_t> GGUFLoader::inflateWeight(const std::string& tensorName)
{
    // Return empty for now - real implementation would load tensor data
    // This is a placeholder that at least allows the app not to crash
    return std::vector<uint8_t>();
}

std::unordered_map<std::string, std::vector<uint8_t>> GGUFLoader::getTokenizerMetadata() const
{
    std::unordered_map<std::string, std::vector<uint8_t>> meta;
    // Return empty - tokenizer metadata loading is complex
    return meta;
}

std::vector<std::string> GGUFLoader::tensorNames() const
{
    return m_cachedTensorNames;
}

