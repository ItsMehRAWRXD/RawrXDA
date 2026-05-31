#include "native_gguf_loader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

NativeGGUFLoader::NativeGGUFLoader()
    : fileHandle(INVALID_HANDLE_VALUE), mappingHandle(nullptr), mappedBase(nullptr), mappedSize(0), version(0),
      metadataCount(0), tensorCount(0)
{
}

NativeGGUFLoader::~NativeGGUFLoader()
{
    Close();
}

bool NativeGGUFLoader::OpenMemoryMap(const std::string& filePath)
{
    fileHandle = static_cast<void*>(CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                                FILE_ATTRIBUTE_NORMAL, nullptr));
    if (fileHandle == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(static_cast<HANDLE>(fileHandle), &fileSize) || fileSize.QuadPart <= 0)
    {
        CloseMemoryMap();
        return false;
    }

    mappingHandle =
        static_cast<void*>(CreateFileMappingA(static_cast<HANDLE>(fileHandle), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (!mappingHandle)
    {
        CloseMemoryMap();
        return false;
    }

    mappedBase = static_cast<const uint8_t*>(MapViewOfFile(static_cast<HANDLE>(mappingHandle), FILE_MAP_READ, 0, 0, 0));
    if (!mappedBase)
    {
        CloseMemoryMap();
        return false;
    }

    mappedSize = static_cast<uint64_t>(fileSize.QuadPart);
    return true;
}

void NativeGGUFLoader::CloseMemoryMap()
{
    if (mappedBase)
    {
        UnmapViewOfFile(mappedBase);
        mappedBase = nullptr;
    }
    if (mappingHandle)
    {
        CloseHandle(static_cast<HANDLE>(mappingHandle));
        mappingHandle = nullptr;
    }
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(static_cast<HANDLE>(fileHandle));
        fileHandle = INVALID_HANDLE_VALUE;
    }
    mappedSize = 0;
}

bool NativeGGUFLoader::Open(const std::string& filePath)
{
    Close();

    if (!OpenMemoryMap(filePath))
        return false;

    file.open(filePath, std::ios::binary);
    if (!file.is_open())
    {
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

void NativeGGUFLoader::Close()
{
    if (file.is_open())
        file.close();
    CloseMemoryMap();
    metadata.clear();
    tensors.clear();
    version = 0;
    metadataCount = 0;
    tensorCount = 0;
}

bool NativeGGUFLoader::ParseHeader()
{
    if (!file)
        return false;

    char magic[4]{};
    file.read(magic, 4);
    if (std::memcmp(magic, "GGUF", 4) != 0)
        return false;

    file.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&tensorCount), sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(&metadataCount), sizeof(uint64_t));

    return (bool)file;
}

bool NativeGGUFLoader::ParseMetadata()
{
    if (!file)
        return false;

    metadata.clear();
    metadata.reserve((size_t)metadataCount);

    for (uint64_t i = 0; i < metadataCount; ++i)
    {
        uint64_t key_len = 0;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (!file || key_len > (1024ull * 1024ull))
            return false;

        std::string key((size_t)key_len, '\0');
        if (key_len)
            file.read(&key[0], (std::streamsize)key_len);
        if (!file)
            return false;

        uint32_t val_type = 0;
        file.read(reinterpret_cast<char*>(&val_type), sizeof(val_type));
        if (!file)
            return false;

        NativeGGUFMetadata m;
        m.key = std::move(key);
        m.value = std::string{};
        metadata.push_back(std::move(m));

        // Skip value payload (subset).
        switch (val_type)
        {
            case 8:  // string
            {
                uint64_t n = 0;
                file.read(reinterpret_cast<char*>(&n), sizeof(n));
                if (!file || n > (1024ull * 1024ull))
                    return false;
                file.seekg((std::streamoff)n, std::ios::cur);
                if (!file)
                    return false;
                break;
            }
            case 0:
            case 1:
            case 7:
                file.seekg(1, std::ios::cur);
                break;
            case 2:
            case 3:
                file.seekg(2, std::ios::cur);
                break;
            case 4:
            case 5:
            case 6:
                file.seekg(4, std::ios::cur);
                break;
            case 9:  // array
            {
                uint32_t elem_type = 0;
                uint64_t count = 0;
                file.read(reinterpret_cast<char*>(&elem_type), sizeof(elem_type));
                file.read(reinterpret_cast<char*>(&count), sizeof(count));
                if (!file)
                    return false;
                size_t elem_size = 1;
                switch (elem_type)
                {
                    case 0:
                    case 1:
                    case 7:
                        elem_size = 1;
                        break;
                    case 2:
                    case 3:
                        elem_size = 2;
                        break;
                    case 4:
                    case 5:
                    case 6:
                        elem_size = 4;
                        break;
                    default:
                        return false;
                }
                const uint64_t bytes = count * (uint64_t)elem_size;
                file.seekg((std::streamoff)bytes, std::ios::cur);
                if (!file)
                    return false;
                break;
            }
            default:
                return false;
        }
    }

    return true;
}

bool NativeGGUFLoader::ParseTensorInfo()
{
    if (!file)
        return false;

    tensors.clear();
    tensors.reserve((size_t)tensorCount);

    for (uint64_t i = 0; i < tensorCount; ++i)
    {
        uint64_t name_len = 0;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        if (!file || name_len > (1024ull * 1024ull))
            return false;

        std::string name((size_t)name_len, '\0');
        if (name_len)
            file.read(&name[0], (std::streamsize)name_len);
        if (!file)
            return false;

        uint32_t n_dims = 0;
        file.read(reinterpret_cast<char*>(&n_dims), sizeof(n_dims));
        if (!file || n_dims > 64)
            return false;

        NativeGGUFTensorInfo ti;
        ti.name = std::move(name);
        ti.dimensions = n_dims;
        ti.shape.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d)
        {
            uint64_t v = 0;
            file.read(reinterpret_cast<char*>(&v), sizeof(v));
            if (!file)
                return false;
            ti.shape[(size_t)d] = v;
        }

        file.read(reinterpret_cast<char*>(&ti.type), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&ti.offset), sizeof(uint64_t));
        if (!file)
            return false;

        tensors.push_back(std::move(ti));
    }

    return true;
}

bool NativeGGUFLoader::LoadTensorData(const std::string&, std::vector<uint8_t>& data)
{
    data.clear();
    return false;
}

const uint8_t* NativeGGUFLoader::GetTensorDataPointer(const std::string&, uint64_t* sizeBytes) const
{
    if (sizeBytes)
        *sizeBytes = 0;
    return nullptr;
}

bool NativeGGUFLoader::IsMemoryMapped() const
{
    return mappedBase != nullptr;
}

uint64_t NativeGGUFLoader::GetMappedSize() const
{
    return mappedSize;
}

const std::vector<NativeGGUFTensorInfo>& NativeGGUFLoader::GetTensors() const
{
    return tensors;
}

const std::vector<NativeGGUFMetadata>& NativeGGUFLoader::GetMetadata() const
{
    return metadata;
}
