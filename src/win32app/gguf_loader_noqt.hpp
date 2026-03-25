/**
 * gguf_loader_noqt.hpp
 * Pure C++ GGUF file loader without Qt dependencies
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <fstream>
#include <stdexcept>

class GGUFLoader {
public:
    explicit GGUFLoader(const std::string& path);
    ~GGUFLoader();
    
    bool isOpen() const { return m_file.is_open(); }
    
    // Metadata access
    std::vector<std::string> getTensorNames() const;
    int getParam(const std::string& key, int defaultValue = 0) const;
    std::string getParamString(const std::string& key, const std::string& defaultValue = "") const;
    
    // Tensor access
    std::vector<uint8_t> getTensor(const std::string& name) const;
    int getTensorType(const std::string& name) const;
    std::pair<int, int> getTensorShape(const std::string& name) const;
    
    // Statistics
    size_t getTensorCount() const { return m_tensorMeta.size(); }
    int64_t getFileSize() const { return m_fileSize; }
    
private:
    struct TensorMetadata {
        std::string name;
        int ggmlType;
        std::vector<int64_t> shape;
        int64_t offset;
        int64_t size;
    };
    
    struct Header {
        uint32_t magic;
        uint32_t version;
        uint64_t tensorCount;
        uint64_t metadataKV;
    };
    
    mutable std::ifstream m_file;
    std::string m_path;
    int64_t m_fileSize;
    Header m_header;
    std::map<std::string, int> m_metadata;
    std::map<std::string, std::string> m_metadataStrings;
    std::vector<TensorMetadata> m_tensorMeta;
    std::map<std::string, TensorMetadata> m_tensorMap;
    
    bool readHeader();
    bool readMetadata();
    bool readTensorMetadata();
    void parseMetadataKV();
};
