#include "native_gguf_loader.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <windows.h>

namespace GGUFUtils {
    uint32_t GetDataTypeSize(uint32_t type) {
        switch (type) {
            case 0: return 1; // F32
            case 1: return 2; // F16
            case 2: return 4; // Q4_0
            case 3: return 4; // Q4_1
            case 6: return 2; // Q4_2
            case 7: return 2; // Q4_3
            case 8: return 1; // Q8_0
            case 9: return 1; // Q8_1
            case 10: return 4; // Q2_K
            case 11: return 2; // Q3_K
            case 12: return 4; // Q4_K
            case 13: return 2; // Q5_K
            case 14: return 4; // Q6_K
            case 15: return 2; // Q8_K
            case 16: return 4; // I8
            case 17: return 2; // I16
            case 18: return 4; // I32
            case 19: return 1; // I8
            case 20: return 2; // I16
            case 21: return 4; // I32
            case 22: return 1; // F8_E4M3
            case 23: return 2; // F8_E5M2
            default: return 1;
        }
    }
}

NativeGGUFLoader::NativeGGUFLoader()
    : fileHandle(INVALID_HANDLE_VALUE)
    , mappingHandle(nullptr)
    , mappedBase(nullptr)
    , mappedSize(0)
    , version(0)
    , metadataCount(0)
    , tensorCount(0) {}

NativeGGUFLoader::~NativeGGUFLoader() {
    Close();
}

bool NativeGGUFLoader::OpenMemoryMap(const std::string& filePath) {
    fileHandle = static_cast<void*>(CreateFileA(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    ));

    if (fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(static_cast<HANDLE>(fileHandle), &fileSize) || fileSize.QuadPart <= 0) {
        CloseMemoryMap();
        return false;
    }

    mappingHandle = static_cast<void*>(CreateFileMappingA(static_cast<HANDLE>(fileHandle), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (!mappingHandle) {
        CloseMemoryMap();
        return false;
    }

    mappedBase = static_cast<const uint8_t*>(MapViewOfFile(static_cast<HANDLE>(mappingHandle), FILE_MAP_READ, 0, 0, 0));
    if (!mappedBase) {
        CloseMemoryMap();
        return false;
    }

    mappedSize = static_cast<uint64_t>(fileSize.QuadPart);
    return true;
}

void NativeGGUFLoader::CloseMemoryMap() {
    if (mappedBase) {
        UnmapViewOfFile(mappedBase);
        mappedBase = nullptr;
    }
    if (mappingHandle) {
        CloseHandle(static_cast<HANDLE>(mappingHandle));
        mappingHandle = nullptr;
    }
    if (fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(fileHandle));
        fileHandle = INVALID_HANDLE_VALUE;
    }
    mappedSize = 0;
}

bool NativeGGUFLoader::Open(const std::string& filePath) {
    Close();

    if (!OpenMemoryMap(filePath)) {
        return false;
    }

    file.open(filePath, std::ios::binary);
    if (!file.is_open()) {
        CloseMemoryMap();
        return false;
    }

    metadata.clear();
    tensors.clear();
    version = 0;
    metadataCount = 0;
    tensorCount = 0;
    return true;
}

void NativeGGUFLoader::Close() {
    if (file.is_open()) {
        file.close();
    }
    CloseMemoryMap();
    metadata.clear();
    tensors.clear();
    version = 0;
    metadataCount = 0;
    tensorCount = 0;
}

bool NativeGGUFLoader::ParseHeader() {
    if (!file) return false;

    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "GGUF", 4) != 0) return false;

    file.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&tensorCount), sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(&metadataCount), sizeof(uint64_t));

    return true;
}

bool NativeGGUFLoader::ParseMetadata() {
    if (!file) return false;

    metadata.resize(metadataCount);
    for (auto& meta : metadata) {
        uint64_t keyLen;
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(uint64_t));
        meta.key.resize(keyLen);
        file.read(&meta.key[0], keyLen);

        uint32_t type;
        file.read(reinterpret_cast<char*>(&type), sizeof(uint32_t));

        switch (type) {
            case 0: { // String
                uint64_t len;
                file.read(reinterpret_cast<char*>(&len), sizeof(uint64_t));
                std::string val(len, '\0');
                file.read(&val[0], len);
                meta.value = val;
                break;
            }
            case 1: { // Int64
                int64_t val;
                file.read(reinterpret_cast<char*>(&val), sizeof(int64_t));
                meta.value = val;
                break;
            }
            case 2: { // Float32
                float val;
                file.read(reinterpret_cast<char*>(&val), sizeof(float));
                meta.value = val;
                break;
            }
            case 3: { // Bool
                bool val;
                file.read(reinterpret_cast<char*>(&val), sizeof(bool));
                meta.value = val;
                break;
            }
            case 4: { // Array
                uint32_t arrType;
                uint64_t arrLen;
                file.read(reinterpret_cast<char*>(&arrType), sizeof(uint32_t));
                file.read(reinterpret_cast<char*>(&arrLen), sizeof(uint64_t));
                std::vector<uint8_t> arr(arrLen);
                file.read(reinterpret_cast<char*>(&arr[0]), arrLen);
                meta.value = arr;
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

bool NativeGGUFLoader::ParseTensorInfo() {
    if (!file) return false;

    tensors.resize(tensorCount);
    for (auto& tensor : tensors) {
        uint64_t nameLen;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(uint64_t));
        tensor.name.resize(nameLen);
        file.read(&tensor.name[0], nameLen);

        file.read(reinterpret_cast<char*>(&tensor.dimensions), sizeof(uint32_t));
        tensor.shape.resize(tensor.dimensions);
        for (auto& dim : tensor.shape) {
            file.read(reinterpret_cast<char*>(&dim), sizeof(uint64_t));
        }

        file.read(reinterpret_cast<char*>(&tensor.type), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&tensor.offset), sizeof(uint64_t));
    }
    return true;
}

bool NativeGGUFLoader::LoadTensorData(const std::string& tensorName, std::vector<uint8_t>& data) {
    uint64_t size = 0;
    const uint8_t* ptr = GetTensorDataPointer(tensorName, &size);
    if (!ptr || size == 0) return false;

    data.assign(ptr, ptr + size);
    return true;
}

const uint8_t* NativeGGUFLoader::GetTensorDataPointer(const std::string& tensorName, uint64_t* sizeBytes) const {
    if (!mappedBase || mappedSize == 0) return nullptr;

    auto it = std::find_if(tensors.begin(), tensors.end(), [&](const NativeGGUFTensorInfo& t) {
        return t.name == tensorName;
    });
    if (it == tensors.end()) return nullptr;

    uint64_t size = 1;
    for (auto dim : it->shape) {
        size *= dim;
    }
    size *= GGUFUtils::GetDataTypeSize(it->type);

    if (it->offset >= mappedSize) return nullptr;
    if (size > mappedSize - it->offset) return nullptr;

    if (sizeBytes) {
        *sizeBytes = size;
    }
    return mappedBase + it->offset;
}

bool NativeGGUFLoader::IsMemoryMapped() const {
    return mappedBase != nullptr && mappedSize > 0;
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