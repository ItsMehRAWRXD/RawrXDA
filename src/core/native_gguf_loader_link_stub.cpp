#include "../../native_gguf_loader.h"

NativeGGUFLoader::NativeGGUFLoader()
    : fileHandle(nullptr),
      mappingHandle(nullptr),
      mappedBase(nullptr),
      mappedSize(0),
      version(0),
      metadataCount(0),
      tensorCount(0) {}

NativeGGUFLoader::~NativeGGUFLoader() {
    Close();
}

bool NativeGGUFLoader::Open(const std::string& filePath) {
    Close();
    file.open(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    mappedSize = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    return true;
}

void NativeGGUFLoader::Close() {
    if (file.is_open()) {
        file.close();
    }
    fileHandle = nullptr;
    mappingHandle = nullptr;
    mappedBase = nullptr;
    mappedSize = 0;
    metadata.clear();
    tensors.clear();
}

bool NativeGGUFLoader::ParseHeader() {
    return file.is_open();
}

bool NativeGGUFLoader::ParseMetadata() {
    if (!file.is_open()) {
        return false;
    }
    metadataCount = metadata.size();
    return true;
}

bool NativeGGUFLoader::ParseTensorInfo() {
    if (!file.is_open()) {
        return false;
    }
    tensorCount = tensors.size();
    return true;
}

bool NativeGGUFLoader::LoadTensorData(const std::string& tensorName, std::vector<uint8_t>& data) {
    (void)tensorName;
    data.clear();
    return file.is_open();
}

const uint8_t* NativeGGUFLoader::GetTensorDataPointer(const std::string& tensorName, uint64_t* sizeBytes) const {
    (void)tensorName;
    if (sizeBytes) {
        *sizeBytes = 0;
    }
    return nullptr;
}

bool NativeGGUFLoader::IsMemoryMapped() const {
    return mappedBase != nullptr;
}

uint64_t NativeGGUFLoader::GetMappedSize() const {
    return mappedSize;
}

const std::vector<NativeGGUFTensorInfo>& NativeGGUFLoader::GetTensors() const {
    return tensors;
}

const std::vector<NativeGGUFMetadata>& NativeGGUFLoader::GetMetadata() const {
    return metadata;
}

bool NativeGGUFLoader::OpenMemoryMap(const std::string& filePath) {
    (void)filePath;
    return false;
}

void NativeGGUFLoader::CloseMemoryMap() {
    mappedBase = nullptr;
    mappedSize = 0;
}
